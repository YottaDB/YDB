/****************************************************************
 *								*
 * Copyright (c) 2018-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "gtm_common_defs.h"
#include "mdef.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "deferred_events.h"
#include "error_trap.h"
#include "mdq.h"
#ifdef DEBUG
#include "compiler.h"
#endif

GBLREF	boolean_t		ztrap_explicit_null;
GBLREF	dollar_ecode_type	dollar_ecode;
GBLREF	volatile boolean_t	dollar_zininterrupt;

void set_events_from_signals(intrpt_state_t prev_intrpt_state)
{	/* act on signaled events stored in their event record while the event mechanism had a lock on the intrpt_ok state */
	int4		event_type;
	save_xfer_entry	*entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((INTRPT_IN_EVENT_HANDLING == intrpt_ok_state) && (INTRPT_NUM_STATES > prev_intrpt_state));
	for (event_type=1; event_type < DEFERRED_EVENTS; event_type++)
	{
		entry = &TAREF1(save_xfer_root, event_type);
		if ((signaled == entry->event_state) && (!dollar_zininterrupt || (jobinterrupt != event_type)))
		{
			xfer_set_handlers(event_type, entry->param_val, FALSE);
			DBGDFRDEVNT((stderr, "%d %s: set_events_from_signals - event type: %d, signaled: %d\n",
				__LINE__, __FILE__, event_type));
		}
	}
	ENABLE_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
}

void save_xfer_queue_entry(int4 event_type, int4 param_val)
{	/* queue an entry; note jobinterrupt goes to the head of the queue  */
	save_xfer_entry	*entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assertpro(DEFERRED_EVENTS > event_type);
	assert(NULL != TREF(save_xfer_root_ptr));
	assert(queued == TAREF1(save_xfer_root, event_type).event_state);
	entry = &(TAREF1(save_xfer_root, event_type));
	entry->param_val = param_val;
	DBGDFRDEVNT((stderr, "%d %s: save_xfer_queue_entry adding new node for %d.\n", __LINE__, __FILE__, event_type));
	if ((jobinterrupt == event_type) || ((tptimeout == event_type)
		&& (ztimeout == (TREF(save_xfer_root_ptr))->ev_que.fl->outofband)))
		dqins((TREF(save_xfer_root_ptr)), ev_que, entry);
	else
		dqrins((TREF(save_xfer_root_ptr)), ev_que, entry);
	assert(no_event != (TREF(save_xfer_root_ptr))->ev_que.fl->outofband);
#	ifdef DEBUG_DEFERRED_EVENT
	DBGDFRDEVNT((stderr, "%d %s: save_xfer_queue_entry outofband: %d, set_fn: %X, param_val: %d, entry: %X, ptr->fl: %X\n",
		__LINE__, __FILE__, entry->outofband, entry->set_fn, entry->param_val, &entry,
		(TREF(save_xfer_root_ptr))->ev_que.fl));
	scan_xfer_queue_entries(FALSE);
#	endif
	return;
}

void pop_real_xfer_queue_entry(int4* event_type, int4* param_val)
{	/* pop a event queue entry */
	save_xfer_entry	*entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (no_event != (*event_type = (TREF(save_xfer_root_ptr))->ev_que.fl->outofband))		/* WARNING: assignment */
	{
		entry = (TREF(save_xfer_root_ptr))->ev_que.fl;
		dqdel(entry, ev_que);
		*param_val = entry->param_val;
		if (*event_type == (TREF(save_xfer_root_ptr))->ev_que.fl->outofband)
		{	/* This should not happen, but it did during testing. Fix it for PRO */
			assert((TREF(save_xfer_root_ptr))->ev_que.fl == (TREF(save_xfer_root_ptr))->ev_que.bl);
			assert(entry == (TREF(save_xfer_root_ptr))->ev_que.fl);
			assert(FALSE); 									/* fix it in pro */
			(TREF(save_xfer_root_ptr))->ev_que.fl = (TREF(save_xfer_root_ptr))->ev_que.bl = TREF(save_xfer_root_ptr);
		}
	}
	DBGDFRDEVNT((stderr, "%d %s: pop_real_xfer_queue_entry: %d\n", __LINE__, __FILE__, *event_type));
	assertpro(DEFERRED_EVENTS > *event_type);
}

