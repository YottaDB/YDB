/****************************************************************
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

#include "xfer_enum.h"
#include "fix_xfer_entry.h"
#include "op.h"
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "deferred_signal_set.h"

GBLREF	volatile int	in_os_signal_handler;
GBLREF	volatile int4	outofband;

/* Function that is invoked when a signal handling is deferred in the SET_FORCED_EXIT_STATE macro */

/* The below is modeled on "jobinterrupt_set()" */
void deferred_signal_set(int4 dummy_val)
{
	assert(in_os_signal_handler);
	assert(deferred_signal == outofband);
	/* We need deferred signal outofband processing at our earliest convenience */
	DEFER_INTO_XFER_TAB;
}
