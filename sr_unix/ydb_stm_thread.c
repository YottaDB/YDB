/****************************************************************
 *								*
 * Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	*
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
#include "sig_init.h"

GBLREF	boolean_t	simpleThreadAPI_active;
GBLREF	pthread_t	gtm_main_thread_id;
GBLREF	boolean_t	gtm_main_thread_id_set;
GBLREF	int		stapi_signal_handler_deferred;
GBLREF	pthread_mutex_t	ydb_engine_threadsafe_mutex[];
GBLREF	pthread_t	ydb_engine_threadsafe_mutex_holder[];
GBLREF	sig_pending	sigPendingQue;				/* Queue of pending signals in alternate signal handling mode */
GBLREF	boolean_t	exit_handler_active;
GBLREF	boolean_t	exit_handler_complete;
#ifdef YDB_USE_POSIX_TIMERS
GBLREF	pid_t		posix_timer_thread_id;
GBLREF	boolean_t	posix_timer_created;
#endif

/* Thread (formerly referred to as the MAIN worker thread) that is created when a SimpleThreadAPI call is done in the process.
 *
 * This thread has one of two purposes depending on the signal hanling mode in effect:
 *
 * 1. When running with a C or C-like main routine that lets YottaDB's signal handlers do the heavy lifting, the purpose
 *    of this thread is to be the only thread that receives any SIGALRM (timer signal) signals sent to this process by the
 *    kernel (using "posix_timer_thread_id" global variable which is later used in "sys_settimer" function). Towards that this
 *    thread is in a 1 second sleep loop that will be interrupted if a SIGALRM is received OR if the sleep times out
 *    and then handles the timer signal right away by invoking the "timer_handler" function IF it can get the
 *    YDB engine multi-thread lock. If it cannot get the lock, then it finds the thread that owns that lock currently and
 *    forwards the signal to that thread that way the "timer_handler" function is invoked in a timely fashion from a thread
 *    that can safely invoke it. In addition, if any signals other than SIGALRM also come in directed at this thread, they are
 *    also handled/forwarded in a similar fashion.
 *
 * 2. When running with a Go or similar main routine where we let the main handle the signals and pass them into YottaDB, this
 *    thread has a similar job. It needs to look for pending signal blocks queued on sigPendingQue and if it finds them, it
 *    should grab the YottaDB engine lock and process any signal(s) on the queue.
 *
 * Note there is no parameter or return value from this routine currently (both passed as NULL).
 * The routine signature (with "dummy_parm" as an input parameter) is dictated by this routine being driven by "pthread_create".
 */
