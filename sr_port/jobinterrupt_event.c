/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

/* job interrupt event - an interrupt has been requested.

   - Call xfer_set_handlers so next M instruction traps to interrupt routine

*/

#include "mdef.h"
<<<<<<< HEAD

#include "gtm_signal.h"
=======
#  include <gtm_signal.h>
>>>>>>> 52a92dfd (GT.M V7.0-001)
#ifdef GTM_PTHREAD
#  include <gtm_pthread.h>
#endif
#include "gtm_stdio.h"

#include "io.h"
#include "gtmio.h"
#include "op.h"
#include "xfer_enum.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "deferred_events.h"
#include "jobinterrupt_process.h"
#include "fix_xfer_entry.h"
#include "sig_init.h"
#include "libyottadb_int.h"
#include "invocation_mode.h"

GBLREF intrpt_state_t		intrpt_ok_state;
GBLREF	xfer_entry_t		xfer_table[];
GBLREF	volatile int4 		outofband;
GBLREF	volatile boolean_t	dollar_zininterrupt;
GBLREF	struct sigaction	orig_sig_action[];
GBLREF	int			jobinterrupt_sig_num;

/* Routine called when an interrupt event occurs (signaled by mupip intrpt or other future method
<<<<<<< HEAD
 * of signaling interrupts). This code is driven as a signal handler on Unix where it intercepts the posix signal.
 */
void jobinterrupt_event(int sig, siginfo_t *info, void *context)
{
	if (!USING_ALTERNATE_SIGHANDLING)
	{
		FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig_hndlr_jobinterrupt_event, sig, IS_EXI_SIGNAL_FALSE, info, context);
	}
	/* Note the (presently unused) args are to match signature for signal handlers in Unix */
	if (!dollar_zininterrupt)
		(void)xfer_set_handlers(outofband_event, &jobinterrupt_set, sig, FALSE);
	/* If we are in SIMPLEAPI mode and the original handler was neither SIG_DFL or SIG_IGN, drive the originally
	 * defined handler before we replaced them.
	 */
	if (!USING_ALTERNATE_SIGHANDLING)
	{
		drive_non_ydb_signal_handler_if_any("jobinterrupt_event", sig, info, context, FALSE);
	}
=======
 * of signaling interrupts). This code is driven as a signal handler on Unix.
 */
void jobinterrupt_event(int sig, siginfo_t *info, void *context)
{	/* Note the (presently unused) args are to match signature for signal handlers in Unix */
	FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig);
	if (!dollar_zininterrupt)
		(void)xfer_set_handlers(jobinterrupt, 0, FALSE);
>>>>>>> 52a92dfd (GT.M V7.0-001)
}

/* Call back routine from xfer_set_handlers to complete outofband setup */
void jobinterrupt_set(int4 sig_num)
{
<<<<<<< HEAD
	DBGDFRDEVNT((stderr, "jobinterrupt_set: Setting jobinterrupt outofband\n"));
	if (jobinterrupt != outofband)
	{	/* We need jobinterrupt out of band processing at our earliest convenience */
		SET_OUTOFBAND(jobinterrupt);
		assert((SIGUSR1 == sig_num) || (SIGUSR2 == sig_num));
		/* Note down the signal number that triggered $ZINTERRUPT processing.
		 * This is used later to retrieve the value of the ISV $ZYINTRSIG.
		 */
		jobinterrupt_sig_num = sig_num;
	}
=======
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
>>>>>>> 52a92dfd (GT.M V7.0-001)
}
