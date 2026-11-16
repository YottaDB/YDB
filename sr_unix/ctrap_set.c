/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2026 YottaDB LLC and/or its subsidiaries.	*
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
GBLREF	volatile int4 		outofband;
GBLREF	xfer_entry_t		xfer_table[];

void ctrap_set(int4 ob_char)
{
	int4	event_type;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(INTRPT_IN_EVENT_HANDLING == intrpt_ok_state);
	/* "xfer_set_handlers" sets "outofband" to the event (ctrap or sighup) before it invokes this function, except when
	 * "xfer_set_handlers" popped an earlier event from the queue to run first: "outofband" then names that event, and the
	 * code below queues the ctrap. In every other case, the code below does not queue the event, even in a $ZINTERRUPT
	 * handler or behind a jobinterrupt: the READ that gets the character ("iott_readfl") acts on the event right away, and
	 * with no READ the transfer table entries set below act on it at the next M line. Queuing it would also leave a queued
	 * event in "outofband", which must not happen (see "deferred_events_queue.h"). The event comes from "outofband", not
	 * from "ob_char", because a CTRAP can name any control character, not just <CTRL-C>, so "ob_char" does not tell ctrap
	 * and sighup apart.
	 */
	event_type = outofband;
	if ((ctrap != event_type) && (sighup != event_type))
	{	/* "xfer_set_handlers" popped an earlier event from the queue and set "outofband" to it, so queue the ctrap */
		TAREF1(save_xfer_root, ctrap).event_state = queued;
		SAVE_XFER_QUEUE_ENTRY(ctrap, ob_char);
		DBGDFRDEVNT((stderr, "%d %s: ctrap_set - ctrap queued - outofband: %d, trap: %d\n",
			     __LINE__, __FILE__, outofband, ((0 < dollar_ecode.index) && (ETRAP_IN_EFFECT))));
		return;
	}
	DBGDFRDEVNT((stderr, "%d %s: ctrap_set - NOT deferred\n", __LINE__, __FILE__));
	TAREF1(save_xfer_root, event_type).param_val = ob_char;
	DEFER_INTO_XFER_TAB;
	DBGDFRDEVNT((stderr, "%d %s: ctrap_set - pending xfer entries for event %d - outofband: %d\n", __LINE__, __FILE__,
		     event_type, outofband));
#	ifdef DEBUG
	if (ydb_white_box_test_case_enabled && (WBTEST_ZTIM_EDGE == ydb_white_box_test_case_number))
		DBGFPF((stderr, "# ctrap_set: set the xfer entries for ctrap\n"));
#	endif
}
