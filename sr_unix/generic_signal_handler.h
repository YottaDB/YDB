/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
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
#define	IS_DONT_WANT_CORE_TRUE(SIG)	((SIGQUIT == SIG) || (SIGTERM == SIG) || (SIGINT == SIG))

#define	IS_EXI_SIGNAL_FALSE	FALSE
#define	IS_EXI_SIGNAL_TRUE	TRUE

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
