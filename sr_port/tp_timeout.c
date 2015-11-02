/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* ------------------------------------------------------------------
 * 	Routines & data for managing TP timeouts
 *	----------------------------------------
 *
 * These functions implement TP timeout state transitions. The states
 * below are defined by three flag variables:
 *
 *              		| Flag:
 *    State     		|  timed    expired	set-xfer
 *    --------  		|  -------  -------	--------
 *     Clear    		|  FALSE    FALSE	FALSE
 *     Set-timer      		|  TRUE     FALSE	FALSE
 *     Expired		 	|  TRUE     TRUE	FALSE
 *     Clearing-no-set-xfer 	|  FALSE    TRUE	FALSE
 *     Expired-set-xfer		|  TRUE     TRUE	TRUE
 *     Clearing1-set-xfer	|  FALSE    TRUE	TRUE
 *     Clearing2-set-xfer	|  FALSE    FALSE	TRUE
 *
 * Only the following transitions are allowed:
 *
 *   Transition
 *   ----------
 *   Clear -> Set
 *   Set -> Clear
 *   Set -> Expired
 *   Expired -> Clearing-no-set-xfer
 *   Expired -> Expired-set-xfer
 *   Clearing-no-set-xfer -> Clear
 *   Clearing1-set-xfer -> Clearing2-set-xfer
 *   Clearing2-set-xfer -> Clear
 *
 * NOTE:
 *   - Each "state" represents multiple program states.
 *   - Transitions are designed to be monotonic, so that
 *     variable values always correctly represent the current state
 *     (even when interrupted between individual operations).
 *   - Memory pipelining effects will require barriers in a fully
 *     reentrant environment (e.g. VMS 7.x + kernel threads).
 * ------------------------------------------------------------------
 */

#include "mdef.h"
#if defined (VMS)
#include "efn.h"
#include <ssdef.h>
#endif
#include "outofband.h"
#include "gt_timer.h"
#include "xfer_enum.h"
#include "tp_timeout.h"
#include "deferred_events.h"
#include "op.h"
#include "fix_xfer_entry.h"

/* ------------------------------------------------------------------
 * Macro for timer ID
 * ------------------------------------------------------------------
 */
#define TP_TIMER_ID (TID) &tp_start_timer

/* =============================================================================
 * EXTERNAL VARIABLES
 * =============================================================================
 */
GBLREF xfer_entry_t     xfer_table[];
GBLREF volatile int4	outofband;

void tptimeout_set(int4 dummy_param);


/* =============================================================================
 * FILE-SCOPE VARIABLES
 * =============================================================================
 */

/*	------------------------------------------------------------
 * 	Shared between timer handler and main process
 *	------------------------------------------------------------
 */

/* "Are we currently in a timed transaction?" It does not change asynchronously */
static boolean_t 		in_timed_tn = FALSE;


/*	"Did timeout succeed in setting xfer_table?"
 * 	 (vs. lose to another event)
 */
GBLDEF	volatile boolean_t 	tp_timeout_set_xfer = FALSE;


/* =============================================================================
 * FILE-SCOPE FUNCTIONS
 * =============================================================================
 */

/* ------------------------------------------------------------------
 * Timer handler (Set -> Expired)
 *
 * - Sets flag to indicate timeout has occurred.
 * - Should only happen if a timeout has been started (and not cancelled),
 *   and has not yet expired.
 * - Static because it's for internal use only.
 * ------------------------------------------------------------------
 */
static void tp_expire_now() /* Param list is empty, not void,
			       * to avoid mixing K&R with ANSI */
{
	assert(in_timed_tn);
/*	Consider logging operator message here.
 *	(ditto for network errors, etc -- log when arrive, handle later).
 */
	tp_timeout_set_xfer = xfer_set_handlers(outofband_event, &tptimeout_set, 0);
}


/* ------------------------------------------------------------------
 * Set transfer table for synchronous handling of TP timeout.
 * Should be called only from set_xfer_handlers.
 *
 * Notes:
 *  - Dummy parameter is for calling compatibility.
 *  - Prototype goes in deferred events header file, not in
 *    tp_timeout header file, because it's not for general use.
 * ------------------------------------------------------------------
 */
void tptimeout_set(int4 dummy_param)
{
#if defined (VMS)
	int4 status;
#endif

	if (tptimeout != outofband)
	{
		FIX_XFER_ENTRY(xf_linefetch, op_fetchintrrpt);
		FIX_XFER_ENTRY(xf_linestart, op_startintrrpt);
		FIX_XFER_ENTRY(xf_zbfetch, op_fetchintrrpt);
		FIX_XFER_ENTRY(xf_zbstart, op_startintrrpt);
		FIX_XFER_ENTRY(xf_forchk1, op_startintrrpt);
		FIX_XFER_ENTRY(xf_forloop, op_forintrrpt);
		outofband = tptimeout;
		VMS_ONLY(
			status = sys$setef(efn_outofband);
			assert(SS$_WASCLR == status);
			if (status != SS$_WASCLR && status != SS$_WASSET)
				GTMASSERT;
			sys$wake(0,0);
		)
	}
}

