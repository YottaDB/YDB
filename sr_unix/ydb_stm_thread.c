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
	/* If we reach here, it means the MAIN worker thread has been asked to shut down (i.e. a "ydb_exit" was done).
	 * Do YottaDB exit processing too as part of the same but before that wait some time for the TP worker threads (if any)
	 * to terminate. "ydb_stm_thread_exit" takes care of that for us.
	 */
	ydb_stm_thread_exit();
	return NULL;
}

/* Function that does exit handling for the MAIN worker thread. */
void	ydb_stm_thread_exit(void)
{
	int		i, j, status;
	boolean_t	timed_out, n_timed_out;
	pthread_t	threadid;

	gtm_exit_handler(); /* rundown all open database resource */
	/* Now that the DBs are down, let's remove the pthread constructs created for SimpleThreadAPI
	 * usages. Note return codes ignored here as reporting errors creates more problems than they
	 * are worth.
	 */
	for (i = 1; (NULL != stmWorkQueue[i]) && (STMWORKQUEUEDIM > i); i++)
	{
		(void)pthread_cond_destroy(&stmWorkQueue[i]->cond);
		(void)pthread_mutex_destroy(&stmWorkQueue[i]->mutex);
	}
	/* TODO SEE: Also destroy the msems in the free queue blocks and release them if they exist */
	return;
}
