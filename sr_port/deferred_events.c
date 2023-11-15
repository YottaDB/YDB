/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "xfer_enum.h"
#include "tp_timeout.h"
#include "deferred_events.h"
#include "interlock.h"
#include "lockconst.h"
#include "add_inter.h"
#include "op.h"
#include "fix_xfer_entry.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "stack_frame.h"
#include "error.h"

/* =============================================================================
 * EXTERNAL VARIABLES
 * =============================================================================
 */

GBLREF boolean_t		is_tracing_on;				/* M profiling */
GBLREF boolean_t		tp_timeout_set_xfer;
GBLREF intrpt_state_t		intrpt_ok_state;
GBLREF stack_frame		*frame_pointer;
GBLREF volatile boolean_t	dollar_zininterrupt;
GBLREF volatile int4		fast_lock_count, outofband;		/* fast_lock_count protects some non-reentrant code */
GBLREF xfer_entry_t		xfer_table[];				/* transfer table */

/* ----------------------------------------------------------------------------
 * Establish only first received; queue others; discard multiples of the same
 * ----------------------------------------------------------------------------
 */
/* =============================================================================
 * EXPORTED FUNCTIONS
 * =============================================================================
 */
/* ------------------------------------------------------------------
 * *** INTERRUPT HANDLER ***
 * Sets up transfer table changes needed for:
 *   - Synchronous handling of asynchronous events.
 *   - Single-stepping and breakpoints
 * Parameters:
 *   - event_type specifies the event being deferred
 *   - param_val to store for in the event's array element for use by the corresponding event handler function when it gets to run
 *   - popped_entry to indicate whether the even has just been popped from the event queue
 *
 *   - the return value is TRUE if the event has been successfully set up, including if it is redundant, i.e. already set up
 * ------------------------------------------------------------------
 */
