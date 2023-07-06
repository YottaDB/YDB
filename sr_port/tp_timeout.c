/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* ------------------------------------------------------------------
 * 	Routines & data for managing TP timeouts
 * ------------------------------------------------------------------
 */

#include "mdef.h"

#include "gtm_stdio.h"

/* tp_timeout.h needs to be included to potentially define DEBUG_TPTIMEOUT_DEFERRAL */
#include "tp_timeout.h"
#ifdef DEBUG_TPTIMEOUT_DEFERRAL
#  include "gtm_time.h"
#endif

#include "gt_timer.h"
#include "xfer_enum.h"
#include "have_crit.h"
#include "deferred_events.h"
#include "deferred_events_queue.h"
#include "op.h"
#include "fix_xfer_entry.h"
#include "error_trap.h"
#include "gtm_time.h"
#include "io.h"
#include "gtmio.h"

#define TP_TIMER_ID (TID)&tp_start_timer
#define TP_QUEUE_ID &tptimeout_set

/* External variables */
GBLREF boolean_t			in_timed_tn, tp_timeout_set_xfer, ztrap_explicit_null;
GBLREF dollar_ecode_type		dollar_ecode;
GBLREF int				process_exiting;
GBLREF intrpt_state_t			intrpt_ok_state;
GBLREF volatile  boolean_t		dollar_zininterrupt;
GBLREF volatile int4			outofband;
GBLREF xfer_entry_t     		xfer_table[];

error_def(ERR_TPTIMEOUT);

void tptimeout_set(int4 dummy_param);
STATICFNDCL void tp_expire_now(void);

/* =============================================================================
 * FILE-SCOPE FUNCTIONS
 * =============================================================================
 */
/* ------------------------------------------------------------------
 * Timer handler (Set -> Expired)
 * - Should only happen if a timeout has been started (and not cancelled), and has not yet expired.
 * - Static because it's for internal use only.
 * ------------------------------------------------------------------
 */
STATICFNDEF void tp_expire_now(void)
{
	SHOWTIME(asccurtime);
	DBGDFRDEVNT((stderr, "%d %s %s: tp_expire_now: Driving xfer_set_handlers\n", __LINE__, __FILE__, asccurtime));
	assert(in_timed_tn);
	tp_timeout_set_xfer = xfer_set_handlers(tptimeout, 0, FALSE);
	DBGDFRDEVNT((stderr, "%d %s: tp_expire_now: tp_timeout_set_xfer: %d\n", __LINE__, __FILE__, tp_timeout_set_xfer));
}

/* ------------------------------------------------------------------
 * Set transfer table for synchronous handling of TP timeout.
 * Should be called only from set_xfer_handlers.
 * Notes:
 *  - Dummy parameter is for calling compatibility.
 */
void tptimeout_set(int4 dummy_param)
{
	intrpt_state_t	prev_intrpt_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(INTRPT_IN_EVENT_HANDLING == intrpt_ok_state);
	assert(tptimeout == outofband);
<<<<<<< HEAD
	if (dollar_zininterrupt || ((0 < dollar_ecode.index) && ETRAP_IN_EFFECT)
		|| (jobinterrupt == (TREF(save_xfer_root_ptr))->ev_que.fl->outofband))
=======
	if (dollar_zininterrupt || ((0 < dollar_ecode.index) && ETRAP_IN_EFFECT))
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
	{	/* Error handling or job interrupt is in effect - defer tp timeout until $ECODE is cleared and/or we have unrolled
		* the job interrupt frame.
		*/
		outofband = no_event;
		TAREF1(save_xfer_root, tptimeout).event_state = queued;
		SAVE_XFER_QUEUE_ENTRY(tptimeout, 0);
		SHOWTIME(asccurtime);
		DBGDFRDEVNT((stderr, "%d %s %s: tptimeout_set - pending entries for tptimout\n", __LINE__, __FILE__,
			asccurtime));
		return;
	}
	DBGDFRDEVNT((stderr, "%d %s %s: tptimeout_set: TP timeout *NOT* deferred - zinint: %d, ecindex: %d,  et: %d\n",
		__LINE__, __FILE__,asccurtime, dollar_zininterrupt, dollar_ecode.index, ETRAP_IN_EFFECT));
	if (queued == (TAREF1(save_xfer_root, tptimeout)).event_state)
		REMOVE_XFER_QUEUE_ENTRY(tptimeout);
	TAREF1(save_xfer_root, tptimeout).event_state = pending;
	outofband = tptimeout;
	DEFER_INTO_XFER_TAB;
	tp_timeout_set_xfer = TRUE;
	DBGDFRDEVNT((stderr, "%d %s: tptimeout_set - tptimeout outofband now pending\n", __LINE__, __FILE__));
	assert((pending == TAREF1(save_xfer_root, tptimeout).event_state)
		|| ((active == TAREF1(save_xfer_root, tptimeout).event_state)));
}

/* ------------------------------------------------------------------
 * Start timer
 * Note: timer handler data length and pointer are specified as 0 and null, respectively.
 * Handler parameter list should therefore probably be (... int4, char*), due to
 * how it's called from timer_handler(), but other invokers use 0 and null.
 * ------------------------------------------------------------------
 */
void tp_start_timer(int4 timeout_seconds)
{
	assert(!in_timed_tn);
	SHOWTIME(asccurtime);
	DBGDFRDEVNT((stderr, "%d %s %s: tp_start_timer - tptimeout: %d\n", __LINE__, __FILE__, asccurtime, timeout_seconds));
	in_timed_tn = TRUE;
	start_timer(TP_TIMER_ID, ((uint8)NANOSECS_IN_SEC * timeout_seconds), &tp_expire_now, 0, NULL);
}

