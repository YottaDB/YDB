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

#include "mdef.h"

#include "gtm_signal.h"		/* for SIGPROCMASK */
#include "gtm_string.h"
#include "gtm_pthread.h"

#include <errno.h>

#include "gtm_multi_thread.h"
#include "iosp.h"		/* for SS_NORMAL */
#include "have_crit.h"		/* for DEFERRED_EXIT_HANDLING_CHECK */
#include "gtmimagename.h"
#ifdef DEBUG
#include "wbox_test_init.h"
#endif
#include "gtmmsg.h"		/* for gtm_putmsg_csa prototype */

GBLREF	pthread_t	gtm_main_thread_id;
GBLREF	boolean_t	gtm_main_thread_id_set;
GBLREF	boolean_t	multi_thread_in_use;			/* TRUE => threads are in use. FALSE => not in use */
GBLREF	boolean_t	thread_mutex_initialized;	/* TRUE => "thread_mutex" variable is initialized */
GBLREF	pthread_mutex_t	thread_mutex;			/* mutex structure used to ensure serialization amongst threads */
GBLREF	boolean_t	blocksig_initialized;
GBLREF  sigset_t	block_sigsent;
GBLREF	int		next_task_index;		/* "next" task index waiting for a thread to be assigned */

error_def(ERR_SYSCALL);

/* This function invokes "fnptr" with the argument "&parm_array[i]" where "i" ranges from 0 thru "ntasks - 1".
 * At most "max_threads" threads will run at any given point in time with two special exceptions.
 *	max_threads = 0 implies one thread runs per region.
 *	max_threads = 1 implies no threads are used.
 * If threads are used, this function uses pthread_t structure from "thr_array[i]" to create the needed threads.
 * Elements of "parm_array[]" are specific to the thread, so access to them does not need to be protected by a mutex,
 *   (unless they contain pointers to shared data, in which case some protection may be necessary.)
 * On platforms where GTM_PTHREAD is not defined (currently HPUX IA64), this function does not use pthreads but
 *   instead serially executes the function "fnptr" for each of the "parm_array[i]" parameters available.
 *   Effectively assumes "max_threads == 1" even if set to a different value at function entry.
 *
 * Returns 0 (SS_NORMAL) if "ntasks" tasks were successfully created and completed (with or without threads).
 *	"ret_array[]" contains individual thread exit status in this case.
 * Returns non-zero otherwise. In this case, it waits for all/any created threads to die down before returning.
 *	Also, "ret_array[]" is filled with return status of each task invocation as appropriate.
 *	Caller needs to look at the function return value and "ret_array[]" and issue appropriate error messages.
 */
