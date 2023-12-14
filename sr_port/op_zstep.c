/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "zstep.h"
#include "stack_frame.h"
#include "xfer_enum.h"
#include "indir_enum.h"
#include "op.h"
#include "fix_xfer_entry.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "deferred_events.h"
#include "restrict.h"

GBLDEF stack_frame	*zstep_level;

GBLREF bool		neterr_pending;
GBLREF int		iott_write_error;
GBLREF int4		gtm_trigger_depth;
GBLREF intrpt_state_t	intrpt_ok_state;
GBLREF stack_frame	*frame_pointer;
GBLREF volatile int4	outofband;
GBLREF xfer_entry_t	xfer_table[];

void op_zstep(uint4 code, mval *action)
{
	boolean_t	already_ev_handling;
	int4		ev, status;
	intrpt_state_t	prev_intrpt_state;
	save_xfer_entry	*entry;
	stack_frame	*fp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((0 < gtm_trigger_depth) && RESTRICTED(trigger_mod))
		return;
	TAREF1(save_xfer_root, zstep_pending).event_state = pending;
	TAREF1(save_xfer_root, zstep_pending).param_val = (int)code;
	if (neterr_pending)
		return;
	if (NULL == action)
		TREF(zstep_action) = TREF(dollar_zstep);	/* no action specified on the command - use $ZSTEP */
	else
	{	/* compile the action to make it ready to execute */
		op_commarg(action, indir_linetail);
		op_unwind();
		TREF(zstep_action) = *action;
	}
	if (0 == (TREF(zstep_action)).str.len)
	{	/* if an action does not exist shut down zstepping */
		DBGDFRDEVNT((stderr, "%d %s: ctrap_set - removing zstep from play\n", __LINE__, __FILE__));
		TAREF1(save_xfer_root, zstep_pending).event_state = not_in_play;
		return;
	}
	/* WARNING! AIO sets multi_thread_in_use which disables DEFER_INTERRUPTS, treat it like an active event */
	if (!(already_ev_handling = ((INTRPT_IN_EVENT_HANDLING == intrpt_ok_state) || multi_thread_in_use)))
		DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
	for (ev = ctrlc; ev <  DEFERRED_EVENTS; ev++)
	{	/* make any thing we've potentially overlaying queued */
		if (zstep_pending == ev)
			continue;
		status = (TAREF1(save_xfer_root, ev)).event_state;
		switch (status)
		{
		case signaled:
		case pending:
		case active:
			DBGDFRDEVNT((stderr, "%d %s: queueing entry for - event: %d\n", __LINE__, __FILE__, ev));
			entry = &(TAREF1(save_xfer_root, ev));
			entry->event_state = queued;
			SAVE_XFER_QUEUE_ENTRY(ev, entry->param_val);
		default:						/* WARNING fallthrough */
			break;
		}
	}
	switch(code)
	{
		case ZSTEP_INTO:
			DBGDFRDEVNT((stderr, "%d %s: INTO \n", __LINE__, __FILE__));
			FIX_XFER_ENTRY(xf_linefetch, op_zstepfetch);	/* shim: transfers to op_zst_break */
			FIX_XFER_ENTRY(xf_linestart, op_zstepstart);	/* shim: transfers to op_zst_break */
			FIX_XFER_ENTRY(xf_zbfetch, op_zstzbfetch);	/* shim: transfer to op_zst_break after op_zbreak */
			FIX_XFER_ENTRY(xf_zbstart, op_zstzbstart);	/* shim: transfer to op_zst_break after op_zbreak */
			break;
		case ZSTEP_OVER:
		case ZSTEP_OUTOF:

			for (fp = frame_pointer; fp && !(fp->type & SFT_COUNT); fp = fp->old_frame_pointer)
				; /* skip uncounted frames (if any) in storing "zstep_level" (when ZSTEP OVER/OUTOF action needs to kick in) */
			zstep_level = fp;
			if (ZSTEP_OVER == code)
			{
				DBGDFRDEVNT((stderr, "%d %s: OVER - zstep_level: %x\n", __LINE__, __FILE__, zstep_level));
				FIX_XFER_ENTRY(xf_linefetch, op_zst_fet_over);	/* shim: zstep_level gates call to op_zst_break */
				FIX_XFER_ENTRY(xf_linestart, op_zst_st_over);	/* shim: zstep_level gates call to op_zst_break */
				FIX_XFER_ENTRY(xf_zbfetch, op_zstzb_fet_over);	/* shim: zstep_level gates call to op_zst_break */
				FIX_XFER_ENTRY(xf_zbstart, op_zstzb_st_over);	/* shim: zstep_level gates call to op_zst_break */
			} else
			{
				DBGDFRDEVNT((stderr, "%d %s: OUTOF - zstep_level: %x\n", __LINE__, __FILE__, zstep_level));
				FIX_XFER_ENTRY(xf_ret, opp_zstepret);		/* shim: zstep_level gates call to op_zstepret */
				FIX_XFER_ENTRY(xf_retarg, opp_zstepretarg);	/* shim: zstep_level gates call to op_zstepret */
				FIX_XFER_ENTRY(xf_linefetch, op_linefetch);	/* normal/default transfer */
				FIX_XFER_ENTRY(xf_linestart, op_linestart);	/* normal/default transfer */
				FIX_XFER_ENTRY(xf_zbfetch, op_zbfetch);		/* shim: normal transfer to op_zbreak */
				FIX_XFER_ENTRY(xf_zbstart, op_zbstart);		/* shim: normal transfer to op_zbreak */
			}
			break;
		default:
			assertpro(FALSE && code);
	}
	if (!already_ev_handling)
		ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
	return;
}
