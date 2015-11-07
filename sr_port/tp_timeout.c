/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

#include "gtm_stdio.h"
#if defined (VMS)
#  include "efn.h"
#  include <ssdef.h>
#endif

/* tp_timeout.h needs to be included to potentially define DEBUG_TPTIMEOUT_DEFERRAL */
#include "tp_timeout.h"
#ifdef DEBUG_TPTIMEOUT_DEFERRAL
#  include "gtm_time.h"
#endif

#include "outofband.h"
#include "gt_timer.h"
#include "xfer_enum.h"
#include "deferred_events.h"
#include "op.h"
#include "fix_xfer_entry.h"
#include "error_trap.h"

#define TP_TIMER_ID (TID)&tp_start_timer

/* If debugging timeout deferral, it is helpful to timestamp the messages. Encapsulate our debugging macro with
 * enough processing to be able to do that.
 */
#ifdef DEBUG_TPTIMEOUT_DEFERRAL
#  define TIME_EXT_FMT "%T"
#  define DBGWTIME(x) 									\
{											\
	time_t		now;								\
	struct tm	*tm_struct;							\
	char		asccurtime[10];							\
	size_t		len;								\
	now = time(NULL);								\
	GTM_LOCALTIME(tm_struct, &now);							\
	STRFTIME(asccurtime, SIZEOF(asccurtime), TIME_EXT_FMT, tm_struct, len);		\
	DBGTPTDFRL(x);									\
}
#else
#  define DBGWTIME(x)
#endif

/* External variables */
GBLREF dollar_ecode_type	dollar_ecode;
GBLREF mval			dollar_etrap;
GBLREF mval			dollar_ztrap;
GBLREF volatile int4		outofband;
GBLREF xfer_entry_t     	xfer_table[];
GBLREF boolean_t		in_timed_tn;
GBLREF boolean_t		tp_timeout_set_xfer;
GBLREF boolean_t		tp_timeout_deferred;
GBLREF boolean_t		dollar_zininterrupt;
GBLREF boolean_t		ztrap_explicit_null;

error_def(ERR_TPTIMEOUT);

STATICFNDCL void tptimeout_set(int4 dummy_param);
STATICFNDCL void tp_expire_now(void);

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
STATICFNDEF void tp_expire_now(void)
{
	DBGWTIME((stderr, "%s tp_expire_now: Driving xfer_set_handlers\n" VMS_ONLY("\n"),
		  asccurtime));
	assert(in_timed_tn);
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
STATICFNDEF void tptimeout_set(int4 dummy_param)
{
	VMS_ONLY(int4 status;)

#	ifdef UNIX
	/* TP timeout deferral is UNIX-only. This is because the mechanism becomes much more complicated on VMS
	 * due to the mixing of timers and TP timeout, both of which use the same event flag so don't play well
	 * together. It could be fixed for VMS with some perhaps non-trivial work but with VMS approaching EOL,
	 * the effort was deemed unnecessary.
	 */
	if (((0 < dollar_ecode.index) && (ETRAP_IN_EFFECT)) UNIX_ONLY( || dollar_zininterrupt))
	{	/* Error handling or job interrupt is in effect - defer tp timeout
		 * until $ECODE is cleared and/or we have unrolled the job interrupt
		 * frame
		 */
		assert(!tp_timeout_deferred);	/* Note: even though we come back thru tptimeout_set() from op_svput and
						 * other places when tp_timeout_deferred was known to be true, we shouldn't
						 * come back through with the above conditions letting us in THIS block. So
						 * We should only be coming through here via the initial timeout where we
						 * should be garranteed this flag is OFF.
						 */
		tp_timeout_deferred = TRUE;
		DBGWTIME((stderr, "%s tptimeout_set: TP timeout deferred\n" VMS_ONLY("\n"), asccurtime));
		return;
	} else
	{
		DBGWTIME((stderr, "%s tptimeout_set: TP timeout *NOT* deferred - ecode index: %d  etrap: %d\n"  VMS_ONLY("\n"),
			  asccurtime, dollar_ecode.index, ETRAP_IN_EFFECT));
	}
#	endif
	if (tptimeout != outofband)
	{
		FIX_XFER_ENTRY(xf_linefetch, op_fetchintrrpt);
		FIX_XFER_ENTRY(xf_linestart, op_startintrrpt);
		FIX_XFER_ENTRY(xf_zbfetch, op_fetchintrrpt);
		FIX_XFER_ENTRY(xf_zbstart, op_startintrrpt);
		FIX_XFER_ENTRY(xf_forchk1, op_startintrrpt);
		FIX_XFER_ENTRY(xf_forloop, op_forintrrpt);
		outofband = tptimeout;
#		ifdef VMS
		/* Set event flag now that intercept is in place */
		status = sys$setef(efn_outofband);
		assert(SS$_WASCLR == status);
		assertpro((SS$_WASCLR == status) || (SS$_WASSET == status));
		sys$wake(0,0);
#		endif
	} else
	{
		DBGWTIME((stderr, "%s tptimeout_set: tptimeout outofband already set\n" VMS_ONLY("\n"), asccurtime));
	}
	UNIX_ONLY(tp_timeout_deferred = FALSE);	/* Clear flag now that intercept setup or already installed */
}

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
	DBGWTIME((stderr, "%s tp_start_timer: Starting timer for tptimeout\n" VMS_ONLY("\n"), asccurtime));
	in_timed_tn = TRUE;
	start_timer(TP_TIMER_ID, (1000 * timeout_seconds), &tp_expire_now, 0, NULL);
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

	DBGWTIME((stderr, "%s tp_clear_timeout: Transaction complete - clearing tptimeout\n" VMS_ONLY("\n"), asccurtime));
	tp_timeout_deferred = FALSE;
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
			DBGWTIME((stderr, "%s tp_clear_timeout: tptimeout timer had already popped - clearing driver "
				  "indicicator\n" VMS_ONLY("\n"), asccurtime));
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
 * Routine is driven at tp timeout recognition point by outofband_action().
 */
void tp_timeout_action(void)
{
	/* Since tp timeout error is about to be driven, reset the interrupt mechanism before
	 * any error handler gets driven so we don't trip another call inside the error handler.
	 */
	DBGWTIME((stderr, "%s tp_timeout_action: Driving TP timeout error\n" VMS_ONLY("\n"), asccurtime));
	tp_clear_timeout();
	rts_error(VARLSTCNT(1) ERR_TPTIMEOUT);
}
