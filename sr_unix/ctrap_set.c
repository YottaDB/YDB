/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
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
#include "op.h"
#include "gtmio.h"
#include "io.h"
#include "gtmimagename.h"

/* ------------------------------------------------------------------
 * Set flags and transfer table for synchronous handling of  ctrap.
 * Should be called only from set_xfer_handlers.
 * ------------------------------------------------------------------
 */
GBLREF	boolean_t		ztrap_explicit_null;
GBLREF	dollar_ecode_type	dollar_ecode;
GBLREF	volatile boolean_t	dollar_zininterrupt;
GBLREF	volatile int4 		outofband;
GBLREF	xfer_entry_t		xfer_table[];

void ctrap_set(int4 ob_char)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(INTRPT_IN_EVENT_HANDLING == intrpt_ok_state);
<<<<<<< HEAD
	if ((ctrap != outofband) || dollar_zininterrupt || (jobinterrupt == (TREF(save_xfer_root_ptr))->ev_que.fl->outofband))
=======
	if ((((CTRLC == ob_char) ? ctrap : sighup) != outofband) || have_crit(CRIT_HAVE_ANY_REG | CRIT_IN_COMMIT)
		 || dollar_zininterrupt || (jobinterrupt == (TREF(save_xfer_root_ptr))->ev_que.fl->outofband))
>>>>>>> eb3ea98c (GT.M V7.0-002)
	{	/* not a good time, so save it */
		TAREF1(save_xfer_root, ctrap).event_state = queued;
		SAVE_XFER_QUEUE_ENTRY(ctrap, 0);
		DBGDFRDEVNT((stderr, "%d %s: ctrap_set - ctrap queued - outofband: %d, trap: %d, intrpt: %d\n",
			     __LINE__, __FILE__, outofband, ((0 < dollar_ecode.index) && (ETRAP_IN_EFFECT)), dollar_zininterrupt));
		return;
	}
	DBGDFRDEVNT((stderr, "%d %s: ctrap_set - NOT deferred\n", __LINE__, __FILE__));
	TAREF1(save_xfer_root, ctrap).param_val = ob_char;
	outofband = (CTRLC == ob_char) ? ctrap : sighup;
	DEFER_INTO_XFER_TAB;
	DBGDFRDEVNT((stderr, "%d %s: ctrap_set - pending xfer entries for ctrap - outofband: %d\n", __LINE__, __FILE__, outofband));
#	ifdef DEBUG
	if (ydb_white_box_test_case_enabled && (WBTEST_ZTIM_EDGE == ydb_white_box_test_case_number))
		DBGFPF((stderr, "# ctrap_set: set the xfer entries for ctrap\n"));
#	endif
}
