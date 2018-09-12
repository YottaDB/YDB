/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef VMS
# include "efn.h"
# include <ssdef.h>
#endif

#include "xfer_enum.h"
#include "tp_timeout.h"
#include "deferred_events.h"
#include "outofband.h"
#include "interlock.h"
#include "lockconst.h"
#include "add_inter.h"
#include "op.h"
#include "iott_wrterr.h"
#ifdef DEBUG_DEFERRED_EVENT
# include "gtm_stdio.h"
#endif
#include "fix_xfer_entry.h"

/* =============================================================================
 * EXTERNAL VARIABLES
 * =============================================================================
 */

/* The transfer table */
GBLREF xfer_entry_t     xfer_table[];
/* M Profiling active */
GBLREF	boolean_t	is_tracing_on;
/* Marks sensitive database operations */
GBLREF volatile int4	fast_lock_count;
#if defined(VMS)
GBLREF volatile short   num_deferred;
#else
GBLREF volatile int4    num_deferred;
#endif
GBLREF volatile int4	ctrap_action_is, outofband;

/* =============================================================================
 * FILE-SCOPE VARIABLES
 * =============================================================================
 */

/* ------------------------------------------------------------------
 * Declared volatile because accesses occur both in main thread
 * and (possibly multiple) interrupts levels.
 * ------------------------------------------------------------------
 */

/* Holds count of events logged of each type.
 * Cleared when table is reset.
 * Location zero (== no_event) is not used.
 */

/* INCR_CNT on VMS doesn't return the post-incremented value as is the
 * case in Unix. But we don't need an interlocked add in VMS since we
 * should be in an AST and AST's can't be nested (we assert to that effect
 * in xfer_set_handlers). The macro INCR_CNT_SP accomplishes this task for us.
 */
#if defined(UNIX)
volatile int4 			xfer_table_events[DEFERRED_EVENTS];
#define	INCR_CNT_SP(X,Y)	INCR_CNT(X,Y)
#elif defined(VMS)
volatile short 			xfer_table_events[DEFERRED_EVENTS];
#define INCR_CNT_SP(X,Y)	(++*X)
#else
# error "Unsupported Platform"
#endif
GBLREF  global_latch_t		defer_latch;

/* -------------------------------------------------------
 * Act only on first recieved.
 * -------------------------------------------------------
 */
GBLDEF	volatile int4	first_event = no_event;

error_def(ERR_DEFEREVENT);

/* =============================================================================
 * EXPORTED FUNCTIONS
 * =============================================================================
 */
/* ------------------------------------------------------------------
 * *** INTERRUPT HANDLER ***
 * Sets up transfer table changes needed for:
 *   - Synchronous handling of asynchronous events.
 *   - Single-stepping and breakpoints
 * Note:
 *   - Call here from a routine specific to each event type.
 *   - Pass in a single value to pass on to xfer_table set function
 *     for that type. Calling routine should record any other event
 *     info, if needed, in volatile global variables.
 *   - If this is first event logged, will call back to the function
 *     provided and pass along parameter.
 * Future:
 *   - mdb_condition_handler does not call here -- should change it.
 *   - Ditto for routines related to zbreak and zstep.
 *   - Should put handler prototypes in a header file & include it here,
 *     if can use with some way to ensure type checking.
 *   - A higher-level interface (e.g. change sets) might be better.
 * ------------------------------------------------------------------
 */
