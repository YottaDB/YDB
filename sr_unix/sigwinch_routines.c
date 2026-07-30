/****************************************************************
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Routines implementing the SIGWINCH deviceparameter for terminal devices (YDB#1247).
 *
 * USE $PRINCIPAL:SIGWINCH arranges for the device WIDTH and LENGTH to be refreshed from the OS whenever the
 * process receives a SIGWINCH signal (i.e. whenever the terminal window changes size). USE $PRINCIPAL:SIGWINCH=expr
 * additionally XECUTEs expr after the refresh; USE $PRINCIPAL:NOSIGWINCH turns both off. The overall flow, modeled
 * on the $ZTIMEOUT machinery, is:
 *
 *   1. "tt_sigwinch_devparam" (driven by OPEN/USE processing in iott_open.c/iott_use.c) validates the code (if any),
 *	stores a private copy in the principal terminal's d_tt_struct and installs "tt_sigwinch_event" as the
 *	SIGWINCH signal handler (SIGWINCH is otherwise ignored - see sig_init.c).
 *   2. "tt_sigwinch_event" defers the event through "xfer_set_handlers" (the sigwinch outofband event) which
 *	drives "sigwinch_set". If there is no code to XECUTE, that refreshes the WIDTH/LENGTH there and then and
 *	drops the event; all the remaining steps apply only to SIGWINCH=expr.
 *   3. At the next safe point, async_action/outofband_action drive "sigwinch_action" below, which refreshes
 *	the device WIDTH/LENGTH from the OS and raises the internal error ERR_SIGWINCHRQST.
 *   4. mdb_condition_handler catches that error and pushes a frame (SFT_SIGWINCH) that XECUTEs the code
 *	(see jobintrpt_ztime_process.c), after which the interrupted M line (e.g. a READ) resumes.
 */

#include "mdef.h"

#include <sys/ioctl.h>

#include "gtm_signal.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_termios.h"

#include "io.h"
#include "iottdef.h"
#include "gtmio.h"
#include "op.h"
#include "xfer_enum.h"
#include "have_crit.h"
#include "error_trap.h"
#include "deferred_events.h"
#include "deferred_events_queue.h"
#include "fix_xfer_entry.h"
#include "stack_frame.h"
#include "indir_enum.h"
#include "stringpool.h"
#include "mv_stent.h"
#include "compiler.h"
#include "sig_init.h"
#include "libyottadb_int.h"
#include "invocation_mode.h"
#include "readline.h"

GBLREF	boolean_t		ztrap_explicit_null;
GBLREF	dollar_ecode_type	dollar_ecode;
GBLREF	int4			gtm_trigger_depth;
GBLREF	io_pair			io_std_device;
GBLREF	stack_frame		*frame_pointer;
GBLREF	uint4			dollar_tlevel;
GBLREF	volatile boolean_t	dollar_zininterrupt;
GBLREF	volatile boolean_t	sigwinch_inprog;
GBLREF	volatile int4		outofband;
GBLREF	xfer_entry_t		xfer_table[];

STATICDEF boolean_t		winch_on;	/* TRUE when tt_sigwinch_event is installed as the SIGWINCH handler */

STATICFNDCL void tt_sigwinch_resize(void);

error_def(ERR_SIGWINCHRQST);

/* Process the SIGWINCH[=expr] and NOSIGWINCH deviceparameters for a terminal device. A SIGWINCH signal describes
 * the process's controlling terminal, i.e. $PRINCIPAL, so (like CENABLE/HUPENABLE) this is a no-op for any other
 * terminal device; it is also a no-op for non-terminal devices, which never get here (iop_sigwinch/iop_nosigwinch
 * are only processed by iott_open.c/iott_use.c).
 *	"enable" is TRUE for SIGWINCH[=expr] (window resizes refresh the device WIDTH and LENGTH and, if "val_len"
 *		is non-zero, XECUTE the code in "val_addr" afterwards) and
 *	"enable" is FALSE for NOSIGWINCH (window resizes are ignored, the default).
 */