boolean_t xfer_set_handlers(int4  event_type, int4 param_val, boolean_t popped_entry)
{	/* Keep track of what event types have come in and deal with them appropriately */
	boolean_t	already_ev_handling;
	int4		e_type, pv;
	intrpt_state_t	prev_intrpt_state;
	save_xfer_entry	*entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assertpro(DEFERRED_EVENTS > event_type);
	entry = &TAREF1(save_xfer_root, event_type);
	if ((INTRPT_IN_EVENT_HANDLING == intrpt_ok_state) && !popped_entry && (no_event != event_type))
	{	/* events already in flux - stash this as in the record for this event */
		if (not_in_play == entry->event_state)
			entry->event_state = signaled;
		return TRUE;						/* currently only used by tp_timeout.c */
	}
	/* WARNING! AIO sets multi_thread_in_use which disables DEFER_INTERRUPTS, treat it like an active event */
	if (!(already_ev_handling = ((INTRPT_IN_EVENT_HANDLING == intrpt_ok_state) || multi_thread_in_use)))
		DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);	/* ensure ownership of the event mechanism */
	if (dollar_zininterrupt && (jobinterrupt == event_type))
	{	/* ignore jobinterrupt flooding */
		real_xfer_reset(jobinterrupt);
		TAREF1(save_xfer_root, jobinterrupt).event_state = not_in_play;
		if (!already_ev_handling)
			ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
		return TRUE;
	}
	if (queued == entry->event_state)
	{	/* Fast path, if not already handling an event, from queue to pending for event already popped from the queue,
		 * but still in the queued state
		 */
		if (!popped_entry && (no_event == outofband) && (!already_ev_handling))
		{	/* this event is at the head of the queue so pop it here */
			POP_XFER_QUEUE_ENTRY(&e_type, &pv);
			DBGDFRDEVNT((stderr, "%d %s: xfer_set_handlers - popped: %d with state: %d\n", __LINE__, __FILE__,
				     e_type, TAREF1(save_xfer_root, e_type).event_state));
			if (e_type != event_type)
			{
				if (not_in_play != TAREF1(save_xfer_root, e_type).event_state)
				{
					assert(queued == TAREF1(save_xfer_root, event_type).event_state);
					event_type = e_type;
				} else
					assert(FALSE);
			}
		}
		DBGDFRDEVNT((stderr, "%d %s: xfer_set_handlers - %sevent_type = %d\n", __LINE__, __FILE__,
			     popped_entry ? "popped " : "", event_type));
		assert(!dollar_zininterrupt || (jobinterrupt != event_type));
		if (entry != (TREF(save_xfer_root_ptr))->ev_que.fl)
		{	/* no event in play so pend this one by jiggeriing the xfer_table */
			entry->event_state = pending;
			outofband = event_type;
			entry->set_fn(param_val);
			DBGDFRDEVNT((stderr, "%d %s: xfer_set_handlers - xfer_table active for event type %d\n", __LINE__, __FILE__,
			     event_type));
		} else
			DBGDFRDEVNT((stderr, "%d %s: xfer_set_handlers - skipping [re]queued event type %d\n", __LINE__, __FILE__,
				event_type));
		if (!already_ev_handling)
			ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
		return TRUE;
	}
	if (not_in_play != entry->event_state)
	{	/* each event only gets one chance at a time */
		DBGDFRDEVNT((stderr, "%d %s: xfer_set_handlers - already in process event: %d with state: %d\n",
			     __LINE__, __FILE__,event_type, entry->event_state));
		if (!already_ev_handling)
			ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
		return TRUE;
	}
	if ((no_event == outofband) && (!dollar_zininterrupt || ((tptimeout != event_type) && (ztimeout != event_type)))
			&& (!already_ev_handling))
	{	/* no competion or blocking interrupt: collect $200 and go straight to pending */
		/* -------------------------------------------------------
		 * If table changed, it was not synchronized.
		 * (Assumes these entries are all that would be changed)
		 * --------------------------------------------------------
		 */
		assert((xfer_table[xf_linefetch] == op_linefetch) ||
		       (xfer_table[xf_linefetch] == op_zstepfetch) ||
		       (xfer_table[xf_linefetch] == op_zst_fet_over) ||
		       (xfer_table[xf_linefetch] == op_mproflinefetch));
		assert((xfer_table[xf_linestart] == op_linestart) ||
		       (xfer_table[xf_linestart] == op_zstepstart) ||
		       (xfer_table[xf_linestart] == op_zst_st_over) ||
		       (xfer_table[xf_linestart] == op_mproflinestart));
		assert((xfer_table[xf_zbfetch] == op_zbfetch) ||
		       (xfer_table[xf_zbfetch] == op_zstzb_fet_over) ||
		       (xfer_table[xf_zbfetch] == op_zstzbfetch));
		assert((xfer_table[xf_zbstart] == op_zbstart) ||
		       (xfer_table[xf_zbstart] == op_zstzb_st_over) ||
		       (xfer_table[xf_zbstart] == op_zstzbstart));
		assert((xfer_table[xf_forchk1] == op_forchk1) ||
		       (xfer_table[xf_forchk1] == op_mprofforchk1));
		assert((xfer_table[xf_forloop] == op_forloop));
		assert(xfer_table[xf_ret] == opp_ret ||
		       xfer_table[xf_ret] == opp_zst_over_ret ||
		       xfer_table[xf_ret] == opp_zstepret);
		assert(xfer_table[xf_retarg] == op_retarg ||
		       xfer_table[xf_retarg] == opp_zst_over_retarg ||
		       xfer_table[xf_retarg] == opp_zstepretarg);
		/* -----------------------------------------------
		 * Now call the specified set function to swap in
		 * the desired handlers (and set flags or whatever).
		 * -----------------------------------------------
		 */
		assert(entry != (TREF(save_xfer_root_ptr))->ev_que.fl);
		assert(!dollar_zininterrupt || (jobinterrupt != event_type));
		entry->event_state = pending;				/* jiggering the transfer table for this event */
		outofband = event_type;
		entry->set_fn(param_val);
		DBGDFRDEVNT((stderr, "%d %s: xfer_set_handlers - set xfer_table for event type %d\n" ,__LINE__, __FILE__,
			     event_type));
	} else if (queued != entry->event_state)
	{	/* queue it */
		entry->event_state = queued;
		SAVE_XFER_QUEUE_ENTRY(event_type, param_val);
		if (outofband == event_type)
			outofband = no_event;
		DBGDFRDEVNT((stderr, "%d %s: xfer_set_handlers: event %d queued %s %d\n",__LINE__, __FILE__, event_type,
			((jobinterrupt == event_type) ? "ahead of" : "behind"), outofband));
	}
	if (!already_ev_handling)
		ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
	return (no_event != outofband);
}

