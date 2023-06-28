/****************************************************************
 *								*
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2019-2023 YottaDB LLC and/or its subsidiaries.	*
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

GBLREF void (*ydb_stm_thread_exit_fnptr)(void);
void drive_non_ydb_signal_handler_if_any(char *caller, int sig, siginfo_t *info, void *context, boolean_t drive_exit);

#define YDB_ALTSTACK_SIZE	(256 * 1024)	/* Required size for alt-stack */

/* Macro to check if a given signal sets the "dont_want_core" variable to TRUE in the function "generic_signal_handler" */
#define	IS_DONT_WANT_CORE_TRUE(SIG)	((SIGQUIT == SIG) || (SIGTERM == SIG) || (SIGINT == SIG))

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

void generic_signal_handler(int sig, siginfo_t *info, void *context, boolean_t is_os_signal_handler);

#endif /* GENERIC_SIGNAL_HANDLER_INCLUDED */