boolean_t xfer_set_handlers(int4  event_type, void (*set_fn)(int4 param), int4 param_val)
{
	boolean_t 	is_first_event = FALSE;

	/* ------------------------------------------------------------
	 * Keep track of what event types have come in.
	 * - Get and set value atomically in case of concurrent
	 *   events and/or resetting while setting.
	 * ------------------------------------------------------------------
	 * Use interlocked operations to prevent races between set and reset,
	 * and to avoid missing overlapping sets.
	 * On HPUX-HPPA:
	 *    OK only if there's no a risk a conflicting operation is
	 *    in progress  (can deadlock in micro-lock).
	 * On all platforms:
	 *    Don't want I/O from a sensitive area.
	 * Avoid both by testing fast_lock_count, and doing interlocks and
	 * I/O only if it is non-zero. Can't be resetting then, so worst
	 * risk is missing an event when there's already one happening.
	 * ------------------------------------------------------------------
	 */
	VMS_ONLY(assert(lib$ast_in_prog()));
	if (fast_lock_count == 0)
	{
		DBGDFRDEVNT((stderr, "xfer_set_handlers: Before interlocked operations:  "
			     "xfer_table_events[%d]=%d, first_event=%s, num_deferred=%d\n",
			     event_type, xfer_table_events[event_type], (is_first_event ? "TRUE" : "FALSE"),
			     num_deferred));
		if (1 == INCR_CNT_SP(&xfer_table_events[event_type], &defer_latch))
			/* Concurrent events can collide here, too */
			is_first_event =  (1 == INCR_CNT_SP(&num_deferred, &defer_latch));
		DBGDFRDEVNT((stderr, "xfer_set_handlers: After interlocked operations:   "
			     "xfer_table_events[%d]=%d, first_event=%s, num_deferred=%d\n",
			     event_type,xfer_table_events[event_type], (is_first_event ? "TRUE" : "FALSE"),
			     num_deferred));
	} else if (1 == ++xfer_table_events[event_type])
		is_first_event = (1 == ++num_deferred);
	if (is_first_event)
	{
		first_event = event_type;
#		ifdef DEBUG_DEFERRED_EVENT
		if (0 != fast_lock_count)
			DBGDFRDEVNT((stderr, "xfer_set_handlers: Setting xfer_table for event type %d\n",
				     event_type));
#		endif
		/* -------------------------------------------------------
		 * If table changed, it was not synchronized.
		 * (Assumes these entries are all that would be changed)
		 * Note asserts bypassed for Itanium due to nature of the
		 * fixed up addresses making direct comparisions non-trivial.
		 * --------------------------------------------------------
		 */
#		ifndef __ia64
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
#		endif /* !IA64 */
		/* -----------------------------------------------
		 * Now call the specified set function to swap in
		 * the desired handlers (and set flags or whatever).
		 * -----------------------------------------------
		 */
		DBGDFRDEVNT((stderr, "xfer_set_handlers: Driving event setup handler\n"));
		set_fn(param_val);
	}
#	ifdef DEBUG_DEFERRED_EVENT
	else if (0 != fast_lock_count)
	{
		DBGDFRDEVNT((stderr, "xfer_set_handlers: ---Multiple deferred events---\n"
			     "Event type %d occurred while type %d was pending\n", event_type, first_event));
	} else
	{
		DBGDFRDEVNT((stderr, "xfer_set_handlers: Event bypassed -- was not first event\n"));
	}
#	endif
 	assert(no_event != first_event);
	return is_first_event;
}

/* ------------------------------------------------------------------
 * Reset the transfer table only if current event type
 * - Needed for abort before action, e.g., a tp timeout
 *   that happened just before commit was too late to be aborted and
 *   so must be cleared. In a case like this, reset should happen
 *   only if the event type attempting the clear is the type
 *   that caused the change.
 * - Other resets should be unconditional, in case there are
 *   unidentified control paths (e.g. exit paths) that cause
 *   resets when they did not set.
 * ------------------------------------------------------------------
 */
boolean_t xfer_reset_if_setter(int4 event_type)
{
	if (event_type == first_event)
	{
		DBGDFRDEVNT((stderr, "xfer_reset_if_setter: event_type is first_event\n"));
		/* Still have to synchronize the same way... */
		if (xfer_reset_handlers(event_type))
		{ 	/* Check for and activate any other pending events before returning success */
		  	/* (Not implemented) */
			return TRUE;
		} else
		{ 	/* Would require interleaved resets to get here, e.g. due to rts_error from interrupt level */
			assert(FALSE);
			return FALSE;
		}
	}
	DBGDFRDEVNT((stderr, "xfer_reset_if_setter: event_type is NOT first_event\n"));
	return FALSE;
}