/* =============================================================================
 * EXPORTED FUNCTIONS
 * =============================================================================
 */
/* ------------------------------------------------------------------
 * Start timer (Clear -> Set-timer)
 *
 * Change state before starting timer so
 * pops will always happen in the Set state.
 *
 * Note: timer handler data length and pointer are specified as
 * 0 and null, respectively. Handler parameter list
 * should therefore probably be (int4, char*), due to
 * how it's called from timer_handler(), but no one else does that.
 * ------------------------------------------------------------------
 */
void tp_start_timer(int4 timeout_seconds)
{
	assert(!in_timed_tn);
	in_timed_tn = TRUE;
	start_timer(TP_TIMER_ID, 1000*timeout_seconds, &tp_expire_now, 0, NULL);
}

/* ------------------------------------------------------------------
 * Transaction done, clear pending timeout:
 *     (Set-timer -> Clear)
 *     (Expired -> Clearing-no-set-xfer
 *              -> Clear)
 *     (Expired-set-xfer -> Clearing1-set-xfer
 *                       -> Clearing2-set-xfer
 *                       -> Clear)
 *
 * - Ok to call even if no timeout was set.
 * - Reset transfer table if expired and timeout was the reason.
 * - Resets expired flag AFTER cancelling the timer,
 *   in case the alarm expires just before it's cancelled.
 *
 * Notes:
 * - Test for tp_timeout_set_xfer may obsolete use of conditional
 *   xfer_table reset function. If so, should change this routine
 *   to simply return value of tp_timeout_set_xfer when entered.
 * ------------------------------------------------------------------
 */
void tp_clear_timeout(void)
{
         boolean_t tp_timeout_check = FALSE;

#ifdef DEBUG_DEFERRED
#include "gtm_stdio.h"
	 FPRINTF(stderr,"\nTPCT: \n");
#endif

	if (in_timed_tn)
	{
		/* ------------------------------------------------
		 * Works whether or not timer already expired.
		 *
		 * Would be faster to only cancel if expired, but
		 * could miss a last-minute timer pop that way.
		 *
		 * Can save time by making timers more efficient, or
		 * by setting a "cancelling" flag to ensure no
		 * missed pops, and then only cancel if expired.
		 * ------------------------------------------------
		 */
		cancel_timer(TP_TIMER_ID);

		/* --------------------------------------------
		 * For unambiguous states, clear this flag
		 * after cancelling timer and before clearing
		 * expired flag.
		 * --------------------------------------------
		 */
		in_timed_tn = FALSE;

		/* -----------------------------------------------------
		 * Should clear xfer settings only if set them.
		 * -----------------------------------------------------
		 */
		if (tp_timeout_set_xfer)
		{
			/* ------------------------------------------------
			 * Get here only if set xfer_table due to timer pop.
			 * - If timeout is aborting transaction, or
			 * - If committing and timer popped too late to
			 *   stop it => want to undo xfer_table change
			 *   before it invokes ZTRAP via async_action.
			 * - Assert should never trigger due to test of
			 *   tp_timeout_set_xfer.
			 * --------------------------------------------
			 */

		        tp_timeout_check = xfer_reset_if_setter(outofband_event);
#ifdef DEBUG_DEFERRED
			FPRINTF(stderr,"\nTPCT: passed reset of xfer table\n");
#endif
			assert(tp_timeout_check);
			tp_timeout_set_xfer = FALSE;
		} else
		{
			/* ----------------------------------------------------
			 * Get here only if clearing TP timer when a
			 * TP timeout did not set xfer_table.
			 * Examples:
			 *   -- Before timer popped, via op_tcommit, if
			 *      successfully committing a timed transaction.
			 *      NOTE: other events could be pending, if they
			 *      occurred late in transaction.
			 *   -- After timer popped, also via op_tcommit,
			 *      if successfully committing
			 *      a timed transaction and another event
			 *      occurred first (but too late in transaction to
			 *      abort it).
			 *   Before or after timer popped:
			 *   -- If another event (e.g. ^C) happened first,
			 *      and now that event is clearing TP timer to
			 *      prevent timer pop in M handler (before it can
			 *      do TROLLBACK).
			 *   -- Via op_trollback(), whether in user code
			 *      or otherwise (e.g. at exit).
			 * ----------------------------------------------------
			 */

			; /* (There's nothing to do) */

		}
	}
}



/*
 * Used in transfer table for signaling exception
 */
void tp_timeout_action(void)
{
	error_def(ERR_TPTIMEOUT);
	rts_error(VARLSTCNT(1) ERR_TPTIMEOUT);
}
