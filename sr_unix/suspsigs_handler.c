/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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

#define MAXINOUT	16

GBLREF	volatile int		suspend_status;
GBLREF	uint4			process_id;
GBLREF	volatile int4		exit_state;
GBLREF	volatile int4		gtmMallocDepth; 	/* Recursion indicator */
GBLDEF	uint4			sig_count=0;

void suspsigs_handler(int sig, siginfo_t* info, void *context)
{
	sigset_t	block_susp_sigs, oldsigmask;
	int		status;

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
						suspend_status = DEFER_SUSPEND;
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
						suspend_status = DEFER_SUSPEND;
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