/* ------------------------------------------------------------------
 * Transaction done, clear pending timeout:
* - Ok to call even if no timeout was set.
 * - Reset transfer table if expired and timeout was the reason.
 * - Resets expired flag AFTER cancelling the timer,
 *   in case the alarm expires just before it's cancelled.
 * Notes:
 * - Test for tp_timeout_set_xfer may obsolete use of conditional
 *   xfer_table reset function. If so, should change this routine
 *   to simply return value of tp_timeout_set_xfer when entered.
 * ------------------------------------------------------------------
 */
void tp_clear_timeout(void)
{
	boolean_t	already_ev_handling;
	intrpt_state_t	prev_intrpt_state;
	save_xfer_entry	*entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	SHOWTIME(asccurtime);
	/* WARNING! AIO sets multi_thread_in_use which disables DEFER_INTERRUPTS, treat it like an active event */
	if (!(already_ev_handling = ((INTRPT_IN_EVENT_HANDLING == intrpt_ok_state) || multi_thread_in_use)))
		DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
	DBGDFRDEVNT((stderr, "%d %s %s: tp_clear_timeout - clearing tptimeout while %sin timed tn\n", __LINE__, __FILE__,
		asccurtime, in_timed_tn ? "": "not "));
	if (in_timed_tn)
	{
		/* ------------------------------------------------
		 * Works whether or not timer already expired.
		 * ------------------------------------------------
		 */
		DBGDFRDEVNT((stderr, "%d %s: tptimeout_clear_timer - queued: %d\n", __LINE__, __FILE__,
			     TAREF1(save_xfer_root, tptimeout).event_state));
		cancel_timer(TP_TIMER_ID);
		entry = &TAREF1(save_xfer_root, tptimeout);
		if (queued == entry->event_state)
			REMOVE_XFER_QUEUE_ENTRY(tptimeout);
#		ifdef DEBUG
		if (pending == entry->event_state)
		{
			if (ydb_white_box_test_case_enabled && (WBTEST_ZTIM_EDGE == ydb_white_box_test_case_number))
				DBGFPF((stderr, "%d %s: tp_clear_timeout - resetting the xfer entries for tptimeout\n",
					__LINE__, __FILE__));
		}
#		endif
		/* For unambiguous state, clear this flag after cancelling timer and before clearing expired flag */
		in_timed_tn = FALSE;
		DBGDFRDEVNT((stderr, "%d %s: tptimeout_clear_timer - clearing in_timed_tn: %d\n",
			     __LINE__, __FILE__, entry->event_state));
		/* -----------------------------------------------------
		 * Should clear xfer settings only if set them.
		 * -----------------------------------------------------
		 */
		if (tp_timeout_set_xfer)
		{	/* ------------------------------------------------
			 * Get here only:
			 * - if set xfer_table due to timer pop.
			 * - if timeout is aborting transaction, or
			 * - if committing and timer popped too late to stop it => want to undo xfer_table change
			 * --------------------------------------------
			 */
			entry->event_state = active;
			(void)xfer_reset_if_setter(tptimeout);
			SHOWTIME(asccurtime);
			DBGDFRDEVNT((stderr, "%d %s %s: tp_clear_timeout - timer already popped - clearing driver indicator\n",
				__LINE__, __FILE__, asccurtime));
			tp_timeout_set_xfer = FALSE;
		} else
		{	/* ----------------------------------------------------
			 * Get here only if clearing TP timer when a
			 * TP timeout did not set xfer_table.
			 * Examples:
			 *   -- Before timer popped, via op_tcommit, if successfully committing a timed transaction.
			 *      NOTE: other events could be pending, if they occurred late in transaction.
			 *   -- After timer popped, also via op_tcommit, if successfully committing a timed transaction
			 *       and another event occurred first (but too late in transaction to abort it).
			 *   -- Before or after timer popped, if another event (e.g. ^C) happened first,
			 *       and now that event is clearing TP timer to prevent timer pop in M handler before it can TROLLBACK
			 *       via op_trollback(), whether in user code or otherwise (e.g. at exit).		BYPASSOK
			 * ----------------------------------------------------
			 * (There's nothing to do) */
		}
		entry->event_state = not_in_play;
	}
	if (!already_ev_handling)
		ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
}

void tp_timeout_action(void)
{	/* driven at tp timeout recognition point by async_action() */
	intrpt_state_t	prev_intrpt_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(INTRPT_IN_EVENT_HANDLING != intrpt_ok_state);
	DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
	SHOWTIME(asccurtime);
	DBGDFRDEVNT((stderr, "%d %s %s: tp_timeout_action - driving TP timeout error\n", __LINE__, __FILE__, asccurtime));
	assert((active == TAREF1(save_xfer_root, tptimeout).event_state)
		|| (pending == TAREF1(save_xfer_root, tptimeout).event_state));
	tp_clear_timeout();
	if (dollar_zininterrupt)
	{	/* safety play */
		assert(!dollar_zininterrupt);
		assert(active == TAREF1(save_xfer_root, jobinterrupt).event_state);
		TAREF1(save_xfer_root, jobinterrupt).event_state = not_in_play;
		dollar_zininterrupt = FALSE;
	}
	TAREF1(save_xfer_root, tptimeout).event_state = not_in_play;
	DBGDFRDEVNT((stderr, "%d %s: tp_timeout_action - changing pending to event_state: %d\n", __LINE__, __FILE__,
		TAREF1(save_xfer_root, tptimeout).event_state));
	ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
	RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_TPTIMEOUT);
}