void tt_sigwinch_devparam(io_desc *iod, char *val_addr, int val_len, boolean_t enable)
{
	d_tt_struct		*tt_ptr, *tt_std;
	mval			*tmpMV;
	struct sigaction	act;

	assert(tt == iod->type);
	assert(enable || (0 == val_len));
	if ((NULL == io_std_device.in) || (tt != io_std_device.in->type))
		return;
	tt_ptr = (d_tt_struct *)iod->dev_sp;
	tt_std = (d_tt_struct *)io_std_device.in->dev_sp;
	if (tt_ptr->fildes != tt_std->fildes)
		return;
	if (val_len)
	{	/* Compile the code to validate it up front (a runtime compile after this point cannot fail - the same
		 * approach $ZTIMEOUT uses to avoid needing compile-failure cleanup). The temporary mval push protects
		 * the value from being lost if "op_commarg" triggers a "stp_gcol" (see DEF_EXCEPTION in io.h).
		 */
		PUSH_MV_STENT(MVST_MVAL);
		tmpMV = &mv_chain->mv_st_cont.mvs_mval;
		tmpMV->mvtype = MV_STR;
		tmpMV->str.len = val_len;
		tmpMV->str.addr = val_addr;
		op_commarg(tmpMV, indir_linetail);
		op_unwind();
		if (tt_std->sigwinch_handler.len)
			free(tt_std->sigwinch_handler.addr);
		tt_std->sigwinch_handler.addr = malloc(val_len);
		memcpy(tt_std->sigwinch_handler.addr, tmpMV->str.addr, val_len);
		tt_std->sigwinch_handler.len = val_len;
		POP_MV_STENT();
	} else if (tt_std->sigwinch_handler.len)
	{	/* SIGWINCH (no value), SIGWINCH="" or NOSIGWINCH - in all three cases there is no code to XECUTE */
		free(tt_std->sigwinch_handler.addr);
		tt_std->sigwinch_handler.addr = NULL;
		tt_std->sigwinch_handler.len = 0;
	}
	tt_std->sigwinch_on = enable;
	if (enable && !winch_on)
	{
		if (!USING_ALTERNATE_SIGHANDLING)
		{
			memset(&act, 0, SIZEOF(act));
			sigemptyset(&act.sa_mask);
			act.sa_flags = YDB_SIGACTION_FLAGS;
			act.sa_sigaction = tt_sigwinch_event;
			sigaction(SIGWINCH, &act, NULL);
		} else
			SET_ALTERNATE_SIGHANDLER(SIGWINCH, &ydb_altwinch_sighandler);
		winch_on = TRUE;
	} else if (!enable && winch_on)
	{	/* NOSIGWINCH - go back to ignoring the signal (like NOHUPENABLE) */
		if (!USING_ALTERNATE_SIGHANDLING)
		{
			memset(&act, 0, SIZEOF(act));
			sigemptyset(&act.sa_mask);
			act.sa_flags = 0;
			act.sa_handler = SIG_IGN;
			sigaction(SIGWINCH, &act, NULL);
		} else
			SET_ALTERNATE_SIGHANDLER(SIGWINCH, NULL);
		winch_on = FALSE;
	}
}

/* Returns the SIGWINCH handler code of the principal terminal device or NULL if there is none */
mstr *tt_sigwinch_handler(void)
{
	d_tt_struct	*tt_ptr;

	if ((NULL == io_std_device.in) || (tt != io_std_device.in->type))
		return NULL;
	tt_ptr = (d_tt_struct *)io_std_device.in->dev_sp;
	return (tt_ptr->sigwinch_handler.len ? &tt_ptr->sigwinch_handler : NULL);
}

/* The OS signal handler for SIGWINCH - modeled on jobinterrupt_event */
void tt_sigwinch_event(int sig, siginfo_t *info, void *context)
{
	/* See Signal Handling comment in sr_unix/readline.c */
	if (readline_catch_signal)
		readline_signal_count++;
	if (!USING_ALTERNATE_SIGHANDLING)
	{
		FORWARD_SIG_TO_MAIN_THREAD_IF_NEEDED(sig_hndlr_tt_sigwinch_event, sig, NULL, info, context);
	}
	(void)xfer_set_handlers(sigwinch, 0, FALSE);
	/* If we are in SIMPLEAPI mode and the original handler was neither SIG_DFL nor SIG_IGN, drive the originally
	 * defined handler before we replaced them.
	 */
	if (!USING_ALTERNATE_SIGHANDLING)
	{
		drive_non_ydb_signal_handler_if_any("tt_sigwinch_event", sig, info, context, FALSE);
	}
	if (readline_catch_signal)
		readline_signal_count--;
	/* Only long jump out of readline if the event actually went pending (it is instead dropped if there is no
	 * code to XECUTE, or queued if a SIGWINCH handler is already running - see "sigwinch_set"); readline.c
	 * saves the partial input line and drives "async_action".
	 */
	if ((sigwinch == outofband) && readline_catch_signal && (0 == readline_signal_count))
	{	/* See Signal Handling comment in sr_unix/readline.c for an explanation of the following lines */
		readline_catch_signal = FALSE;
		assert(!in_os_signal_handler);
		siglongjmp(readline_signal_jmp, 1);
	}
}

/* The alternate handler for SIGWINCH - we are called from the dispatcher and call the regular handler but with
 * two NULL parms that are not used when doing alternate signal handling.
 */
int ydb_altwinch_sighandler(int signum)
{
	tt_sigwinch_event(signum, NULL, NULL);
	return YDB_OK;
}