/* ------------------------------------------------------------------
 * Reset transfer table to normal settings.
 *
 * - Intent: Put back all state that was or could have been changed
 *   due to prior deferral(s).
 *    - Would be easier to implement this assumption if this routine
 *      were changed to delegate responsibility as does the
 *      corresponding set routine.
 * - Note that all events are reenabled before user's handler
 *   would be executed (assuming one is appropriate for this event
 *   and has been specified)
 *    => It's possible to have handler-in-handler execution.
 *    => If no handler executed, would lose other deferred events due
 *       to reset of all pending.
 * - If M profiling is active, some entries should be set to the
 *       op_mprof* routines.
 * - Return value indicates whether reset type matches set type.
 *   If it does not, this indicates an "abnormal" path.
 *    - Should still reset the table in this case.
 *    - BUT: Consider also calling a reset routine for all setters
 *      that have been logged, to allow them to reset themselves,
 *      (for example, to reset TP timer & flags, or anything else
 *      that could cause unintended effects if left set after
 *      deferred events have been cleared).
 * - May need to update behavior to ensure it doesn't miss a
 *   critical event between registration of first event
 *   and clearing of all events. This seems problematic only if
 *   the following are true:
 *    - Two events are deferred at one time (call them A and B).
 *    - An M exception handler (ZTRAP or device) is required to
 *      execute due to B and perform action X.
 *    - Either no handler executes due to A, or the handler that
 *      does execute does not perform action X in response to B
 *      (this includes the possibility of performing X but not
 *       as needed by B, e.g. perhaps it should happen for both
 *       A and B but only happens for A).
 *   Seems like most or all of these can be addressed by carefully
 *   specifying coding requirements on M handlers.
 * ------------------------------------------------------------------
 */
boolean_t xfer_reset_handlers(int4 event_type)
{
	int4		e_type;
	boolean_t	reset_type_is_set_type;
	int4		status;
	int 		e, ei, e_tot = 0;

	/* ------------------------------------------------------------------
	 * Note: If reset routine can preempt path from handler to
	 * set routine (e.g. clearing event before acting on it),
	 * these assertions can fail.
	 * Should not happen in current design.
	 * ------------------------------------------------------------------
	 */
	assert(0 < num_deferred);
	assert(0 < xfer_table_events[event_type]);
	if (is_tracing_on)
	{
		FIX_XFER_ENTRY(xf_linefetch, op_mproflinefetch);
		FIX_XFER_ENTRY(xf_linestart, op_mproflinestart);
		FIX_XFER_ENTRY(xf_forchk1, op_mprofforchk1);
	} else
	{
		FIX_XFER_ENTRY(xf_linefetch, op_linefetch);
		FIX_XFER_ENTRY(xf_linestart, op_linestart);
		FIX_XFER_ENTRY(xf_forchk1, op_forchk1);
	}
	FIX_XFER_ENTRY(xf_forloop, op_forloop);
	FIX_XFER_ENTRY(xf_zbfetch, op_zbfetch);
	FIX_XFER_ENTRY(xf_zbstart, op_zbstart);
	FIX_XFER_ENTRY(xf_ret, opp_ret);
	FIX_XFER_ENTRY(xf_retarg, op_retarg);
	DBGDFRDEVNT((stderr, "xfer_reset_handlers: Reset xfer_table for event type %d.\n", event_type));
	reset_type_is_set_type =  (event_type == first_event);
#	ifdef DEBUG
	if (!reset_type_is_set_type)
 		rts_error(VARLSTCNT(4) ERR_DEFEREVENT, 2, event_type, first_event);
#	endif

#	ifdef DEBUG_DEFERRED_EVENT
	/* Note: concurrent modification of array elements means events that occur during this section will
	 * cause inconsistent totals.
	 */
	for (ei = no_event; ei < DEFERRED_EVENTS; ei++)
		e_tot += xfer_table_events[ei];
	if (1 < e_tot)
	{
		DBGDFRDEVNT((stderr, "xfer_reset_handlers: Event Log:\n"));
		for (ei=no_event; ei<DEFERRED_EVENTS; ei++)
			DBGDFRDEVNT((stderr, "xfer_reset_handlers:   Event type %d: count was %d.\n",
				     ei, xfer_table_events[ei]));
	}
#	endif

	/* -------------------------------------------------------------------------
	 * Kluge(?): set all locations to nonzero value to
	 * prevent interleaving with reset activities.
	 *
	 * Would be better to aswp with 0:
	 * - Won't lose any new events that way.
	 * -------------------------------------------------------------------------
	 */
	for (e_type = 1; DEFERRED_EVENTS > e_type; e_type++)
	{
		xfer_table_events[e_type] = 1;
	}

	/* -------------------------------------------------------------------------
	 * Reset external event modules that need it.
	 * (Should do this in a more modular fashion.)
	 * None
	 * -------------------------------------------------------------------------
	 */

	/* --------------------------------------------
	 * Reset private variables.
	 * --------------------------------------------
	 */
	first_event = no_event;
	num_deferred = 0;
	ctrap_action_is = 0;
	outofband = 0;
#	ifdef VMS
	status = sys$clref(efn_outofband);
	assert(SS$_WASSET == status);
	assertpro((SS$_WASSET == status) || (SS$_WASCLR == status));
#	endif
	/* ******************************************************************
	 * There is a race here:
	 * If a new event interrupts after previous line and before
	 * corresponding assignment in next loop, it will be missed.
	 * For most events, we're going to an M handler anyway, so it won't
	 * matter (assuming the handler would handle all pending events).
	 * But if not going to an M handler (e.g. if resetting zbreak/zstep),
	 * could miss another event.
	 *
	 * Better (to avoid missing any events):
	 *      aswp xfer_table_events elements (as described above), and
	 * check here if still zero. If not, must have missed that event
	 * since aswp, possibly before num_deferred was reset => never set
	 * xfer_table => should do that now.
	 *      If more than one is nonzero, choose first arbitrarily
	 * unless first_event is now set -- unless it is, we've lost track of
	 * which event was first.
	 * ******************************************************************
	 */
	/* Clear to allow new events to be reset only after we're all done. */
	for (e_type = 1; DEFERRED_EVENTS > e_type; e_type++)
		xfer_table_events[e_type] = FALSE;
	return reset_type_is_set_type;
}

