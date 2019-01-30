/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2019 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GENERIC_SIGNAL_HANDLER_INCLUDED
#define GENERIC_SIGNAL_HANDLER_INCLUDED

#include "gtm_string.h"

#include <errno.h>

#include "error.h"
#include "gtmsiginfo.h"
#include "sleep.h"
#include "sleep_cnt.h"

/* Define debugging macros for signal handling - uncomment the define below to enable but note it emits
 * output to stderr.
 */
//#define DEBUG_SIGNAL_HANDLING
#ifdef DEBUG_SIGNAL_HANDLING
# define DBGSIGHND(x) DBGFPF(x)
#else
# define DBGSIGHND(x)
#endif

GBLREF	siginfo_t		exi_siginfo;
GBLREF	gtm_sigcontext_t 	exi_context;
GBLREF	int			exi_signal_forwarded;
GBLREF	void			(*ydb_stm_thread_exit_fnptr)(void);

/* When we share signal handling with a main in simpleAPI mode and need to drive the non-YottaDB base routine's handler
 * for a signal, we need to move it to a matching type because Linux does not define the handler with information
 * attributes (the siginfo_t and context parameters sent when SA_SIGINFO is specified) without some exotic C99* flag.
 * Use this type to drive the handler.
 */
typedef void (*nonYDB_sighandler_t)(int, siginfo_t *, void *);

#define IS_HANDLER_DEFINED(SIGNAL) \
	((SIG_DFL != orig_sig_action[(SIGNAL)].sa_handler) && (SIG_IGN != orig_sig_action[(SIGNAL)].sa_handler))

#define DRIVE_NON_YDB_SIGNAL_HANDLER_IF_ANY(NAME, SIGNAL, INFO, CONTEXT, DRIVEEXIT)					\
MBSTART {														\
	nonYDB_sighandler_t	sighandler;										\
															\
	if ((MUMPS_CALLIN & invocation_mode) && IS_HANDLER_DEFINED(SIGNAL))						\
	{														\
		assert((0 < (SIGNAL)) && (NSIG >= (SIGNAL)));								\
		if (DRIVEEXIT)												\
		{													\
			DBGSIGHND((stderr, "%s: Driving ydb_stm_thread_exit() prior to signal passthru\n", NAME));	\
			if (NULL != ydb_stm_thread_exit_fnptr)								\
				(*ydb_stm_thread_exit_fnptr)();								\
		}													\
		sighandler = (nonYDB_sighandler_t)(orig_sig_action[(SIGNAL)].sa_handler);				\
		DBGSIGHND((stderr, "%s: Passing signal %d through to the caller\n", (NAME), (SIGNAL)));			\
		(*sighandler)((SIGNAL), (INFO), (CONTEXT));	/* Note - likely to NOT return */			\
		DBGSIGHND((stderr, "%s: Returned from signal passthru for signal %d\n", (NAME), (SIGNAL)));		\
	}														\
} MBEND

/* Macro to check if a given signal sets the "dont_want_core" variable to TRUE in the function "generic_signal_handler" */
#define	IS_DONT_WANT_CORE_TRUE(SIG)	((SIGQUIT == SIG) || (SIGTERM == SIG))

#define	SET_EXI_SIGINFO_AS_APPROPRIATE(INFO)			\
{								\
	if (NULL != INFO)					\
		exi_siginfo = *(siginfo_t *)INFO;		\
	else							\
		memset(&exi_siginfo, 0, SIZEOF(siginfo_t));	\
}

#define	SET_EXI_CONTEXT_AS_APPROPRIATE(CONTEXT)			\
{								\
	if (NULL != CONTEXT)					\
		exi_context = *(gtm_sigcontext_t *)CONTEXT;	\
	else							\
		memset(&exi_context, 0, SIZEOF(exi_context));	\
}

#define	IS_EXI_SIGNAL_FALSE	FALSE
#define	IS_EXI_SIGNAL_TRUE	TRUE

#ifdef GTM_PTHREAD
/* If we detect a case when the signal came to a thread other than the main GT.M thread, this macro will redirect the signal to the
 * main thread if such is defined. Such scenarios is possible, for instance, if we are running along a JVM, which, upon receiving a
 * signal, dispatches a new thread to invoke signal handlers other than its own. The pthread_kill() enables us to target the signal
 * to a specific thread rather than rethrow it to the whole process.
 */