/* Call back routine from xfer_set_handlers to complete outofband setup - modeled on ztimeout_set */
void sigwinch_set(int4 dummy_param)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(INTRPT_IN_EVENT_HANDLING == intrpt_ok_state);
	assert(sigwinch == outofband);
	if (!winch_on)
	{	/* SIGWINCH is no longer in effect (e.g. NOSIGWINCH after the signal was received) so drop the event */
		outofband = no_event;
		TAREF1(save_xfer_root, sigwinch).event_state = not_in_play;
		return;
	}
	if (NULL == tt_sigwinch_handler())
	{	/* SIGWINCH with no code to XECUTE. Refresh the device WIDTH/LENGTH right here (all it takes is an
		 * ioctl and a couple of assignments) and drop the event instead of deferring it to a safe point.
		 * Apart from being cheaper, this leaves any in-progress READ undisturbed (it just sees an EINTR and
		 * keeps going) whereas an outofband event would have it save its state for a restart that only the
		 * XECUTE of the handler code (see "sigwinch_action") would drive.
		 */
		tt_sigwinch_resize();
		outofband = no_event;
		TAREF1(save_xfer_root, sigwinch).event_state = not_in_play;
		return;
	}
	if (dollar_zininterrupt || sigwinch_inprog || ((0 < dollar_ecode.index) && ETRAP_IN_EFFECT)
		|| (jobinterrupt == (TREF(save_xfer_root_ptr))->ev_que.fl->outofband)
		|| ((0 == gtm_trigger_depth) && dollar_tlevel))
	{	/* not a good time, so save it; note the sigwinch_inprog check above coalesces a burst of resize
		 * events (e.g. an interactive window drag) into at most one queued event while a handler runs
		 */
		outofband = no_event;
		TAREF1(save_xfer_root, sigwinch).event_state = queued;
		SAVE_XFER_QUEUE_ENTRY(sigwinch, 0);
		DBGDFRDEVNT((stderr, "%d %s: sigwinch_set - SIGWINCH queued\n", __LINE__, __FILE__));
		return;
	}
	DBGDFRDEVNT((stderr, "%d %s: sigwinch_set - NOT deferred\n", __LINE__, __FILE__));
	outofband = sigwinch;
	TAREF1(save_xfer_root, sigwinch).event_state = pending;
	DEFER_INTO_XFER_TAB;
}

/* Update the principal terminal device's WIDTH/LENGTH from the OS. Without this the XECUTEd handler would have
 * no way to learn the new terminal size (YottaDB otherwise only reads the size from terminfo at OPEN time).
 */
STATICFNDEF void tt_sigwinch_resize(void)
{
	d_tt_struct	*tt_ptr;
	int		status;
	io_desc		*d_in, *d_out;
	struct winsize	ws;

	d_in = io_std_device.in;
	if ((NULL == d_in) || (tt != d_in->type))
		return;
	tt_ptr = (d_tt_struct *)d_in->dev_sp;
	status = ioctl(tt_ptr->fildes, TIOCGWINSZ, &ws);
	if (0 != status)
		return;		/* harmless - just leave the dimensions alone */
	d_out = io_std_device.out;
	if (0 < ws.ws_col)
	{
		d_in->width = ws.ws_col;
		if ((d_out != d_in) && (tt == d_out->type))
			d_out->width = ws.ws_col;
	}
	if (0 < ws.ws_row)
	{
		d_in->length = ws.ws_row;
		if ((d_out != d_in) && (tt == d_out->type))
			d_out->length = ws.ws_row;
	}
}

/* Driven at the recognition point of a sigwinch event by async_action/outofband_action - modeled on ztimeout_action */
void sigwinch_action(void)
{
	intrpt_state_t	prev_intrpt_state;
	save_xfer_entry	*entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(INTRPT_IN_EVENT_HANDLING != intrpt_ok_state);
	DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
	assert(sigwinch == outofband);
	assert(pending == TAREF1(save_xfer_root, sigwinch).event_state);
	DBGDFRDEVNT((stderr, "%d %s: sigwinch_action - driving the SIGWINCH handler\n", __LINE__, __FILE__));
	if (winch_on)
		tt_sigwinch_resize();
	entry = &TAREF1(save_xfer_root, sigwinch);
	if (queued == entry->event_state)
		REMOVE_XFER_QUEUE_ENTRY(sigwinch);
	entry->event_state = active;		/* required by the routine invoked on the next line */
	(void)xfer_reset_if_setter(sigwinch);
	entry->event_state = not_in_play;
	if (!winch_on || (NULL == tt_sigwinch_handler()))
	{	/* NOSIGWINCH or a valueless SIGWINCH was specified between the event going pending and it being
		 * processed here, so the refresh above (if any) is all there is to do. Note that an event is never
		 * deferred to here in the first place if there is no code to XECUTE (see "sigwinch_set").
		 */
		ENABLE_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
		return;
	}
	frame_pointer->mpc = frame_pointer->restart_pc;
	frame_pointer->ctxt = frame_pointer->restart_ctxt;
	ENABLE_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_SIGWINCHRQST);
}
