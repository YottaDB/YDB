/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Perform necessary functions for signal handling that was deferred */
#include "mdef.h"

#include "gtm_stdlib.h"		/* for EXIT() */
#include "gtm_signal.h"

#include "error.h"
#include "gtmsiginfo.h"
#include "gtmimagename.h"
#include "send_msg.h"
#include "io.h"
#include "gtmio.h"
#include "have_crit.h"
#include "deferred_exit_handler.h"
#include "gtmmsg.h"
#include "forced_exit_err_display.h"
#include "libyottadb_int.h"
#include "invocation_mode.h"
#include "sig_init.h"
#include "gtm_exit_handler.h"
#include "signal_exit_handler.h"
#ifdef DEBUG
#include "wcs_sleep.h"
#include "wbox_test_init.h"
#include "gt_timer.h"
#endif

GBLREF	int4			exi_condition;
GBLREF	int4			forced_exit_err;
GBLREF	uint4			process_id;
GBLREF	gtmsiginfo_t		signal_info;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	boolean_t		exit_handler_active;
GBLREF	boolean_t		exit_handler_complete;
GBLREF	void			(*exit_handler_fptr)();
GBLREF	boolean_t		ydb_quiet_halt;
GBLREF	volatile int4		gtmMallocDepth;         /* Recursion indicator */
GBLREF	intrpt_state_t		intrpt_ok_state;
GBLREF	struct sigaction	orig_sig_action[];

LITREF	gtmImageName		gtmImageNames[];

error_def(ERR_FORCEDHALT);
error_def(ERR_KILLBYSIG);
error_def(ERR_KILLBYSIGSINFO1);
error_def(ERR_KILLBYSIGSINFO2);
error_def(ERR_KILLBYSIGSINFO3);
error_def(ERR_KILLBYSIGUINFO);

void deferred_exit_handler(void)
{
	char			*rname;
	intrpt_state_t		prev_intrpt_state;
	int			sig;
	siginfo_t		*info;
	gtm_sigcontext_t 	*context;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!INSIDE_THREADED_CODE(rname));	/* below code is not thread safe as it does EXIT() etc. */
	/* To avoid nested calls to this routine, progress the forced_exit state. */
	DBGSIGHND_ONLY(fprintf(stderr, "deferred_exit_handler: Entering deferred exit handling state - forced_exit_err: %d,  "
			       "forced_exit_sig: %d\n", forced_exit_err, forced_exit_sig); fflush(stderr));
	SET_FORCED_EXIT_STATE_ALREADY_EXITING;
	if (exit_handler_active)
	{
		assert(FALSE);	/* at this point in time (June 2003) there is no way we know of to get here, hence the assert */
		return;	/* since anyway we are exiting currently, resume exit handling instead of reissuing another one */
	}
	/* For signals that get a delayed response so we can get out of crit, we also delay the messages.
	 * This routine will output those delayed messages from the appropriate structures to both the
	 * user and the system console.
	 */
	if (ERR_FORCEDHALT != forced_exit_err || !ydb_quiet_halt) /* No HALT messages if quiet halt is requested */
		forced_exit_err_display();
	assert(OK_TO_INTERRUPT);
	/* Signal intent to exit BEFORE driving condition handlers. This avoids checks that will otherwise fail (for example
	 * if mdb_condition_handler/preemptive_db_clnup gets called below, that could invoke the RESET_GV_TARGET macro which in turn
	 * would assert that gv_target->gd_csa is equal to cs_addrs. This could not be true in case we were in mainline code
	 * that was interrupted by the flush timer for a different region which in turn was interrupted by an external signal
	 * that would drive us to exit. Setting the "process_exiting" variable causes those csa checks to pass.
	 */
	SET_PROCESS_EXITING_TRUE;
#	ifdef DEBUG
	if (ydb_white_box_test_case_enabled && (WBTEST_DEFERRED_TIMERS == ydb_white_box_test_case_number)
	    && (2 == ydb_white_box_test_case_count))
	{
		DEFER_INTERRUPTS(INTRPT_NO_TIMER_EVENTS, prev_intrpt_state);
		DBGFPF((stderr, "DEFERRED_SIGNAL_HANDLER: will sleep for 20 seconds\n"));
		LONG_SLEEP(20);
		DBGFPF((stderr, "DEFERRED_SIGNAL_HANDLER: done sleeping\n"));
		ENABLE_INTERRUPTS(INTRPT_NO_TIMER_EVENTS, prev_intrpt_state);
	}
#	endif
	/* If we are using alternate signal handling, retrieve the signal number from `sig_num` field which was
	 * stored using the ALTERNATE_SIGHANDLING_SAVE_SIGNUM macro in `generic_signal_handler.c`. This is because
	 * we would not have done a FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED invocation in that case.
	 */
	sig = (!USING_ALTERNATE_SIGHANDLING
		? stapi_signal_handler_oscontext[sig_hndlr_generic_signal_handler].sig_info.si_signo
		: stapi_signal_handler_oscontext[sig_hndlr_generic_signal_handler].sig_num);
	assert(sig);	/* Assert that we did store the signal number to be handled in a deferred fashion */
	info = &stapi_signal_handler_oscontext[sig_hndlr_generic_signal_handler].sig_info;
	context = &stapi_signal_handler_oscontext[sig_hndlr_generic_signal_handler].sig_context;
	signal_exit_handler("deferred_exit_handler", sig, info, context, IS_DEFERRED_EXIT_TRUE);	/* exits the process */
	return;
}
