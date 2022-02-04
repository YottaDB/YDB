/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef SIG_INIT_H_INCLUDED
#define SIG_INIT_H_INCLUDED

#include "gtm_string.h"
#include "gtm_semaphore.h"

#include "memcoherency.h"
#include "libyottadb.h"
#include "generic_signal_handler.h"
#include "alternate_sighandling.h"
#include "interlock.h"

/* Below signal handler function types are used as the first parameter to FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED */
enum sig_handler_t
{
	sig_hndlr_none,
	sig_hndlr_continue_handler,
	sig_hndlr_ctrlc_handler,
	sig_hndlr_dbcertify_signal_handler,
	sig_hndlr_generic_signal_handler,
	sig_hndlr_jobexam_signal_handler,
	sig_hndlr_jobinterrupt_event,
	sig_hndlr_job_term_handler,
	sig_hndlr_op_fnzpeek_signal_handler,
	sig_hndlr_suspsigs_handler,
	sig_hndlr_timer_handler,
	sig_hndlr_num_entries
};

/* Below is the structure that records whether a signal handler invocation from the OS was forwarded/deferred.
 * In that case "siginfo" and "sigcontext" record the "info" and "context" passed into the signal handler by the OS.
 * This way we do not lose the original information (e.g. if another process-id sent us the signal, forwarding the
 * signal from one thread to another thread in the same process causes the signal to be treated as having originated in the
 * same process and thus loses the sending pid information). For example, sending SIGQUIT/SIG-3 should show up as KILLBYSIGUINFO
 * but would show up as KILLBYSIGSINFO1 without the pre-forwarding store.
 */
typedef struct
{
	int			sig_num;	/* Signal number */
	boolean_t		sig_forwarded;	/* Whether signal "sig_num" got forwarded */
	siginfo_t		sig_info;	/* "info" from OS signal handler invocation */
	gtm_sigcontext_t	sig_context;	/* "context" from OS signal handler invocation */
} sig_info_context_t;

GBLREF	global_latch_t		stapi_signal_handler_deferred;
GBLREF	sig_info_context_t	stapi_signal_handler_oscontext[sig_hndlr_num_entries];
GBLREF	void			(*ydb_stm_thread_exit_fnptr)(void);

#ifdef DEBUG
/* This macro is used by a few places that invoke "timer_handler" with DUMMY_SIG_NUM (e.g. "check_for_timer_pops")
 * to show as if the SIGALRM signal was forwarded/deferred even though it is not. This is to avoid an assert otherwise
 * in the STAPI_CLEAR_SIGNAL_HANDLER_DEFERRED from failing inside the "timer_handler" function when it invokes the
 * FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED macro.
 */
#define	STAPI_FAKE_TIMER_HANDLER_WAS_DEFERRED							\
{												\
	BIT_SET_INTERLOCKED(stapi_signal_handler_deferred, (1 << sig_hndlr_timer_handler));	\
	stapi_signal_handler_oscontext[sig_hndlr_timer_handler].sig_forwarded = TRUE;		\
}
#endif

#define	STAPI_IS_SIGNAL_HANDLER_DEFERRED(SIGHNDLRTYPE)	(BIT_GET_INTERLOCKED(stapi_signal_handler_deferred) & (1 << SIGHNDLRTYPE))

/* The below macro uses interlock instructions since multiple threads could potentially invoke this at the same time
 * (e.g. if SIGALRM and SIGTERM happen at same time, OR if multiple SIGCONT signals happen at the same time etc.)
 * We cannot use INTERLOCK_ADD since we could double-increment in that case. Therefore, we use COMPSWAP_LOCK which
 * checks the current value and atomically does the increment only if no other concurrent change happened to this
 * global variable value from another thread. This macro is similar to the SET_DEFERRED_CONDITION macro in that regard.
 */
