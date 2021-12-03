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
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "deferred_events.h"
#include "op.h"
#include "fix_xfer_entry.h"


/* ------------------------------------------------------------------------
 * Set flags and transfer table for synchronous handling of TTY write error
 * Should be called only from xfer_set_handlers.
 * ------------------------------------------------------------------------
 */
GBLREF xfer_entry_t     xfer_table[];

void tt_write_error_set(int4 error_status)
{
	DEFER_INTO_XFER_TAB;
}
