/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <sys/types.h>
#include "xfer_enum.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "deferred_events.h"
#include "fix_xfer_entry.h"
#include "error_trap.h"
#include "op.h"
#include "gtmio.h"
#include "io.h"
#include "gtmimagename.h"

/* ------------------------------------------------------------------
 * Set flags and transfer table for synchronous handling of cntl-C.
 * Should be called only from set_xfer_handlers.
 *
 * Note: dummy parameter is for calling compatibility.
 * ------------------------------------------------------------------
 */
GBLREF	boolean_t		ztrap_explicit_null;
GBLREF	dollar_ecode_type	dollar_ecode;
GBLREF	volatile boolean_t	ctrlc_on, dollar_zininterrupt;
GBLREF	volatile int4 		outofband;
GBLREF	xfer_entry_t		xfer_table[];

void ctrlc_set(int4 dummy_param)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(INTRPT_IN_EVENT_HANDLING == intrpt_ok_state);
	if (!(ctrlc_on && IS_MCODE_RUNNING))
	{
		DBGDFRDEVNT((stderr, "%d %s: ctrlc_set - ctrlc outofband not enabled\n", __LINE__, __FILE__));
		assert((pending == TAREF1(save_xfer_root, ctrlc).event_state)
			|| ((active == TAREF1(save_xfer_root, ctrlc).event_state)));
		return;
	}
	if (ctrlc != outofband)
	{	/* not a good time, so save it */
		TAREF1(save_xfer_root, ctrlc).event_state = queued;
		SAVE_XFER_QUEUE_ENTRY(ctrlc, 0);
		DBGDFRDEVNT((stderr, "%d %s: ctrlc_set - ctrlc queued - outofband: %d, trap: %d, intrpt: %d\n",
			 __LINE__, __FILE__, outofband, ((0 < dollar_ecode.index) && (ETRAP_IN_EFFECT)), dollar_zininterrupt));
		return;
	}
	DBGDFRDEVNT((stderr, "%d %s: ctrlc_set - NOT deferred\n", __LINE__, __FILE__));
	TAREF1(save_xfer_root, ctrap).param_val = 0;
	outofband = ctrlc;
	DEFER_INTO_XFER_TAB;
	DBGDFRDEVNT((stderr, "%d %s: ctrlc_set - pending xfer entries for ctrlc\n", __LINE__, __FILE__));
#	ifdef DEBUG
	if (ydb_white_box_test_case_enabled && (WBTEST_ZTIM_EDGE == ydb_white_box_test_case_number))
		DBGFPF((stderr, "# ctrlc_set: set the xfer entries for ctrlc\n"));
#	endif
}