void pop_xfer_queue_entry(int4* event_type, int4* param_val)
{	/* wrapper for pop_real_xfer_queue_entry that deals with the juggling of timeouts with respect to jobinterrupts*/
	boolean_t	defer_tptimeout, defer_ztimeout;
	int4		next_event;
	save_xfer_entry	*entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef DEBUG_DEFERRED_EVENT
	scan_xfer_queue_entries(FALSE);
	DBGDFRDEVNT((stderr, "%d %s: dzinin: %d, dec_idx: %d, dztr_null: %d, dztr_len: %d\n", __LINE__, __FILE__,
		     dollar_zininterrupt, dollar_ecode.index, ztrap_explicit_null, (TREF(dollar_ztrap)).str.len));
#	endif
	*event_type = no_event;
	if (dollar_zininterrupt || ((0 != dollar_ecode.index) && (ETRAP_IN_EFFECT)))
	{	/* conditions indicate tptimeout and ztimeout should remain deferred */
		for (next_event = no_event, defer_tptimeout = defer_ztimeout = FALSE; DEFERRED_EVENTS > next_event; next_event++)
		{	/* don't pend tptimeout or ztimeout it they should remain deferred */
			pop_real_xfer_queue_entry(event_type, param_val);
			DBGDFRDEVNT((stderr, "%d %s: pop_reset_xfer returned event %d\n", __LINE__, __FILE__, *event_type));
			switch (*event_type)
			{
			case tptimeout:
				defer_tptimeout = TRUE;
				TAREF1(save_xfer_root, tptimeout).event_state = queued;
				continue;
			case ztimeout:
				defer_ztimeout = TRUE;
				TAREF1(save_xfer_root, ztimeout).event_state = queued;
				continue;
			case jobinterrupt:
				if (dollar_zininterrupt)
					continue;					/* WARNING: possible fallthrough */
			default:
				next_event = DEFERRED_EVENTS;	/* leave loop */
				DBGDFRDEVNT((stderr, "%d %s: pop_xfer_queue_entry %d\n", __LINE__, __FILE__, *event_type));
				break;					/* found other event behind one or both timers */
			}
		}
		/* if we popped either or both of these, put them back; perhaps swapped, but OK to give priority to the tptimer*/
		if (defer_tptimeout)
		{
			SAVE_XFER_QUEUE_ENTRY(tptimeout, (TAREF1(save_xfer_root, tptimeout)).param_val);
			DBGDFRDEVNT((stderr, "%d %s: requeued event %d\n", __LINE__, __FILE__, tptimeout));
		}
		if (defer_ztimeout)					/* should be behind any tptimeout */
		{
			SAVE_XFER_QUEUE_ENTRY(ztimeout, (TAREF1(save_xfer_root, ztimeout)).param_val);
			DBGDFRDEVNT((stderr, "%d %s: requeued event %d\n", __LINE__, __FILE__, ztimeout));
		}
	} else
	{	/* things are straightforward */
		pop_real_xfer_queue_entry(event_type, param_val);
		DBGDFRDEVNT((stderr, "%d %s: pop_reset_xfer returned event %d\n", __LINE__, __FILE__, *event_type));
	}
	return;
}

void remove_xfer_queue_entry(int4 event_type)
{	/* given an event_type, if it's in the queue, remove it */
	save_xfer_entry *entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(no_event != event_type);
	if ((TREF(save_xfer_root_ptr))->ev_que.fl != TREF(save_xfer_root_ptr))
	{	/* something queued */
		entry = NULL;
		dqloop(TREF(save_xfer_root_ptr), ev_que, entry)
		{
			assert(NULL != entry);
			if (entry->outofband == event_type)
			{	/* it's the one we're after */
				DBGDFRDEVNT((stderr, "%d %s: remove_xfer_queue_entry event_type: %d\n",
					     __LINE__, __FILE__, event_type));
				dqdel(entry, ev_que);
				break;
			}
			assert((entry != entry->ev_que.fl) && (entry != entry->ev_que.bl));
			if ((entry == entry->ev_que.fl) && (entry == entry->ev_que.bl))
			{	/* WARNING: The current entry's forward and back links point back to itself. The queue
				 * is not well formed. Assume that this the final entry in the queue and repoint the
				 * forward and back links to TREF(save_xfer_root_ptr). For now, we fix this situation
				 * in PRO and assert fail in DBG. */
				assert(FALSE);
				entry->ev_que.fl = entry->ev_que.bl = TREF(save_xfer_root_ptr);
			}
		}
	}
}

void scan_xfer_queue_entries(boolean_t check4players)
{	/* debug-only state checker */
	save_xfer_entry	*entry;
	int		event_type;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#ifdef	DEBUG
	DBGDFRDEVNT((stderr, "%d %s: scan_xfer_queue_entries in play:\n", __LINE__, __FILE__));
	for (event_type=no_event; event_type < DEFERRED_EVENTS; event_type++)
	{
		entry = &TAREF1(save_xfer_root, event_type);
		if (not_in_play != entry->event_state)
		{
			DBGDFRDEVNT((stderr, "%d %s: event type: %d, state: %d\n",__LINE__, __FILE__,
				     event_type, entry->event_state));
			if (check4players)
				assert(not_in_play == TAREF1(save_xfer_root, event_type).event_state);
		}
	}
	DBGDFRDEVNT((stderr, "%d %s: scan_xfer_queue_entries queued:\n", __LINE__, __FILE__));
	dqloop((TREF(save_xfer_root_ptr)), ev_que, entry)
	{
		DBGDFRDEVNT((stderr, "%d %s: scan_xfer_queue_entries - event type: %d\n",
			     __LINE__, __FILE__, entry->outofband));
	}
#	else
	assertpro(FALSE);					/* at this point this routine is just used for debug mode */
#	endif
	return;
}

void empty_xfer_queue_entries(void)
{	/* clear the queue */
	save_xfer_entry	*entry;
	DCL_THREADGBL_ACCESS;

	DBGDFRDEVNT((stderr, "%d %s: empty_xfer_queue_entries event_type: %d\n", __LINE__, __FILE__, entry->outofband));
	SETUP_THREADGBL_ACCESS;
	dqloop((TREF(save_xfer_root_ptr)), ev_que, entry)
	{
		if (not_in_play == entry->event_state)
		DBGDFRDEVNT((stderr, "%d %s: empty_xfer_queue_entries event_type: %d event_state: %d\n", __LINE__, __FILE__,
			entry->outofband, entry->event_state));
		assert(queued == entry->event_state);
		dqdel(entry, ev_que);
		entry->event_state = not_in_play;
	}
}
