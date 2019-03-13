/****************************************************************
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#ifdef YDB_USE_POSIX_TIMERS
#include <sys/syscall.h>	/* for "syscall" */
#endif

#include "gtm_semaphore.h"

#include "libyottadb_int.h"
#include "mdq.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab_int4.h"     /* needed for tp.h */
#include "buddy_list.h"
#include "tp.h"
#include "gtm_multi_thread.h"
#include "caller_id.h"
#include "gtmci.h"
#include "gtm_exit_handler.h"
#include "memcoherency.h"

GBLREF	stm_workq	*stmWorkQueue[];
GBLREF	boolean_t	simpleThreadAPI_active;
GBLREF	pthread_t	gtm_main_thread_id;
GBLREF	boolean_t	gtm_main_thread_id_set;
GBLREF	uint64_t 	stmTPToken;			/* Counter used to generate unique token for SimpleThreadAPI TP */
GBLREF	boolean_t	forced_simplethreadapi_exit;
#ifdef YDB_USE_POSIX_TIMERS
GBLREF	pid_t		posix_timer_thread_id;
GBLREF	boolean_t	posix_timer_created;
#endif

/* Routine to manage worker thread(s) for the *_st() interface routines (Simple Thread API aka the
 * Simple Thread Method). Note for the time being, only one worker thread is created. In the future,
 * if/when YottaDB becomes fully-threaded, more worker threads may be allowed.
 *
 * Note there is no parameter or return value from this routine currently (both passed as NULL). The
 * routine signature is dictated by this routine being driven by pthread_create().
 */
void *ydb_stm_thread(void *parm)
{
	int		status;
	boolean_t	queueChanged;
	boolean_t	forced_simplethreadapi_exit_seen = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Now that we are establishing this main work queue thread, we need to make sure all timers and checks done by
	 * YottaDB *and* user code deal with THIS thread and not some other random thread.
	 */
	assert(gtm_main_thread_id_set);
	assert(!simpleThreadAPI_active);
	gtm_main_thread_id = pthread_self();
	INITIALIZE_THREAD_MUTEX_IF_NEEDED; /* Initialize thread-mutex variables if not already done */
#	ifdef YDB_USE_POSIX_TIMERS
	assert(0 == posix_timer_created);
	assert(0 == posix_timer_thread_id);
	posix_timer_thread_id = syscall(SYS_gettid);
#	endif
	SHM_WRITE_MEMORY_BARRIER;
	simpleThreadAPI_active = TRUE;	/* to indicate to caller/creator thread that we are done with setup */
	/* Initialize which queue we are looking for work in */
	TREF(curWorkQHead) = stmWorkQueue[0];			/* Initially pick requests from main work queue */
	assert(NULL != TREF(curWorkQHead));			/* Queue should be setup by now */
	/* Must have mutex locked before we start waiting */
	status = pthread_mutex_lock(&(TREF(curWorkQHead))->mutex);
	if (0 != status)
	{
		SETUP_SYSCALL_ERROR("pthread_mutex_lock(curWorkQHead)", status);
		assertpro(FALSE && YDB_ERR_SYSCALL);			/* No return possible so abandon thread */
	}
	for ( ; ; )
	{	/* Before we wait, verify nobody snuck something onto the queue by processing anything there */
		do
		{
			queueChanged = FALSE;
			/* If "forced_thread_exit" has been set to TRUE, but "forced_simplethreadapi_exit" has not yet
			 * been set to TRUE, check to see if now is a logical point to set the latter.
			 */
			if (forced_thread_exit && !forced_simplethreadapi_exit && OK_TO_INTERRUPT)
				forced_simplethreadapi_exit = TRUE;
			if (forced_simplethreadapi_exit_seen)
			{	/* We saw "forced_simplethreadapi_exit" to be TRUE while inside "ydb_stm_threadq_dispatch".
				 * Any new requests that are queued (by "ydb_stm_args") AFTER this point on will need to lock
				 * the queue header mutex at which point we set "callblkServiceNeeded" (in ydb_stm_args.c) to TRUE
				 * to not queue such requests. Therefore the worker thread can safely exit at this point without
				 * any risk of having unserviced requests. But before that check if we are servicing a TP queue,
				 * if so bubble back down the work queues at various TP depth until we reach the main work queue
				 * before exiting.
				 */
				assert((TREF(curWorkQHead))->stm_wqhead.que.fl == &(TREF(curWorkQHead))->stm_wqhead);
				assert(!queueChanged);
				if (TREF(curWorkQHead) != stmWorkQueue[0])
				{
					TREF(curWorkQHead) = (TREF(curWorkQHead))->prevWorkQHead;
					queueChanged = TRUE;
				} else
					break;	/* Reached the main work queue and no work to do and signaled to exit. So exit. */
			}
		} while (queueChanged);
		if (forced_simplethreadapi_exit_seen)
			break;
		/* Wait for some work to probably show up */
		status = pthread_cond_wait(&(TREF(curWorkQHead))->cond, &(TREF(curWorkQHead))->mutex);
		if (0 != status)
		{
			SETUP_SYSCALL_ERROR("pthread_cond_wait(curWorkQHead)", status);
			assertpro(FALSE && YDB_ERR_SYSCALL);		/* No return possible so abandon thread */
		}
	}
	/* If we reach here, it means the MAIN worker thread has been asked to shut down (i.e. a "ydb_exit" was done).
	 * Do YottaDB exit processing too as part of the same but before that wait some time for the TP worker threads (if any)
	 * to terminate. "ydb_stm_thread_exit" takes care of that for us.
	 */
	ydb_stm_thread_exit();
	return NULL;
}