#define FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(SIG, IS_EXI_SIGNAL, INFO, CONTEXT)					\
{														\
	GBLREF	pthread_t	gtm_main_thread_id;								\
	GBLREF	boolean_t	gtm_main_thread_id_set;								\
	GBLREF	boolean_t	safe_to_fork_n_core;								\
														\
	pthread_t		this_thread_id;									\
	int			i;										\
														\
	this_thread_id = pthread_self();									\
	if (gtm_main_thread_id_set && !pthread_equal(gtm_main_thread_id, this_thread_id))			\
	{	/* Only redirect the signal if the main thread ID has been defined, and we are not that. */	\
		if (IS_EXI_SIGNAL)										\
		{	/* Caller wants INFO and CONTEXT to be recorded as forwarding the signal would lose	\
			 * that information in the forwarded call to the signal handler. Do that first.		\
			 * Then forward the signal.								\
			 */											\
			exi_signal_forwarded = SIG;								\
			SET_EXI_SIGINFO_AS_APPROPRIATE(INFO);							\
			SET_EXI_CONTEXT_AS_APPROPRIATE(CONTEXT);						\
		}												\
		pthread_kill(gtm_main_thread_id, SIG);								\
		if (IS_EXI_SIGNAL)										\
		{	/* This thread got a signal that will trigger exit handler processing. The signal	\
			 * has been forwarded to the MAIN worker thread. But it is possible we got a signal	\
			 * that requires this thread's current C-stack for further analysis (e.g. SIGSEGV).	\
			 * In this case, we should not return from this thread but wait for the MAIN worker	\
			 * thread to continue exit processing and let us know when we should do a		\
			 * "gtm_fork_n_core". This way the core that gets dumped after a fork (which can only	\
			 * have the current thread's C-stack) will be from this thread rather than the MAIN	\
			 * worker thread (it is this thread that got the SIGSEGV and hence this is what the	\
			 * user cares about). So wait for such a signal from the MAIN worker thread.		\
			 * Need to do this only in case the incoming signal SIG would have not set		\
			 * "dont_want_core" to TRUE in "generic_signal_handler". This is easily identified	\
			 * by the IS_DONT_WANT_CORE_TRUE macro. Note that we cannot do a "gtm_fork_n_core"	\
			 * in this thread at any time since the MAIN worker thread would be concurrently	\
			 * running the YottaDB engine and we do not want more than one thread running that at	\
			 * the same time (YottaDB engine is not multi-thread safe yet). Hence waiting for the	\
			 * MAIN worker thread to let us know when it is safe for us to take over control of	\
			 * the YottaDB engine for a short period of time (1 minute) to generate the core.	\
			 */											\
			if (!IS_DONT_WANT_CORE_TRUE(SIG))							\
			{											\
				for (i = 0; i < SAFE_TO_FORK_N_CORE_TRIES; i++)					\
				{										\
					if (safe_to_fork_n_core)						\
					{	/* MAIN worker thread has told us to do "gtm_fork_n_core" */	\
						break;								\
					}									\
					SLEEP_USEC(SAFE_TO_FORK_N_CORE_SLPTIME_USEC, TRUE);			\
				}										\
				gtm_fork_n_core();								\
				safe_to_fork_n_core = FALSE;	/* signal MAIN worker thread to resume */	\
			}											\
		}												\
		return;												\
	}													\
}
#else
#define FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(SIG)
#endif

#define	MULTI_THREAD_AWARE_FORK_N_CORE(SIGNAL_FORWARDED)								\
{															\
	GBLREF	boolean_t	safe_to_fork_n_core;									\
															\
	int	i;													\
															\
	if (!SIGNAL_FORWARDED)												\
		gtm_fork_n_core();	/* Generate "virgin" core while we can */					\
	else														\
	{	/* Signal the forwarding thread that it is now okay for it to do the "gtm_fork_n_core".			\
		 * Wait for that to be done and then proceed. Do not wait indefinitely (60 second max wait).		\
		 */													\
		assert(!safe_to_fork_n_core);										\
		safe_to_fork_n_core = TRUE;	/* this sends the signal to the forwarding thread */			\
		for (i = 0; i < SAFE_TO_FORK_N_CORE_TRIES; i++)								\
		{													\
			if (!safe_to_fork_n_core)									\
			{	/* forwarding thread has done the "gtm_fork_n_core" */					\
				break;											\
			}												\
			SLEEP_USEC(SAFE_TO_FORK_N_CORE_SLPTIME_USEC, TRUE);						\
		}													\
		safe_to_fork_n_core = FALSE;	/* reset it to be safe in case we timed out above */			\
	}														\
}

void generic_signal_handler(int sig, siginfo_t *info, void *context);

#endif /* GENERIC_SIGNAL_HANDLER_INCLUDED */