void *ydb_stm_thread(void *dummy_parm)
{
	int			sig_num, status, tLevel;
	pthread_t		mutex_holder_thread_id;
	enum sig_handler_t	sig_handler_type;
	uint64_t		i;
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
	/* Note: This MAIN worker thread runs indefinitely till the process exits OR a "ydb_exit" OR a signal drives
	 * the exit handler hence the check for "exit_handler_active" below (set by the exit handler when it starts).
	 * Once the exit handler starts dismantling the engine allocations, our ability to handle signals is compromised
	 * so we should shut down. We check it twice - once before we sleep, an again before we sleep but before we try
	 * to drive any signal processing.
	 */
	for (i = 1; !exit_handler_active; i++)
	{
		assert(ydb_engine_threadsafe_mutex_holder[0] != pthread_self());
		SLEEP_USEC(MICROSECS_IN_SEC - 1, FALSE);	/* Sleep for 999,999 micro-seconds (i.e. almost 1 second).
								 * Second parameter is FALSE to indicate do not restart the sleep
								 * in case of signal interruption but instead check if signal can
								 * be handled/forwarded right away first (i.e. in a timely fashion).
								 */
		if (exit_handler_active)
			break;				/* If the exit handler is running or has run, then we're done */
		if (stapi_signal_handler_deferred || (USING_ALTERNATE_SIGHANDLING && SPQUE_NOT_EMPTY(&sigPendingQue, que)))
		{	/* A signal handler was deferred (or queued if using alternate signal handling). Try getting the YottaDB
			 * engine multi-thread mutex lock to see if we can invoke the signal handler in this thread. If we cannot
			 * get the lock, another thread will take care of it. In any case, keep retrying this in a loop periodically
			 * so we avoid potential hangs due to an indefinite delay in handling timer signals.
			 */
			DBGSIGHND_ONLY(fprintf(stderr, "ydb_stm_thread: Noted pending signal - trying to get engine lock\n");
				       fflush(stderr));
			SET_YDB_ENGINE_MUTEX_HOLDER_THREAD_ID(mutex_holder_thread_id, tLevel);
			if (!mutex_holder_thread_id)
			{	/* There is no current holder of the YDB engine multi-thread lock. Try to get it. */
				status = pthread_mutex_trylock(&ydb_engine_threadsafe_mutex[0]);
				assert((0 == status) || (EBUSY == status));
				/* If status is non-zero, we do not have the YottaDB engine lock so we cannot call
				 * "rts_error_csa" etc. therefore just silently continue to next iteration.
				 */
				if (0 == status)
				{	/* Got the YDB engine lock. Handle SIGALRM (and any other signals if necessary)
					 * using the STAPI_INVOKE_DEFERRED_SIGNAL_HANDLER_IF_NEEDED macro.
					 */
					assert(0 == ydb_engine_threadsafe_mutex_holder[0]);
					ydb_engine_threadsafe_mutex_holder[0] = pthread_self();
					if (!USING_ALTERNATE_SIGHANDLING)
					{
						STAPI_INVOKE_DEFERRED_SIGNAL_HANDLER_IF_NEEDED(OK_TO_NEST_TRUE);
					} else
					{	/* Process the pending signal(s) */
						assert(NULL != process_pending_signals_fptr);
						(*process_pending_signals_fptr)();
					}
					ydb_engine_threadsafe_mutex_holder[0] = 0;
					status = pthread_mutex_unlock(&ydb_engine_threadsafe_mutex[0]);
					/* If status is non-zero, we still have the YottaDB engine lock so we CAN call
					 * "rts_error_csa" etc. therefore do just that.
					 */
					if (status)
					{
						assert(FALSE);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
							RTS_ERROR_LITERAL("pthread_mutex_unlock()"), CALLFROM, status);
					}
				} else
				{	/* Else we got an EBUSY error in the "pthread_mutex_trylock" call. This means
					 * some other thread got the lock after we did the SET_YDB_ENGINE_MUTEX_HOLDER_THREAD_ID
					 * call above. We go back to sleep as when the engine lock is released, the signal
					 * should be noticed (we only need to care once).
					 */
					DBGSIGHND_ONLY(fprintf(stderr, "ydb_stm_thread: Could not get engine lock - sleeping (1)\n");
					       fflush(stderr));
				}
			} else
			{	/* There is a current holder of the YDB engine multi-thread lock.
				 * Note: It might not be safe to forward the signal to that thread as it is possible
				 * the thread that was holding the YDB engine lock a few instructions above could no
				 * longer be holding it when it receives the signal and could get confused (in the
				 * case of the YDBGo wrapper (prior to YDBGo#25, we saw SIG-11s in arbitary places
				 * in Go code because of this). Hence we do no signal forwarding.
				 */
				DBGSIGHND_ONLY(fprintf(stderr, "ydb_stm_thread: Could not get engine lock - sleeping (2)\n");
					       fflush(stderr));
			}
		}
	}
	gtm_main_thread_id_set = FALSE;		/* Indicate thread no longer active */
	return NULL;
}

/* Function to shutdown our signal handling thread prior as part of the YDB engine closing down. Note that this is called from
 * various places in generic_signal_handler() via its function pointer but in all cases, we have the engine lock so the call is
 *  properly protected.
 */
void	ydb_stm_thread_exit(void)
{
	pthread_t	sigThreadId;
	int		rc;

	/* This assert checks if either we are in TP OR we hold the engine lock. We cannot easily check the engine lock when
	 * a process is in TP because we don't have the current TP token (contains lock index) so half a check is better than none.
	 */
	assert((0 < dollar_tlevel) || (pthread_equal(ydb_engine_threadsafe_mutex_holder[0], pthread_self())));
	sigThreadId = gtm_main_thread_id;
	if (gtm_main_thread_id_set)				/* Only do this if it has not already shut itself down */
	{
		if (pthread_equal(sigThreadId, pthread_self()))
		{	/* If the signal thread is us, just return as we'll stop with the rest of them. This can happen if a fatal
			 * signal was processed by the signal thread itself.
			 */
			return;
		}
		rc = pthread_kill(sigThreadId, YDBSIGNOTIFY);	/* Wakeup signal thread to shut it down */
		assert(0 == rc);				/* This might cause issues if it goes off but at least we know */
		rc = pthread_join(sigThreadId, NULL);		/* Wait for thread to shutdown */
		assert(0 == rc);
	}
	return;
}
