/****************************************************************
 *								*
 * Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <signal.h>
#include <errno.h>

#include "error.h"
#include "gtmsiginfo.h"
#include "gtmimagename.h"
#include "have_crit.h"
#include "suspsigs_handler.h"
#include "sig_init.h"

#define MAXINOUT	16

GBLREF	uint4			process_id;
GBLREF	volatile int4		exit_state;
GBLREF	volatile int4		gtmMallocDepth; 	/* Recursion indicator */
GBLDEF	uint4			sig_count = 0;

void suspsigs_handler(int sig, siginfo_t* info, void *context)
{
	sigset_t	block_susp_sigs, oldsigmask;
	int		status;

	if (!USING_ALTERNATE_SIGHANDLING)
	{
		FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig_hndlr_suspsigs_handler, sig, IS_EXI_SIGNAL_FALSE, info, context);
		/* Note: We do not have any "drive_non_ydb_signal_handler_if_any()" usages below. That is, we do not forward
		 * these signals to any non-YottaDB signal handler routine (e.g Go program using the YottaDB GoWrapper)
		 * in this case but instead depend purely on YottaDB's suspend/continue operations.
		 */
	}
	switch(sig)
	{
		case SIGTTIN:
		case SIGTTOU:
			/* Terminal access while running in the background */
			if (!IS_GTMSECSHR_IMAGE)	/* Ignore signal if gtmsecshr */
			{
				if ((EXIT_NOTPENDING < exit_state) || !OK_TO_INTERRUPT)
				{	/* Let the process run in the foreground till it finishes the terminal I/O */
					if (EXIT_NOTPENDING == exit_state)
						SET_DEFERRED_CTRLZ_CHECK_NEEDED;
					status = kill(process_id, SIGCONT);
					assert(0 == status);
					/*
					 * If SIGCONT is not delivered before we go back to the I/O operation that caused
					 * SIGTTIN/OU, these signals can be generated again. In such cases,
					 * we might send ourselves an extra SIGCONT. The extra SIGCONT won't affect
					 * the I/O operation since we are running in the foreground already due to the
					 * first SIGCONT.
					 */
					sig_count++;
					/* When process receives SIGTTIN or SIGTTOUT signal, it goes into suspend mode.
					 * If it is NOT OK_TO_INTERRUPT i.e. if process is holding some critical
					 * resources, the suspension of the process is deferred until the critical
					 * resources are released. It is not expected to arrive extra TTIN/TTOU signals
					 * between arrival of first TTIN/TTOU and actual suspension of the process.
					 * If such out of design situation arises, we limit the number of unexpected
					 * TTIN/TTOU to MAXINOUT and then GTMASSERT.
					 */
					if (MAXINOUT == sig_count)
						GTMASSERT;
					break;
				}
				suspend(sig);
			}
			break;

		case SIGTSTP:
			if (!IS_GTMSECSHR_IMAGE)	/* not a good idea to suspend gtmsecshr */
			{
				if (EXIT_NOTPENDING == exit_state) /* not processing other signals */
				{
					if (!OK_TO_INTERRUPT)
					{
						SET_DEFERRED_CTRLZ_CHECK_NEEDED;
						break;
					}
					suspend(sig);
				}
			}
			break;

		default:
			GTMASSERT;
	}
	return;
}
