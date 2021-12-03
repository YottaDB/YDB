/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "xfer_enum.h"
#include "op.h"
#include "mprof.h"
#include "fix_xfer_entry.h"
#include "have_crit.h"
#include "deferred_events_queue.h"

GBLREF bool 		neterr_pending;
GBLREF boolean_t	is_tracing_on;
GBLREF volatile int4	outofband;
GBLREF xfer_entry_t	xfer_table[];

void op_zst_over(void)
{
	intrpt_state_t		prev_intrpt_state;

	DEFER_INTERRUPTS(INTRPT_IN_EVENT_HANDLING, prev_intrpt_state);
	if (!neterr_pending && (no_event != outofband))
	{
		DEFER_OUT_OF_XFER_TAB(is_tracing_on);
		FIX_XFER_ENTRY(xf_ret,opp_zst_over_ret);
		FIX_XFER_ENTRY(xf_retarg, opp_zst_over_retarg);
	}
	ENABLE_EVENT_INTERRUPTS(prev_intrpt_state);
}