#define	BIT_SET_INTERLOCKED(LATCH, BITVAL)								\
{													\
	int4		oldvalue, newvalue, absolute_bitval;						\
	boolean_t	clear_condition;								\
													\
	/* Note that BITVAL can be negative to indicate the bit has to be CLEARed and positive to	\
	 * indicate the bit has to be SET. In that case, we use the sign to first know whether it	\
	 * is a SET or CLEAR and then use the absolute value (stored in "absolute_bitval" local		\
	 * variable to check if the bit is already set or not).						\
	 */												\
	if (0 > BITVAL)											\
	{	/* This is a call from the CLEAR_DEFERRED_CONDITION macro. Invert sense. */		\
		clear_condition = TRUE;									\
		absolute_bitval = -BITVAL;								\
	} else												\
	{												\
		clear_condition = FALSE;								\
		absolute_bitval = BITVAL;								\
	}												\
	assert(0 < absolute_bitval);									\
	for ( ; ; )											\
	{												\
		oldvalue = GLOBAL_LATCH_VALUE(&LATCH);							\
		if (!clear_condition)									\
		{											\
			if (oldvalue & absolute_bitval)							\
			{	/* Deferred condition is already set. No more action needed. */		\
				break;									\
			}										\
		} else											\
		{											\
			if (!(oldvalue & absolute_bitval))						\
			{	/* Deferred condition is already cleared. No more action needed. */	\
				break;									\
			}										\
		}											\
		newvalue = oldvalue + BITVAL;	/* Note: BITVAL can be positive or negative */		\
		assert(0 <= newvalue);									\
		if (COMPSWAP_LOCK(&LATCH, oldvalue, newvalue))						\
			break;										\
		/* If we are here, it means the latch value changed concurrently since we took a copy.	\
		 * If LATCH parameter is "deferred_signal_handling_needed", this is possible for	\
		 * example if a timer interrupt occurs and the LATCH variable gets modified inside the	\
		 * interrupt code. In that case, redo the deferred condition set based on the current	\
		 * global variable. Same reasoning applies for any LATCH parameter.			\
		 */											\
	}												\
	assert(0 <= GLOBAL_LATCH_VALUE(&LATCH));							\
}

/* The below macro gets the latch value. It does not do any interlocked operations but is named BIT_GET_INTERLOCKED
 * to be consistent with the related BIT_SET_INTERLOCKED macro.
 */
#define	BIT_GET_INTERLOCKED(LATCH) (DBG_ASSERT(0 <= GLOBAL_LATCH_VALUE(&LATCH))	GLOBAL_LATCH_VALUE(&LATCH))

/* The below macro needs to invoke BIT_SET_INTERLOCKED since multliple threads can invoke it at the same time.
 */
#define	STAPI_SET_SIGNAL_HANDLER_DEFERRED(SIGHNDLRTYPE, SIG_NUM, INFO, CONTEXT)		\
{											\
	SAVE_OS_SIGNAL_HANDLER_SIGNUM(SIGHNDLRTYPE, SIG_NUM);				\
	SAVE_OS_SIGNAL_HANDLER_INFO(SIGHNDLRTYPE, INFO);				\
	SAVE_OS_SIGNAL_HANDLER_CONTEXT(SIGHNDLRTYPE, CONTEXT);				\
	SHM_WRITE_MEMORY_BARRIER;							\
	stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_forwarded = TRUE;		\
	SHM_WRITE_MEMORY_BARRIER;							\
	/* Need BIT_SET_INTERLOCKED below since multiple threads can execute this code	\
	 * at the same time (e.g. if SIGALRM and SIGTERM happen at same time,		\
	 * OR if multiple SIGCONT signals happen at the same time etc.).		\
	 */										\
	BIT_SET_INTERLOCKED(stapi_signal_handler_deferred, (1 << SIGHNDLRTYPE));	\
	SHM_WRITE_MEMORY_BARRIER;							\
	SET_DEFERRED_STAPI_CHECK_NEEDED;						\
}