int	gtm_multi_thread(gtm_pthread_fnptr_t fnptr, int ntasks, int max_threads,
					pthread_t *thr_array, void **ret_array, void *parm_array, int parmElemSize)
{
	int		final_ret, rc, rc1, error_line;
	void		**ret_ptr, *ret;
	pthread_t	*thr_ptr, *thr_start, *thr_top;
	uchar_ptr_t	parm_ptr;
	pthread_attr_t	attr;
	sigset_t	savemask;
#	ifdef GTM_PTHREAD
	thread_parm_t	tparm;
#	endif

	assert(!multi_thread_in_use);
	assert(0 < ntasks);
#	ifdef GTM_PTHREAD
	if (!max_threads || (max_threads > ntasks))
		max_threads = ntasks;
#	else
	max_threads = 1;	/* do not use threads on thread-unsupported platform */
#	endif
	thr_start = &thr_array[0];
	thr_top = thr_start + ntasks;
	parm_ptr = (uchar_ptr_t)parm_array;
	ret_ptr = &ret_array[0];
	memset(&ret_array[0], 0, SIZEOF(void *) * ntasks);	/* initialize return status to SS_NORMAL/0 */
	final_ret = 0;
	if (1 == max_threads)
	{	/* Simplest case. No thread usage. Finish and return */
		assert(0 == SS_NORMAL);
		for (thr_ptr = &thr_array[0]; thr_ptr < thr_top; thr_ptr++, parm_ptr += parmElemSize, ret_ptr++)
		{
			if (!final_ret)
			{
				rc1 = (INTPTR_T)(*fnptr)(parm_ptr);
				if (rc1)
					final_ret = rc1;
			} else
				rc1 = 0;
			*ret_ptr = (void *)(INTPTR_T)rc1;
		}
		return final_ret;
	}
#	ifdef GTM_PTHREAD
	/* Initialize thread-mutex variables if not already done */
	if (!thread_mutex_initialized)
	{
		rc = pthread_mutex_init(&thread_mutex, NULL);
		if (rc)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8)
					ERR_SYSCALL, 5, RTS_ERROR_LITERAL("pthread_mutex_init()"), CALLFROM, rc);
		thread_mutex_initialized = TRUE;
	}
	/* Initialize and set thread-is-joinable attribute */
	rc = pthread_attr_init(&attr);
	if (rc)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8)
				ERR_SYSCALL, 5, RTS_ERROR_LITERAL("pthread_attr_init()"), CALLFROM, rc);
	rc = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if (rc)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8)
				ERR_SYSCALL, 5, RTS_ERROR_LITERAL("pthread_attr_setdetachstate()"), CALLFROM, rc);
	RESET_FORCED_THREAD_EXIT;	/* reset "forced_thread_exit" from a previous call to "gtm_multi_thread" */
	multi_thread_in_use = TRUE;
	/* Temporarily block external signals. That way the threads we create inherit a signal-mask that has those
	 * signals blocked. This way any SIG-15 (for example) that gets sent while threads are running gets sent
	 * to the current process and not any of the threads that it is about to create.
	 * In addition, the pthread_* functions are not async-signal-safe so it is better to block those signals
	 * when we use those functions below.
	 */
	assert(blocksig_initialized);
	SIGPROCMASK(SIG_BLOCK, &block_sigsent, &savemask, rc);
	DEBUG_ONLY(error_line = 0;)
	tparm.ntasks = ntasks;
	tparm.fnptr = fnptr;
	tparm.ret_array = ret_array;
	tparm.parm_array = parm_array;
	tparm.parmElemSize = parmElemSize;
	thr_top = thr_start + max_threads;
	next_task_index = 0;
	for (thr_ptr = thr_start; thr_ptr < thr_top; thr_ptr++)
	{
		rc1 = pthread_create(thr_ptr, &attr, (gtm_pthread_fnptr_t)&gtm_multi_thread_helper, (void *)&tparm);
		if (rc1)
		{	/* Thread creation failed */
			DEBUG_ONLY(if (!error_line) error_line = __LINE__);
			SIGPROCMASK(SIG_SETMASK, &savemask, NULL, rc);
			if ((0 != rc1) && !IS_GTM_IMAGE)	/* Display error if mupip/dse etc. but not for mumps */
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8)
						ERR_SYSCALL, 5, RTS_ERROR_LITERAL("pthread_create()"), CALLFROM, rc1);
			}
			final_ret = rc1;
			/* Stop the already started threads */
			SET_FORCED_THREAD_EXIT;	/* this signals spawned-off threads to stop execution at a logical point */
			break;
		}
	}
	if (!rc1)
		SIGPROCMASK(SIG_SETMASK, &savemask, NULL, rc);
	/* Wait for started threads to complete */
	for ( ; thr_start < thr_ptr; thr_start++)
	{
		rc1 = pthread_join(*thr_start, &ret);
		if (rc1)
		{
			DEBUG_ONLY(if (!error_line) error_line = __LINE__);
			if (!final_ret)
				final_ret = rc1;
		} else
		{
			ret;	/* "ret" would have been set by "pthread_join" */
			if (ret && !final_ret)
			{
				DEBUG_ONLY(if (!error_line) error_line = __LINE__;)
				final_ret = (INTPTR_T)ret;
			}
		}
	}
	multi_thread_in_use = FALSE;
	DEFERRED_EXIT_HANDLING_CHECK; /* Now that all threads have terminated, check for need of deferred signal/exit handling */
	rc = pthread_attr_destroy(&attr);	/* Free attribute */
	if (rc)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8)
				ERR_SYSCALL, 5, RTS_ERROR_LITERAL("pthread_attr_destroy()"), CALLFROM, rc);
	/* Now that there are no thread usages in this process, ideally we should be doing the following.
	 *	pthread_mutex_destroy(&thread_mutex);
	 * But it is possible the process uses threads again (e.g. journal rollback uses threads in various stages of the process).
	 * We dont want to be initializing and destroying the mutex everytime. So we avoid this step.
	 */
#	endif
	return final_ret;
}

#ifdef GTM_PTHREAD
int	gtm_multi_thread_helper(thread_parm_t *tparm)
{
	boolean_t		was_holder;
	gtm_pthread_fnptr_t	fnptr;
	int			ntasks, nexttask;
	int			parmElemSize, rc1;
	pthread_t		*thr_array;
	uchar_ptr_t		parm_ptr;
	void			**ret_array;
	void			*parm_array;
	void			**ret_ptr;

	ntasks = tparm->ntasks;
	fnptr = tparm->fnptr;
	ret_array = tparm->ret_array;
	parm_array = tparm->parm_array;
	parmElemSize = tparm->parmElemSize;
	rc1 = SS_NORMAL;
	while (TRUE)
	{
		PTHREAD_MUTEX_LOCK_IF_NEEDED(was_holder);
		assert(!was_holder);
		nexttask = next_task_index;
		if (nexttask < ntasks)
			next_task_index++;
		PTHREAD_MUTEX_UNLOCK_IF_NEEDED(was_holder);
		if (nexttask >= ntasks)
			break;
		parm_ptr = (uchar_ptr_t)parm_array + (parmElemSize * nexttask);
		ret_ptr = &ret_array[nexttask];
		/* The below invocation of *fnptr() can exit this thread without returning back to us
		 * (e.g. if PTHREAD_EXIT_IF_FORCED_EXIT is invoked etc.). In that case, we want the return
		 * value to reflect the correct value. Initialize accordingly.
		 */
		*ret_ptr = PTHREAD_CANCELED;
		rc1 = (INTPTR_T)(*fnptr)(parm_ptr);
		*ret_ptr = (void *)(INTPTR_T)rc1;
		if (SS_NORMAL != rc1)
		{	/* Stop the already running threads */
			SET_FORCED_THREAD_EXIT;	/* this signals spawned-off threads to stop execution at a logical point */
			break;
		}
		nexttask++;
	}
	return rc1;
}
#endif
