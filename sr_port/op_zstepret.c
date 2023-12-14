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
#include "zstep.h"
#include "xfer_enum.h"
#include "op.h"
#include "fix_xfer_entry.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "restrict.h"

GBLREF bool		neterr_pending;
GBLREF intrpt_state_t	intrpt_ok_state;
GBLREF volatile int4	outofband;
GBLREF xfer_entry_t	xfer_table[];

void op_zstepret(void)
{
	intrpt_state_t	prev_intrpt_state;

	assert(INTRPT_IN_EVENT_HANDLING != intrpt_ok_state);
	DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
	if (!neterr_pending && (no_event == outofband))
	{
		FIX_XFER_ENTRY(xf_linefetch, op_zst_fet_over);	/* shim: zstep_level gates call to op_zst_break */
		FIX_XFER_ENTRY(xf_linestart, op_zst_st_over);	/* shim: zstep_level gates call to op_zst_break */
		FIX_XFER_ENTRY(xf_zbfetch, op_zstzb_fet_over);	/* shim: zstep_level gates call to op_zst_break */
		FIX_XFER_ENTRY(xf_zbstart, op_zstzb_st_over);	/* shim: zstep_level gates call to op_zst_break */
		FIX_XFER_ENTRY(xf_ret, opp_ret);		/* default/normal transfer */
		FIX_XFER_ENTRY(xf_retarg, op_retarg);		/* default/normal transfer */
	}
	ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
}