/* ------------------------------------------------------------------
 * Perform action corresponding to the first async event that
 * was logged.
 * ------------------------------------------------------------------
 */
void async_action(bool lnfetch_or_start)
{
	/* Double-check that we should be here: */
	assert(0 < num_deferred);

	switch(first_event)
	{
		case (outofband_event):
			/* This function can be invoked only by a op_*intrrpt* transfer table function. Those transfer table
			 * functions should be active only for a short duration between the occurrence of an outofband event
			 * and the handling of it at a logical boundary (next M-line). We dont expect to be running with
			 * those transfer table functions for more than one M-line. If "outofband" is set to 0, the call to
			 * "outofband_action" below will do nothing and we will end up running with the op_*intrrpt* transfer
			 * table functions indefinitely. In this case M-FOR loops are known to return incorrect results which
			 * might lead to application integrity issues. It is therefore considered safer to GTMASSERT as we
			 * will at least have the core for analysis.
			 */
			assertpro(0 != outofband);
			outofband_action(lnfetch_or_start);
			break;
		case (tt_write_error_event):
#			ifdef UNIX
			xfer_reset_if_setter(tt_write_error_event);
			iott_wrterr();
#			endif
			/* VMS tt error processing is done in op_*intrrpt */
			break;
		case (network_error_event):
			/* -------------------------------------------------------
			 * Network error not implemented here yet. Need to move
			 * from mdb_condition_handler after review.
			 * -------------------------------------------------------
			 */
		case (zstp_or_zbrk_event):
			/* -------------------------------------------------------
			 * ZStep/Zbreak events not implemented here yet. Need to
			 * move here after review.
			 * -------------------------------------------------------
			 */
		default:
			assertpro(FALSE);	/* see above assertpro() for comment as to why this is needed */
	}
}

/* ------------------------------------------------------------------
 * Indicate whether an xfer_table change is pending.
 * Only works for changes made using routines in this module
 * (need to rework others, too).
 *
 * Might not be needed.
 * ------------------------------------------------------------------
 */
boolean_t xfer_table_changed(void)
{
	return (0 != num_deferred);
}
