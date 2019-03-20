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
GBLREF	uint64_t 	stmTPToken;			/* Counter used to generate unique token for SimpleThreadAPI TP */
GBLREF	int		stapi_signal_handler_deferred;
GBLREF	pthread_mutex_t	ydb_engine_threadsafe_mutex[];
GBLREF	pthread_t	ydb_engine_threadsafe_mutex_holder[];
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
	int			i, sig_num, status, tLevel;
	pthread_t		mutex_holder_thread_id;
	enum sig_handler_t	sig_handler_type;
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
	/* NARSTODO: Fix the below infinite sleep loop */
	for ( i = 0; ; i++)
	{
		assert(ydb_engine_threadsafe_mutex_holder[0] != pthread_self());
		SLEEP_USEC(1, FALSE);	/* Sleep for 1 second; FALSE to indicate if system call is interrupted, do not
					 * restart the sleep.
					 */
		if (stapi_signal_handler_deferred)
		{	/* A signal handler was deferred. Try getting the YottaDB engine multi-thread mutex lock to
			 * see if we can invoke the signal handler in this thread. If we cannot get the lock, forward
			 * the signal to the thread that currently holds the ydb engine mutex lock in the hope that
			 * it can handle the signal right away. In any case, keep retrying this in a loop periodically
			 * so we avoid potential hangs due to indefinitely delaying handling of timer signals.
			 */
			/* NARSTODO: Handle non-zero return from "pthread_mutex_trylock" below */
			status = pthread_mutex_trylock(&ydb_engine_threadsafe_mutex[0]);
			assert((0 == status) || (EBUSY == status));
			if (0 == status)
			{
				ydb_engine_threadsafe_mutex_holder[0] = pthread_self();
				STAPI_INVOKE_DEFERRED_SIGNAL_HANDLER_IF_NEEDED;
				ydb_engine_threadsafe_mutex_holder[0] = 0;
				/* NARSTODO: Handle non-zero return from "pthread_mutex_unlock" below */
				pthread_mutex_unlock(&ydb_engine_threadsafe_mutex[0]);
			} else
			{
				SET_YDB_ENGINE_MUTEX_HOLDER_THREAD_ID(mutex_holder_thread_id, tLevel);
				assert(!pthread_equal(mutex_holder_thread_id, pthread_self()));
				if (mutex_holder_thread_id)
				{
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
	}
	return NULL;
}

/* Function that does exit handling in SimpleThreadAPI mode */
void	ydb_stm_thread_exit(void)
{
	gtm_exit_handler(); /* rundown all open database resource */
	return;
}