boolean_t xfer_reset_if_setter(int4 event_type)
{	/* reset the state and potentially the transfer table for a particular event that needs canceling */
	boolean_t	already_ev_handling, res;
	intrpt_state_t	prev_intrpt_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* WARNING! AIO sets multi_thread_in_use which disables DEFER_INTERRUPTS, treat it like an active event */
	if (!(already_ev_handling = ((INTRPT_IN_EVENT_HANDLING == intrpt_ok_state) || multi_thread_in_use)))
		DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
	if ((event_type == outofband) || ((no_event == outofband) && (tptimeout == event_type ? tp_timeout_set_xfer
		: (jobinterrupt == event_type) ? dollar_zininterrupt : (ztimeout == event_type ? TRUE
		: (ctrlc == event_type) ? TRUE : FALSE))))
	{
		DBGDFRDEVNT((stderr, "%d %s: xfer_reset_if_setter: event_type %d is first\n", __LINE__, __FILE__, event_type));
		if (res = (active == TAREF1(save_xfer_root, event_type).event_state))			/* WARNING: assignment */
		{
			assert(res);
			res = (real_xfer_reset(event_type));
		}
		DBGDFRDEVNT((stderr, "%d %s: xfer_reset_if_setter: xfer_reset_handlers returned %d\n", __LINE__, __FILE__, res));
		return res;
	}
	DBGDFRDEVNT((stderr, "%d %s: xfer_reset_if_setter: event_type %d is but pending event is %d\n", __LINE__, __FILE__,
		event_type, outofband));
	if (!already_ev_handling)
		ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
	return FALSE;
}

boolean_t xfer_reset_handlers(int4 event_type)
{	/* but individual cases and states may get special treatment */
	boolean_t	to_queue;
	int 		e_type;
	int4		state;
	intrpt_state_t	prev_intrpt_state;
	save_xfer_entry	*entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
	assert(INTRPT_IN_EVENT_HANDLING != prev_intrpt_state);
#	ifdef DEBUG_DEFERRED_EVENT
	DBGDFRDEVNT((stderr, "%d %s: xfer_reset_handlers - event: %d\n", __LINE__, __FILE__, event_type));
	scan_xfer_queue_entries(FALSE);
#	endif
	assert(outofband == event_type);
	if ((event_type != outofband) && (not_in_play == TAREF1(save_xfer_root, event_type).event_state)) /* term 1 protects pro */
	{	/* no need if state shows the invoking event has no current interest in the xfer table */
		ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
		return FALSE;
	}
	for (e_type = no_event + 1; DEFERRED_EVENTS > e_type; e_type++)
	{
		entry = &TAREF1(save_xfer_root, e_type);
		to_queue = ((jobinterrupt == event_type) && ((tptimeout == e_type) || (ztimeout == e_type)));
		state = entry->event_state;
		switch (state)
		{
		case pending:
			if (zstep_pending == e_type)
				continue;	/* zsteps tend to repeat unless specifically turned off */
		case active:
			if (!to_queue)
				real_xfer_reset(e_type);	/* WARNING: fallthrough */
		case signaled:
			if (!to_queue)
				entry->event_state = not_in_play;
			else
			{	/* if clearing jobinterrupt we leave (z & tp) timeouts queued until that event is over */
				SAVE_XFER_QUEUE_ENTRY(e_type, TAREF1(save_xfer_root, e_type).param_val);
				entry->event_state = queued;
			}					/* WARNING fallthrough */
		case queued:
			break;
		default:
			assertpro(num_event_states > state);
		}
		assert((not_in_play == entry->event_state) || (queued == entry->event_state));
		if (!to_queue)
		{	/* when resetting doesn't leave queued == event state alone */
			REMOVE_XFER_QUEUE_ENTRY(e_type);
			entry->event_state = not_in_play;
		}
	}
	assert(not_in_play == TAREF1(save_xfer_root, event_type).event_state);
	TAREF1(save_xfer_root, ctrap).param_val = 0;
#	ifdef DEBUG_DEFERRED_EVENT
	DBGDFRDEVNT((stderr, "%d %s: xfer_reset_handlers - event: %d\n", __LINE__, __FILE__, event_type));
	scan_xfer_queue_entries(FALSE);
#	endif
	ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
	return TRUE;
}