#define	STAPI_CLEAR_SIGNAL_HANDLER_DEFERRED(SIGHNDLRTYPE)				\
{											\
	/* Need BIT_SET_INTERLOCKED below since multiple threads can execute this code	\
	 * at the same time (e.g. if SIGALRM and SIGTERM happen at same time,		\
	 * OR if multiple SIGCONT signals happen at the same time etc.).		\
	 */										\
	BIT_SET_INTERLOCKED(stapi_signal_handler_deferred, -(1 << SIGHNDLRTYPE));	\
	SHM_WRITE_MEMORY_BARRIER;						\
	stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_forwarded = FALSE;	\
	SHM_WRITE_MEMORY_BARRIER;						\
}

#define	OK_TO_NEST_FALSE	FALSE
#define	OK_TO_NEST_TRUE		TRUE

#define	STAPI_INVOKE_DEFERRED_SIGNAL_HANDLER_IF_NEEDED(OK_TO_NEST)								\
{																\
	GBLREF	void			(*ydb_stm_invoke_deferred_signal_handler_fnptr)(void);					\
	GBLREF	enum sig_handler_t	ydb_stm_invoke_deferred_signal_handler_type;						\
																\
	enum sig_handler_t		lcl_stm_sighndlr_type;									\
																\
	if (BIT_GET_INTERLOCKED(stapi_signal_handler_deferred) && (NULL != ydb_stm_invoke_deferred_signal_handler_fnptr))	\
	{	/* It is possible that "ydb_stm_invoke_deferred_signal_handler_type" is set to a value				\
		 * other than sig_hndlr_none at this point (for example with the following C-stack)				\
		 *   "wait_for_repl_inst_unfreeze_nocsa_jpl"									\
		 *    -> "ydb_stm_invoke_deferred_signal_handler"								\
		 *      -> "timer_handler"											\
		 *        -> "wcs_stale"											\
		 *          -> "wcs_wtstart"											\
		 *            -> "wait_for_repl_inst_unfreeze"									\
		 *              -> "wait_for_repl_inst_unfreeze_nocsa_jpl"							\
		 *                -> "STAPI_INVOKE_DEFERRED_SIGNAL_HANDLER_IF_NEEDED"						\
		 *                  -> "ydb_stm_invoke_deferred_signal_handler"							\
		 * In this case, we want the nested invocation to happen as it might be handling a				\
		 * deferred SIGTERM (for example) which would terminate the process. Not doing the				\
		 * nested invocation would cause the process to not exit in a timely fashion.					\
		 * Hence the need to reset "ydb_stm_invoke_deferred_signal_handler_type" here (as				\
		 * otherwise "ydb_stm_invoke_deferred_signal_handler" would return immediately).				\
		 * Therefore, if caller says OK_TO_NEST is TRUE, then we do the reset thereby allowing				\
		 * the nested invocation. If not, we skip the reset thereby the nested invocation will				\
		 * return right away.												\
		 */														\
		lcl_stm_sighndlr_type = ydb_stm_invoke_deferred_signal_handler_type;						\
		if (OK_TO_NEST)													\
			ydb_stm_invoke_deferred_signal_handler_type = sig_hndlr_none;						\
		(*ydb_stm_invoke_deferred_signal_handler_fnptr)();								\
		ydb_stm_invoke_deferred_signal_handler_type = lcl_stm_sighndlr_type ;						\
	}															\
}

#define	SAVE_OS_SIGNAL_HANDLER_SIGNUM(SIGHNDLRTYPE, SIGNUM)	stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_num = SIGNUM

#define	SAVE_OS_SIGNAL_HANDLER_INFO(SIGHNDLRTYPE, INFO)							\
{													\
	if (NULL != INFO)										\
		stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_info = *(siginfo_t *)INFO;		\
	else												\
		memset(&stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_info, 0, SIZEOF(siginfo_t));	\
	INFO = &stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_info;					\
}

#define	SAVE_OS_SIGNAL_HANDLER_CONTEXT(SIGHNDLRTYPE, CONTEXT)							\
{														\
	if (NULL != CONTEXT)											\
		stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_context = *(gtm_sigcontext_t *)CONTEXT;	\
	else													\
		memset(&stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_context, 0,				\
				SIZEOF(stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_context));		\
	CONTEXT = &stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_context;					\
}

