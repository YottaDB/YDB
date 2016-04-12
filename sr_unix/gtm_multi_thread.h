/****************************************************************
 *								*
 * Copyright (c) 2015 Fidelity National Information 		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_MULTI_THREAD_H
#define GTM_MULTI_THREAD_H

#include "gtm_pthread.h"

typedef	void *(*gtm_pthread_fnptr_t)(void *parm);

int	gtm_multi_thread(gtm_pthread_fnptr_t fnptr, int ntasks, int max_threads,
					pthread_t *thr_array, void **ret_array, void *parmarray, int parmElemSize);

GBLREF	boolean_t	multi_thread_in_use;		/* TRUE => threads are in use. FALSE => not in use */
GBLREF	boolean_t	thread_mutex_initialized;	/* TRUE => "thread_mutex" variable is initialized */
GBLREF	pthread_mutex_t	thread_mutex;			/* mutex structure used to ensure serialization amongst threads */
GBLREF	pthread_t	thread_mutex_holder;		/* pid/tid of the thread that has "thread_mutex" currently locked */
GBLREF	pthread_key_t	thread_gtm_putmsg_rname_key;	/* points to region name corresponding to each running thread */
GBLREF	boolean_t	thread_block_sigsent;		/* TRUE => block external signals SIGINT/SIGQUIT/SIGTERM/SIGTSTP/SIGCONT */
GBLREF	int		gtm_mupjnl_parallel;		/* Maximum # of concurrent threads or procs to use in "gtm_multi_thread" */
#ifdef DEBUG
GBLREF	boolean_t	in_nondeferrable_signal_handler;

# define	IN_GENERIC_SIGNAL_HANDLER	1
# define	IN_TIMER_HANDLER		2
#endif
GBLREF	boolean_t	forced_thread_exit;

error_def(ERR_SYSCALL);

/* Some pthread operations don't do well when interrupted by signals, so provide deferred interrupt wrappers for them. */

#define PTHREAD_COND_SIGNAL(COND, RVAL)						\
MBSTART {									\
	intrpt_state_t		prev_intrpt_state;				\
										\
	DEFER_INTERRUPTS(INTRPT_IN_PTHREAD_NB, prev_intrpt_state);		\
	(RVAL) = pthread_cond_signal(COND);					\
	ENABLE_INTERRUPTS(INTRPT_IN_PTHREAD_NB, prev_intrpt_state);		\
} MBEND

#ifdef GTM_PTHREAD

/* For use with gtm_multi_thread() */

typedef struct {
	int			ntasks;
	gtm_pthread_fnptr_t	fnptr;
	void			**ret_array;
	void			*parm_array;
	int			parmElemSize;
} thread_parm_t;

int	gtm_multi_thread_helper(thread_parm_t *tparm);

#define	IS_LIBPTHREAD_MUTEX_LOCK_HOLDER 	(pthread_self() == thread_mutex_holder)

#define	PTHREAD_MUTEX_LOCK_IF_NEEDED(WAS_HOLDER)								\
{														\
	int	rc;												\
														\
	if (multi_thread_in_use)										\
	{	/* gtm_malloc/gtm_free is not thread safe. So use locks to serialize */				\
		assert(thread_mutex_initialized);								\
		/* We should never use pthread_* calls inside a signal/timer handler. Assert that */		\
		assert(!in_nondeferrable_signal_handler);							\
		/* Allow for self to already own the lock (due to nested codepaths that need the lock. */	\
		if (!IS_LIBPTHREAD_MUTEX_LOCK_HOLDER)								\
		{												\
			rc = pthread_mutex_lock(&thread_mutex);							\
			if (rc)											\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,			\
						RTS_ERROR_LITERAL("pthread_mutex_lock()"), CALLFROM, rc);	\
			thread_mutex_holder = pthread_self();							\
			WAS_HOLDER = FALSE;									\
		} else												\
			WAS_HOLDER = TRUE;									\
	} else													\
		assert(!thread_mutex_holder);									\
}

#define	PTHREAD_MUTEX_UNLOCK_IF_NEEDED(WAS_HOLDER)							\
{													\
	int	rc;											\
													\
	if (multi_thread_in_use)									\
	{												\
		assert(thread_mutex_initialized);							\
		/* We should never use pthread_* calls inside a signal/timer handler. Assert that */	\
		assert(!in_nondeferrable_signal_handler);						\
		/* assert self does own the lock */							\
		assert(pthread_self() == thread_mutex_holder);						\
		if (!WAS_HOLDER)									\
		{											\
			thread_mutex_holder = 0;							\
			rc = pthread_mutex_unlock(&thread_mutex);					\
			if (rc)										\
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,		\
					RTS_ERROR_LITERAL("pthread_mutex_unlock()"), CALLFROM, rc);	\
		}											\
	} else												\
		assert(!thread_mutex_holder);								\
}

