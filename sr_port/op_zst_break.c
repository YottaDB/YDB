/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "xfer_enum.h"
#include "indir_enum.h"
#include "op.h"
#include "io.h"
#include "deferred_events.h"
#include "deferred_events_queue.h"
#include "fix_xfer_entry.h"
#include "restrict.h"
#include "have_crit.h"

GBLREF boolean_t	is_tracing_on;
GBLREF int4		gtm_trigger_depth;
GBLREF intrpt_state_t	intrpt_ok_state;
GBLREF stack_frame	*frame_pointer;
GBLREF xfer_entry_t	xfer_table[];

void op_zst_break(void)
{
	boolean_t	step_trace;
	intrpt_state_t	prev_intrpt_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((0 < gtm_trigger_depth) && (RESTRICTED(trigger_mod)))
		return;
	assert(INTRPT_IN_EVENT_HANDLING != intrpt_ok_state);
	DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
	DEFER_OUT_OF_XFER_TAB(is_tracing_on);
	FIX_XFER_ENTRY(xf_ret, opp_ret);
	FIX_XFER_ENTRY(xf_retarg, op_retarg);
	ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
	flush_pio();
	step_trace = is_tracing_on;
	is_tracing_on = FALSE;
	op_commarg(&(TREF(zstep_action)), indir_linetail);
	is_tracing_on = step_trace;
	frame_pointer->type = SFT_ZSTEP_ACT;
}