#define	DO_FORK_N_CORE_IN_THIS_THREAD_IF_NEEDED(SIG)						\
{												\
	/* This thread got a signal that will trigger exit handler processing. The signal	\
	 * has been forwarded to the appropriate thread. But it is possible we got a signal	\
	 * that requires this thread's current C-stack for further analysis (e.g. SIGSEGV).	\
	 * In this case, we should not return from this thread but wait for the signal handling \
	 * thread to continue exit processing and let us know when we should do a		\
	 * "gtm_fork_n_core". This way the core that gets dumped after a fork (which can only	\
	 * have the current thread's C-stack) will be from this thread rather than the signal	\
	 * handling thread (it is this thread that got the SIGSEGV and hence this is what the	\
	 * user cares about). So wait for such a signal from the signal handling thread.	\
	 * Need to do this only in case the incoming signal SIG would have not set		\
	 * "dont_want_core" to TRUE in "generic_signal_handler". This is easily identified	\
	 * by the IS_DONT_WANT_CORE_TRUE macro. Note that we cannot do a "gtm_fork_n_core"	\
	 * in this thread at any time since the signal handling thread would be concurrently	\
	 * running the YottaDB engine and we do not want more than one thread running that at	\
	 * the same time (YottaDB engine is not multi-thread safe yet). Hence waiting for the	\
	 * signal handling thread to let us know when it is safe for us to take over control of	\
	 * the YottaDB engine for a short period of time (1 minute) to generate the core.	\
	 */											\
	if (!IS_DONT_WANT_CORE_TRUE(SIG))							\
	{											\
		for (i = 0; i < SAFE_TO_FORK_N_CORE_TRIES; i++)					\
		{										\
			if (safe_to_fork_n_core)						\
			{	/* signal handling thread gave us okay to "gtm_fork_n_core" */	\
				break;								\
			}									\
			SLEEP_USEC(SAFE_TO_FORK_N_CORE_SLPTIME_USEC, TRUE);			\
		}										\
		gtm_fork_n_core();								\
		safe_to_fork_n_core = FALSE;	/* signal handling thread to resume */		\
	}											\
}

#ifdef GTM_PTHREAD

#define	SET_YDB_ENGINE_MUTEX_HOLDER_THREAD_ID(HOLDER_THREAD_ID, TLEVEL)						\
{														\
	GBLREF	uint4		dollar_tlevel;									\
	GBLREF	pthread_t	ydb_engine_threadsafe_mutex_holder[];						\
														\
	/* If not in TP, the YottaDB engine lock index is 0 (i.e. ydb_engine_threadsafe_mutex_holder[0] is	\
	 * current lock holder thread if it is non-zero). But if we are in TP, then lock index could be		\
	 * "dollar_tlevel"     : e.g. if a "ydb_get_st" call occurs inside of the "ydb_tp_st" call OR		\
	 * "dollar_tlevel - 1" : if control is in the TP callback function inside "ydb_tp_st" but not a		\
	 *	SimpleThreadAPI call like "ydb_get_st" etc.							\
	 */													\
	TLEVEL = dollar_tlevel;	/* take a local copy of global variable as it could be concurrently changing */	\
	if (!TLEVEL)												\
		HOLDER_THREAD_ID = ydb_engine_threadsafe_mutex_holder[0];					\
	else													\
	{													\
		HOLDER_THREAD_ID = ydb_engine_threadsafe_mutex_holder[TLEVEL];					\
		if (!HOLDER_THREAD_ID)										\
			HOLDER_THREAD_ID = ydb_engine_threadsafe_mutex_holder[TLEVEL - 1];			\
	}													\
}

/* Macro returns TRUE if SIG is generated as a direct result of the execution of a specific hardware instruction
 * within the context of the thread/process.
 */
#define	IS_SIG_THREAD_DIRECTED(SIG)	((SIGBUS == SIG) || (SIGFPE == SIG) || (SIGILL == SIG) || (SIGSEGV == SIG))