/* Function that does exit handling for the MAIN worker thread. Includes wait for TP worker threads to terminate
 * before invoking YottaDB exit handler. See function body for more comments.
 */
void	ydb_stm_thread_exit(void)
{
	int		i, j, status;
	boolean_t	timed_wait, timed_out, n_timed_out;
	pthread_t	threadid;

	/* If "forced_simplethreadapi_exit" is TRUE when we enter this function, it means both MAIN and TP worker threads
	 *   have reached a logical state and they can terminate right away. Therefore it is okay to wait indefinitely for the
	 *   TP worker threads to terminate.
	 * Else, it is possible the TP worker threads are hung waiting for some request to be serviced by the MAIN worker thread
	 *   and so waiting indefinitely for the TP worker thread in the MAIN worker thread would lead to a deadlock.
	 *   Therefore in this case, wait only for a finite time period before giving up and proceeding to exit handling.
	 *   If the TP worker thread is hung, it is okay for the MAIN worker thread to continue exit handling without risk of
	 *   any issues.
	 */
	timed_wait = !forced_simplethreadapi_exit;
	n_timed_out = 0;
	/* Signal TP worker threads to exit at a logical point if they are not hung waiting for the MAIN worker thread */
	forced_simplethreadapi_exit = TRUE;
	/* TP worker thread(s) would have also seen the signal to exit ("forced_simplethreadapi_exit").
	 * Wait for them to terminate.
	 */
	for (i = 1; (STMWORKQUEUEDIM > i); i++)
	{
		if (NULL == stmWorkQueue[i])
			break;
		threadid = stmWorkQueue[i]->threadid;
		if (0 != threadid)
		{	/* See comments in similar code in the function "ydb_exit" (search for "pthread_tryjoin_np")
			 * for why the for loop below is necessary.
			 */
			timed_out = FALSE;
			for (j = 0; ; j++)
			{
				status = pthread_cond_signal(&stmWorkQueue[i]->cond);
				assert(0 == status);
				status = pthread_tryjoin_np(threadid, NULL);
				if (EBUSY != status)
				{
					assert(0 == status);
					break;
				}
				SLEEP_USEC(1, FALSE);	/* sleep for 1 micro-second before retrying */
				if (timed_wait && (MICROSECS_IN_MSEC < j))
				{
					timed_out = TRUE;
					n_timed_out++;
					break;
				}
			}
			if (!timed_out)
				stmWorkQueue[i]->threadid = 0;
		}
	}
	gtm_exit_handler(); /* rundown all open database resource */
	/* Now that the DBs are down, let's remove the pthread constructs created for SimpleThreadAPI
	 * usages. Note return codes ignored here as reporting errors creates more problems than they
	 * are worth. Note the three situations this loop cleans up are:
	 *   1. No work thread was ever created but we do have the work queue mutex and condition var
	 *      in index 0 created by gtm_startup() that need cleaning up.
	 *   2. We have created a worker thread in [0] but no TP levels.
	 *   3. We have created TP levels with threads, queues, and mutex/condvars.
	 * It is only [0] where we can have initialized mutex and condvar that need cleaning up (because
	 * they were created in gtm_startup()) but if never used, we have no active thread to clean up.
	 * Note that we clean up everything except stmWorkQueue[0]->cond and stmWorkQueue[0]->mutex.
	 * These will be cleaned up by the thread
	 * that invokes "ydb_exit" (which will do that once it sees that the MAIN worker thread has
	 * terminated). Until then that thread could be using this cond/mutex for signaling us
	 * (the MAIN worker thread) and so we should not destroy it.
	 */
	if (!n_timed_out)
	{
		for (i = 1; (NULL != stmWorkQueue[i]) && (STMWORKQUEUEDIM > i); i++)
		{
			(void)pthread_cond_destroy(&stmWorkQueue[i]->cond);
			(void)pthread_mutex_destroy(&stmWorkQueue[i]->mutex);
		}
	}
	/* TODO SEE: Also destroy the msems in the free queue blocks and release them if they exist */
	return;
}