/* ------------------------------------------------------------------
 * Reset transfer table to normal settings.
 *
 * - Intent: Put back all state that was or could have been changed
 *   due to prior deferral(s).
 *    - Might be preferable to implement this assumption if this routine
 *      were changed to delegate responsibility as does the
 *      corresponding set routine.
 * - If M profiling is active, some entries should be set to the
 *       op_mprof* routines.
 * - Return value indicates whether reset type matches set type.
 *   If it does not, this indicates an "abnormal" path.
 *    - Should still reset the table in this case.
* ------------------------------------------------------------------
 */
boolean_t real_xfer_reset(int4 event_type)
{
	boolean_t	already_ev_handling, cur_outofband;
	int 		e_type;
	int4		param_val, status;
	intrpt_state_t	prev_intrpt_state;
	save_xfer_entry	*entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* ------------------------------------------------------------------
	 * Note: If reset routine can preempt path from handler to
	 * set routine (e.g. clearing event before acting on it),
	 * these assertions can fail.
	 * Should not happen in current design.
	 * ------------------------------------------------------------------
	 */
	/* WARNING! AIO sets multi_thread_in_use which disables DEFER_INTERRUPTS, treat it like an active event */
	if (!(already_ev_handling = ((INTRPT_IN_EVENT_HANDLING == intrpt_ok_state) || multi_thread_in_use)))
		DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
	assert(no_event != event_type);
	DBGDFRDEVNT((stderr, "%d %s: real_xfer_reset - event: %d\n", __LINE__, __FILE__, event_type));
	assertpro(DEFERRED_EVENTS > event_type);
	assert(pending <= TAREF1(save_xfer_root, event_type).event_state);
	if ((pending == TAREF1(save_xfer_root, zstep_pending).event_state) && (TREF(zstep_action)).str.len)
	{
		(TREF(zstep_action)).mvtype = MV_STR;
		DEBUG_ONLY(cur_outofband = outofband);
		op_zstep(TAREF1(save_xfer_root, zstep_pending).param_val, &(TREF(zstep_action)));	/* reinstate ZSTEP */
		assert(outofband == cur_outofband);
		FIX_XFER_ENTRY(xf_forchk1, op_forchk1);				/* zstep does not mess with or use xf_forchk1 */
	} else
	{
		DEFER_OUT_OF_XFER_TAB(is_tracing_on);
		FIX_XFER_ENTRY(xf_ret, opp_ret);
		FIX_XFER_ENTRY(xf_retarg, op_retarg);
	}
	FIX_XFER_ENTRY(xf_forloop, op_forloop);
	TAREF1(save_xfer_root, event_type).event_state = not_in_play;
	DBGDFRDEVNT((stderr, "%d %s: real_xfer_reset cleared event_type: %d from event_state active to %d\n", __LINE__, __FILE__,
		     event_type, TAREF1(save_xfer_root, event_type).event_state));
	REMOVE_XFER_QUEUE_ENTRY(event_type);
	outofband = no_event;
	TAREF1(save_xfer_root, event_type).event_state = not_in_play;
	if (!already_ev_handling)
		ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
	/* -------------------------------------------------------------------------
	 * Reset external event modules that need it.
	 * (Should do this in a more modular fashion.)
	 * None
	 * -------------------------------------------------------------------------
	 */
	return TRUE;
}

/* ------------------------------------------------------------------
 * Perform action corresponding to the first async event that
 * was logged.
 * ------------------------------------------------------------------
 */
/* This function can be invoked by op_*intrrpt* transfer table functions or by long-running functions that check for pending events.
 * The transfer table adjustments should be active only for a short duration between the occurrence of an outofband event
 * and the handling of it at a logical boundary where we have a captured mpc to allow and appropriate return to normal execution.
 * We don't expect to be running with those transfer table adjustmentss for more than one M-line. If "outofband" is set to 0, the
 * call to"outofband_action" below will do nothing and we will end up running with the op_*intrrpt* transfer table functions
 * indefinitely. In this case M-FOR loops are known to return incorrect results which might lead to application integrity issues.
 * It is therefore safer to assertpro, as we will at least have the core for analysis.
 */