/* If SIG is DUMMY_SIG_NUM, we are guaranteed this is a forwarded signal. Otherwise, it could still be forwarded
 * from a "pthread_kill" invocation from the STAPI signal thread or the thread that randomly got the original signal.
 * We find that out by using the linux-only SI_TKILL info code to check if this signal came from a "tkill" system call.
 * Even if the info code is SI_TKILL, it is possible a completely different signal than what was recorded before comes
 * in. In that case, do not treat that as a forwarded signal but as a new signal. Hence the "sig_num" check below.
 */
#define	IS_SIGNAL_FORWARDED(SIGHNDLRTYPE, SIG, INFO)					\
	((DUMMY_SIG_NUM == SIG)								\
	     || (simpleThreadAPI_active && (SI_TKILL == INFO->si_code)			\
		 && (SIG == stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_num)))

/* Values to pass to FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED() macro below as the IS_EXI_SIGNAL parameter */
#define	IS_EXI_SIGNAL_FALSE	FALSE
#define	IS_EXI_SIGNAL_TRUE	TRUE

/* If we detect a case when the signal came to a thread other than the main GT.M thread, this macro will redirect the signal to the
 * main thread if such is defined. Such scenarios is possible, for instance, if we are running along a JVM, which, upon receiving a
 * signal, dispatches a new thread to invoke signal handlers other than its own. The pthread_kill() enables us to target the signal
 * to a specific thread rather than rethrow it to the whole process.
 */
