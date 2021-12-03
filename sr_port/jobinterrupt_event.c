/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* job interrupt event - an interrupt has been requested.

   - Call xfer_set_handlers so next M instruction traps to interrupt routine
   - Other required housecleaning for VMS.

*/

#include "mdef.h"
#  include <gtm_signal.h>
#ifdef GTM_PTHREAD
#  include <gtm_pthread.h>
#endif
#include "gtm_stdio.h"
#include "io.h"
#include "op.h"
#include "xfer_enum.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "deferred_events.h"
#include "jobinterrupt_process.h"
#include "fix_xfer_entry.h"

GBLREF intrpt_state_t		intrpt_ok_state;
GBLREF	xfer_entry_t		xfer_table[];
GBLREF	volatile int4 		outofband;
GBLREF	volatile boolean_t	dollar_zininterrupt;

/* Routine called when an interrupt event occurs (signaled by mupip intrpt or other future method
 * of signaling interrupts). This code is driven as a signal handler on Unix.
 */
void jobinterrupt_event(int sig, siginfo_t *info, void *context)
{	/* Note the (presently unused) args are to match signature for signal handlers in Unix */
	FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig);
	if (!dollar_zininterrupt)
		(void)xfer_set_handlers(jobinterrupt, 0, FALSE);
}

/* Call back routine from xfer_set_handlers to complete outofband setup */
void jobinterrupt_set(int4 dummy_val)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(INTRPT_IN_EVENT_HANDLING == intrpt_ok_state);
	assert(pending == TAREF1(save_xfer_root, jobinterrupt).event_state);
	assert(jobinterrupt == outofband);
	DBGDFRDEVNT((stderr, "%d %s: jobinterrupt_set - %sneeded\n", __LINE__, __FILE__,
		     (jobinterrupt == outofband) ? "NOT " : ""));
	outofband = jobinterrupt;
	DEFER_INTO_XFER_TAB;
	TAREF1(save_xfer_root, jobinterrupt).event_state = active;
	DBGDFRDEVNT((stderr, "%d %s: jobinterrupt_set - set the xfer entries for jobinterrupt_event\n", __LINE__, __FILE__));
}
