/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "have_crit_any_region.h"
#include "suspsigs_handler.h"

GBLREF	volatile int		suspend_status;
GBLREF	uint4			process_id;
GBLREF	volatile int4		exit_state;
GBLREF	enum gtmImageTypes	image_type;

#ifdef __sparc
void suspsigs_handler(int sig)
#else
void suspsigs_handler(int sig, siginfo_t* info, void *context)
#endif
{
	sigset_t	block_susp_sigs, oldsigmask;
	int		status;

	switch(sig)
	{
		case SIGTTIN:
		case SIGTTOU:
			/* Terminal access while running in the background */
			if (GTMSECSHR_IMAGE != image_type) /* Ignore signal if gtmsecshr */
			{
				if (EXIT_NOTPENDING < exit_state || have_crit_any_region(TRUE))
				{
					 /* Let the process run in the foreground till it finishes the terminal I/O */
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
					break;
				}
				suspend();
			}
			break;

		case SIGTSTP:
			if (GTMSECSHR_IMAGE != image_type) /* not a good idea to suspend gtmsecshr */
			{
				if (EXIT_NOTPENDING == exit_state) /* not processing other signals */
				{
					if (have_crit_any_region(TRUE))
					{
						suspend_status = DEFER_SUSPEND;
						break;
					}
					suspend();
				}
			}
			break;

		default:
			GTMASSERT;
	}
	return;
}