#define FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(SIGHNDLRTYPE, SIG, IS_EXI_SIGNAL, INFO, CONTEXT)					\
{																\
	GBLREF	pthread_t		gtm_main_thread_id;									\
	GBLREF	boolean_t		gtm_main_thread_id_set;									\
	GBLREF	boolean_t		safe_to_fork_n_core;									\
	GBLREF	pthread_t		ydb_engine_threadsafe_mutex_holder[];							\
	GBLREF	boolean_t		gtm_jvm_process;									\
	GBLREF	enum sig_handler_t	ydb_stm_invoke_deferred_signal_handler_type;						\
	DEBUG_ONLY(GBLREF uint4	dollar_tlevel;)											\
																\
	pthread_t		mutexHolderThreadId, thisThreadId;								\
	int			i, tLevel;											\
	boolean_t		isSigThreadDirected, signalForwarded, thisThreadIsMutexHolder;					\
																\
	assert(!USING_ALTERNATE_SIGHANDLING);											\
	assert((DUMMY_SIG_NUM == SIG) || (NULL != INFO));									\
	if (DUMMY_SIG_NUM != SIG)												\
	{	/* This is an invocation of the signal handler by the OS. Could be a forwarded signal handler invocation in	\
		 * case this is invoked by "pthread_kill" from another thread in the function "ydb_stm_thread" or this macro	\
		 * "FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED" itself. The variable "signalForwarded" tracks this below.		\
		 */														\
		if (simpleThreadAPI_active)											\
		{	/*													\
			 * Note: The below comment talks about SIGALRM as an example but the same reasoning applies to		\
			 * other signals too which this macro can be called for.						\
			 *													\
			 * If SimpleThreadAPI is active, the STAPI signal thread is usually the one that will receive		\
			 * the SIGALRM signal (when using POSIX timers). Note though that it is possible some other		\
			 * thread also receives the signal (for example if a C program holding a parent M-lock with		\
			 * SimpleAPI can fork off processes that start waiting for a child M-lock with SimpleThreadAPI		\
			 * in which case the lock wake up signal, also the same SIGALRM signal, from the parent program		\
			 * would get sent to the child process-id, using "crit_wake", which should forward the signal		\
			 * to the STAPI signal thread OR the currently active thread that is holding the YottaDB engine		\
			 * lock). In any case, as long as the thread holds the YottaDB engine lock, we can continue		\
			 * "timer_handler" processing in this thread. If not, we need to defer the invocation until a		\
			 * later point when we do hold the lock (cannot do a "pthread_mutex_lock" call here since we		\
			 * are inside a signal handler and "pthread_mutex_lock" is not async-signal-safe).  Record the		\
			 * fact that a timer invocation happened and let the STAPI signal thread invoke "timer_handler"		\
			 * outside of the signal handler as part of its normal processing ("ydb_stm_thread"). 			\
			 */													\
			thisThreadId = pthread_self();										\
			assert(thisThreadId);											\
			SET_YDB_ENGINE_MUTEX_HOLDER_THREAD_ID(mutexHolderThreadId, tLevel);					\
			/* If we are in TP, we hold the mutex lock "ydb_engine_threadsafe_mutex[tLevel]" but it is possible	\
			 * concurrent threads are creating nested TP transactions in which case the YottaDB engine mutex lock	\
			 * would move to "ydb_engine_threadsafe_mutex[tLevel+1]" and so on. In order to be sure we are the only	\
			 * thread currently holding the YottaDB engine mutex lock, we need to acquire the "tLevel+1" lock and	\
			 * higher but we cannot do that while inside a signal handler therefore we cannot safely determine if	\
			 * we are the engine lock holder in TP. In that case, we forward the signal to the current holder	\
			 * thread. Even if we forward to the wrong thread (since we are not determining the engine lock holder	\
			 * using any other locks), it is okay since the STAPI signal thread ("ydb_stm_thread") will take care	\
			 * of periodically forwarding the signal to the current engine lock holder thread until it eventually	\
			 * gets handled. The only exception to this rule is if we are handling a signal that is thread directed	\
			 * (e.g. SIG-11/SIGSEGV) in which case we check if we hold the "ydb_engine_threadsafe_mutex[tLevel]"	\
			 * lock and if so we assume there is no other thread holding "ydb_engine_threadsafe_mutex[tLevel+1]"	\
			 * and treat us as holding the YottaDB engine lock and proceed with signal handling even if we are in	\
			 * TP. This is to avoid a scenario where we otherwise forward the signal to ourselves eternally (as	\
			 * we are the YottaDB engine lock holder) and not proceed to invoking the signal handler function.	\
			 */													\
			thisThreadIsMutexHolder = pthread_equal(mutexHolderThreadId, thisThreadId);				\
			isSigThreadDirected = IS_SIG_THREAD_DIRECTED(SIG);							\
			signalForwarded = IS_SIGNAL_FORWARDED(SIGHNDLRTYPE, SIG, INFO);						\
			if (signalForwarded && (SIGHNDLRTYPE == ydb_stm_invoke_deferred_signal_handler_type))			\
			{	/* This is a forwarded signal that we are receiving while we are already executing the		\
				 * same signal handler in a deferred fashion through "ydb_stm_invoke_deferred_signal_handler".	\
				 * We do not want to nest the signal handler since those handlers are not re-entrant.		\
				 * So skip this signal handler invocation and return to the caller so the deferred signal	\
				 * handler invocation can resume its work.							\
				 */												\
				 return;											\
			}													\
			if (!thisThreadIsMutexHolder										\
					|| (!IS_EXI_SIGNAL && (tLevel && (!isSigThreadDirected || signalForwarded))))		\
			{	/* Two possibilities.										\
				 *  a) We do not hold the YottaDB engine lock OR						\
				 *  b) We hold the YottaDB engine lock but this is not a signal that terminates the process	\
				 *	(e.g. SIGALRM) and we are in TP and this is not a thread-directed signal or this is	\
				 *	a thread-directed signal that is forwarded. If this signal is one that terminates the	\
				 *	process, we assume that we are the mutex lock holder (even if we cannot confirm/acquire	\
				 *	 the "tLevel+1" lock) and go to handle it.						\
				 * In either case, we cannot handle the signal right away. Note down this to be handled in a	\
				 * deferred manner and return.									\
				 */												\
				if (!signalForwarded)										\
				{												\
					STAPI_SET_SIGNAL_HANDLER_DEFERRED(SIGHNDLRTYPE, SIG, INFO, CONTEXT);			\
					if (IS_EXI_SIGNAL)									\
						DO_FORK_N_CORE_IN_THIS_THREAD_IF_NEEDED(SIG);					\
				}												\
				return;												\
			} else													\
			{	/* Two possibilities.										\
				 * a) We are not in TP and hold the YottaDB engine lock "ydb_engine_threadsafe_mutex[0]".	\
				 *    We are guaranteed no new TP transaction happens while we hold this lock.			\
				 *    Therefore we can safely proceed to handle the signal.					\
				 * b) We are in TP and hold the engine lock at the current Level				\
				 *      1) this is a thread-directed signal OR							\
				 *      2) this is not a thread-directed signal but is a process terminating signal		\
				 */												\
				if (!signalForwarded)										\
				{	/* Signal was not forwarded.								\
					 * Reset "INFO" and "CONTEXT" to be usable by a later call to "extract_signal_info".	\
					 */											\
					SAVE_OS_SIGNAL_HANDLER_INFO(SIGHNDLRTYPE, INFO);					\
					SAVE_OS_SIGNAL_HANDLER_CONTEXT(SIGHNDLRTYPE, CONTEXT);					\
				} else												\
				{	/* Signal was forwarded. Reuse "info" and "context" saved from prior call.		\
					 * Set "INFO" and "CONTEXT" to be usable by a later call to "extract_signal_info".	\
					 */											\
					assert(SIG == stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_num);			\
					INFO = &stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_info;				\
					CONTEXT = &stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_context;			\
				}												\
			}													\
		} else														\
		{														\
			thisThreadId = pthread_self();										\
			if (gtm_main_thread_id_set && !pthread_equal(gtm_main_thread_id, thisThreadId))				\
			{	/* Only redirect the signal if the main thread ID has been defined, and we are not that. */	\
				/* Caller wants INFO and CONTEXT to be recorded as forwarding the signal would lose		\
				 * that information in the forwarded call to the signal handler. Do that first.			\
				 * Then forward the signal.									\
				 */												\
				STAPI_SET_SIGNAL_HANDLER_DEFERRED(SIGHNDLRTYPE, SIG, INFO, CONTEXT);				\
				/* Now that we have saved OS signal handler information, forward the signal */			\
				pthread_kill(gtm_main_thread_id, SIG);								\
				if (IS_EXI_SIGNAL)										\
					DO_FORK_N_CORE_IN_THIS_THREAD_IF_NEEDED(SIG);						\
				return;												\
			}													\
			/* Reset "INFO" and "CONTEXT" to be usable by a later call to "extract_signal_info" */			\
			SAVE_OS_SIGNAL_HANDLER_INFO(SIGHNDLRTYPE, INFO);							\
			SAVE_OS_SIGNAL_HANDLER_CONTEXT(SIGHNDLRTYPE, CONTEXT);							\
		}														\
	} else															\
	{	/* Signal handler is explicitly invoked by YottaDB (e.g. by "ydb_stm_invoke_deferred_signal_handler") after	\
		 * signal forwarding has taken effect while having multiple threads (currently possible	in SimpleThreadAPI	\
		 * mode or with the Java interface). Use stored signal number/info/context and continue the signal handler	\
		 * invocation. Note that "timer_handler" can be invoked with DUMMY_SIG_NUM even outside of SimpleThreadAPI	\
		 * mode so account for that in the assert below.								\
		 */														\
		assert(simpleThreadAPI_active || gtm_jvm_process || (sig_hndlr_timer_handler == SIGHNDLRTYPE));			\
		SIG = stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_num;							\
		/* Reset "INFO" and "CONTEXT" to be usable by a later call to "extract_signal_info" */				\
		INFO = &stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_info;							\
		CONTEXT = &stapi_signal_handler_oscontext[SIGHNDLRTYPE].sig_context;						\
		STAPI_CLEAR_SIGNAL_HANDLER_DEFERRED(SIGHNDLRTYPE);								\
	}															\
}
#endif

/* Include this at the end as it needs many of the macros above defined */
#include "sighnd_debug.h"

int drain_signal_queues(ydb_buffer_t *errstr);
void sig_init(void (*signal_handler)(), void (*ctrlc_handler)(), void (*suspsig_handler)(), void (*continue_handler)());
void null_handler(int sig, siginfo_t *info, void *context);
void ydb_stm_invoke_deferred_signal_handler(void);
void setup_altstack(void);

#endif