/* Below macro identifies if the caller is inside threaded code. A quick check of this is using the global
 * "multi_thread_in_use". If that is FALSE, we are guaranteed not to be in threaded code. If it is TRUE though, it is
 * still possible the caller is the master process (that spawned off the threads) and not the threads themselves.
 * The only way to distinguish between the master and the threads is to get the rname_key. It is NULL for the
 * master and non-NULL for the threads.
 * Note: In the future, if more threads are implemented with no region context, we might be better off checking
 * "gtm_main_thread_id" against "pthread_self()". Right now, the former is not maintained in all cases and is
 * hence not used.
 */
#define	INSIDE_THREADED_CODE(rname) (multi_thread_in_use && (NULL != (rname = pthread_getspecific(thread_gtm_putmsg_rname_key))))

#else

#define	IS_LIBPTHREAD_MUTEX_LOCK_HOLDER 		FALSE
#define	ASSERT_NO_THREAD_USAGE				assert(!multi_thread_in_use && !thread_mutex_holder)
#define	PTHREAD_MUTEX_LOCK_IF_NEEDED(WAS_HOLDER) 	ASSERT_NO_THREAD_USAGE
#define	PTHREAD_MUTEX_UNLOCK_IF_NEEDED(WAS_HOLDER)	ASSERT_NO_THREAD_USAGE
#define	INSIDE_THREADED_CODE(rname)			FALSE

#endif

/* Returns FALSE if threads are in use and we dont own the libpthread mutex lock. Returns TRUE otherwise */
#define	IS_PTHREAD_LOCKED_AND_HOLDER	(!multi_thread_in_use || IS_LIBPTHREAD_MUTEX_LOCK_HOLDER)

/* Below macro is invoked just before we read or write global variables that are also updated inside threaded code.
 * Global variables currently in this list (of those that are updated inside threaded code) are
 *	TREF(util_outbuff_ptr)
 *	TREF(util_outptr)
 * Any time any variable in the above list is read or written, the below macro needs to be added before the reference.
 */
#define	ASSERT_SAFE_TO_UPDATE_THREAD_GBLS	assert(IS_PTHREAD_LOCKED_AND_HOLDER)

/* Note: Below macro can be safely used even in threaded code since this variable only transitions from 0 to 1 inside threaded code.
 * Also, it is okay for other threads to read a stale value of this since they keep checking this at logical points in their
 * execution (using the PTHREAD_EXIT_IF_FORCED_EXIT macro). Reading a stale value will only delay thread exit a little.
 * The 1 to 0 transition happens only in non-threaded code. Assert accordingly.
 */
#define	SET_FORCED_THREAD_EXIT	forced_thread_exit = 1
#define	RESET_FORCED_THREAD_EXIT								\
{												\
	char			*rname;								\
												\
	assert(!INSIDE_THREADED_CODE(rname));							\
	forced_thread_exit = 0;									\
}

/* If a process with multiple threads receives a signal (e.g. SIGTERM) asking it to terminate,
 * the master process sets forced_exit to a non-zero value. But does not initiate exit handling
 * right then. Instead it goes back to what it was doing (most likely "pthread_join" inside
 * "gtm_multi_thread") and continues waiting for the threads to die. The threads are supposed to
 * check for "forced_exit" periodically (when they reach a logical stage in their processing) and
 * exit as soon as possible thereby letting the master process initiate exit handling processing.
 * The below macro helps with that.
 */
#define	PTHREAD_EXIT_IF_FORCED_EXIT				\
{								\
	char	*rname;						\
								\
	GTM_PTHREAD_ONLY(assert(INSIDE_THREADED_CODE(rname)));	\
	NON_GTM_PTHREAD_ONLY(assert(FALSE));			\
	if (forced_thread_exit)					\
		GTM_PTHREAD_EXIT(PTHREAD_CANCELED);		\
}

/* Exit the thread with status "STATUS". But before that release any mutex locks you hold (possible for example
 * if the exiting thread had done a "rts_error" call which would have grabbed the pthread_mutex_t lock but not
 * released it anywhere until thread exit time (which is here). The release is needed to prevent other threads
 * from deadlocking.
 */
#define	GTM_PTHREAD_EXIT(STATUS)										\
{														\
	char	*rname;												\
														\
	GTM_PTHREAD_ONLY(assert(INSIDE_THREADED_CODE(rname));)							\
	NON_GTM_PTHREAD_ONLY(assert(FALSE));									\
	/* If thread is exiting with abnormal status, signal other threads to stop execution			\
	 * at a logical point since the parent is anyways going to return with a non-zero exit status.		\
	 * This is necessary so only ONE thread modifies the condition-handler stack and other global variables	\
	 * as part of error-handling. If multiple threads encounter errors, the first thread will go		\
	 * through the condition handler scheme, the second thread onwards will exit in DRIVECH without		\
	 * going through any condition handler invocations.							\
	 */													\
	if (0 != STATUS)											\
		SET_FORCED_THREAD_EXIT;										\
	if (IS_LIBPTHREAD_MUTEX_LOCK_HOLDER)									\
		PTHREAD_MUTEX_UNLOCK_IF_NEEDED(FALSE);								\
	pthread_exit((void *)(INTPTR_T)STATUS);									\
}

#endif