void async_action(bool lnfetch_or_start)
{
	intrpt_state_t	prev_intrpt_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(INTRPT_IN_EVENT_HANDLING != intrpt_ok_state);
	DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
	if (jobinterrupt == outofband)
	{
		if (dollar_zininterrupt)
		{	/* This moderately deparate hack deals with interrupt flooding creeping through little state windows */
			assert(active == TAREF1(save_xfer_root, outofband).event_state);
			real_xfer_reset(jobinterrupt);
			assert(FALSE);
			MUM_TSTART;
		}
		TAREF1(save_xfer_root, jobinterrupt).event_state = pending;	/* jobinterrupt gets a pass from the assert below */
	} else if (!lnfetch_or_start)
	{	/* something other than a new line caugth this, so  */
		assert(pending >= TAREF1(save_xfer_root, outofband).event_state);
		TAREF1(save_xfer_root, outofband).event_state = pending;	/* make it pending in case it was not there yet */
	}
	DBGDFRDEVNT((stderr, "%d %s: async_action - pending event: %d active\n", __LINE__, __FILE__, outofband));
	assertpro(DEFERRED_EVENTS > outofband);
	ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);				/* opening a window of race */
	switch (outofband)
	{
		case jobinterrupt:
			dollar_zininterrupt = TRUE;	/* do this at every point to minimize nesting; WARNING: fallthrough*/
		case ctrlc:							/* these go from pending to active here */
		case ctrap:
		case sighup:
		case (ttwriterr):
			TAREF1(save_xfer_root, outofband).event_state = active;			/*WARNING: fallthrough */
		case tptimeout:					/* these have their own action routines that do pending -> active */
		case ztimeout:
		case deferred_signal:
			outofband_action(lnfetch_or_start);	/* which effectively does assertpro(no_event == outofband) */
			break;
		case (neterr_action):
			/* network error not implemented here yet - might should move from mdb_condition_handler */
		case (zstep_pending):
		case (zbreak_pending):
			assert(FALSE);	/* ZStep/Zbreak events not not really asynchronous, so they don't come here */
		case no_event:
			/* if table changed, it was not synchronized (assumes these entries are all that are be changed) */
			DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
			if (((xfer_table[xf_linefetch] == op_linefetch)
				|| (xfer_table[xf_linefetch] == op_zstepfetch)
				|| (xfer_table[xf_linefetch] == op_zst_fet_over)
				|| (xfer_table[xf_linefetch] == op_mproflinefetch))
				&& ((xfer_table[xf_linestart] == op_linestart)
				|| (xfer_table[xf_linestart] == op_zstepstart)
				|| (xfer_table[xf_linestart] == op_zst_st_over)
				|| (xfer_table[xf_linestart] == op_mproflinestart))
				&& ((xfer_table[xf_zbfetch] == op_zbfetch)
				|| (xfer_table[xf_zbfetch] == op_zstzb_fet_over)
				|| (xfer_table[xf_zbfetch] == op_zstzbfetch))
				&& ((xfer_table[xf_zbstart] == op_zbstart)
				|| (xfer_table[xf_zbstart] == op_zstzb_st_over)
				|| (xfer_table[xf_zbstart] == op_zstzbstart))
				&& ((xfer_table[xf_forchk1] == op_forchk1)
				|| (xfer_table[xf_forchk1] == op_mprofforchk1))
				&& (xfer_table[xf_forloop] == op_forloop)
				&& ((xfer_table[xf_ret] == opp_ret)
				|| (xfer_table[xf_ret] == opp_zst_over_ret)
				|| (xfer_table[xf_ret] == opp_zstepret))
				&& ((xfer_table[xf_retarg] == op_retarg)
				|| (xfer_table[xf_retarg] == opp_zst_over_retarg)
				|| (xfer_table[xf_retarg] == opp_zstepretarg)))
			{
				DEBUG_ONLY(scan_xfer_queue_entries(TRUE));	/* verify all not_in_play event states */
				ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
				return;	/* there was no event, but the transfer table does not present any either */
			}							/* WARNING: potential fallthrough */
		default:
			assertpro(FALSE && outofband);	/* see above comment for why this is needed */
	}
}
