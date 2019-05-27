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
#include "sig_init.h"

GBLREF	boolean_t	simpleThreadAPI_active;
GBLREF	pthread_t	gtm_main_thread_id;
GBLREF	boolean_t	gtm_main_thread_id_set;
GBLREF	int		stapi_signal_handler_deferred;
GBLREF	pthread_mutex_t	ydb_engine_threadsafe_mutex[];
GBLREF	pthread_t	ydb_engine_threadsafe_mutex_holder[];
#ifdef YDB_USE_POSIX_TIMERS
GBLREF	pid_t		posix_timer_thread_id;
GBLREF	boolean_t	posix_timer_created;
#endif

/* Thread (formerly referred to as the MAIN worker thread) that is created when a SimpleThreadAPI call is done in the process.
 *
 * This thread's purpose is to be the only thread that receives any SIGALRM (timer signal) signals sent to this process by the
 * kernel (using "posix_timer_thread_id" global variable which is later used in "sys_settimer" function). Towards that this
 * thread is in a 1 second sleep loop that will be interrupted if a SIGALRM is received OR if the sleep times out
 * and then handles the timer signal right away by invoking the "timer_handler" function IF it can get the
 * YDB engine multi-thread lock. If it cannot get the lock, then it finds the thread that owns that lock currently and
 * forwards the signal to that thread that way the "timer_handler" function is invoked in a timely fashion from a thread
 * that can safely invoke it. In addition, if any signals other than SIGALRM also come in directed at this thread, they are
 * also handled/forwarded in a similar fashion.
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
	/* Note: This MAIN worker thread runs indefinitely till the process exits OR
	 * a "ydb_exit" is done hence the check for "ydb_init_complete" below.
	 */
	for (i = 1; ydb_init_complete; i++)
	{
		assert(ydb_engine_threadsafe_mutex_holder[0] != pthread_self());
		SLEEP_USEC(MICROSECS_IN_SEC - 1, FALSE);	/* Sleep for 999,999 micro-seconds (i.e. almost 1 second).
								 * Second parameter is FALSE to indicate do not restart the sleep
								 * in case of signal interruption but instead check if signal can
								 * be handled/forwarded right away first (i.e. in a timely fashion).
								 */
		if (stapi_signal_handler_deferred)
		{	/* A signal handler was deferred. Try getting the YottaDB engine multi-thread mutex lock to
			 * see if we can invoke the signal handler in this thread. If we cannot get the lock, forward
			 * the signal to the thread that currently holds the ydb engine mutex lock in the hope that
			 * it can handle the signal right away. In any case, keep retrying this in a loop periodically
			 * so we avoid potential hangs due to indefinitely delaying handling of timer signals.
			 */
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
					STAPI_INVOKE_DEFERRED_SIGNAL_HANDLER_IF_NEEDED;
					ydb_engine_threadsafe_mutex_holder[0] = 0;
					status = pthread_mutex_unlock(&ydb_engine_threadsafe_mutex[0]);
					/* If status is non-zero, we do have the YottaDB engine lock so we CAN call
					 * "rts_error_csa" etc. therefore do just that.
					 */
					if (status)
					{
						assert(FALSE);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
							RTS_ERROR_LITERAL("pthread_mutex_unlock()"), CALLFROM, status);
					}
				}
				/* else: we got an EBUSY was an error in the "pthread_mutex_trylock" call. This means
				 * some other thread got the lock after we did the SET_YDB_ENGINE_MUTEX_HOLDER_THREAD_ID
				 * call above. Sleep for a bit and try again.
				 */
			} else
			{	/* There is a current holder of the YDB engine multi-thread lock. Forward signal to that thread. */
				for (sig_handler_type = 0; sig_handler_type < sig_hndlr_num_entries; sig_handler_type++)
				{
					if (!STAPI_IS_SIGNAL_HANDLER_DEFERRED(sig_handler_type))
						continue;
					sig_num = stapi_signal_handler_oscontext[sig_handler_type].sig_num;
					if (sig_num)
						pthread_kill(mutex_holder_thread_id, sig_num);
				}
			}
		}
	}
	return NULL;
}

/* Function that does exit handling in SimpleThreadAPI mode */
void	ydb_stm_thread_exit(void)
{
	gtm_exit_handler(); /* rundown all open database resource */
	return;
}
