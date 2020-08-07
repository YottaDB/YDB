/****************************************************************
 *								*
 * Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
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
#include "outofband.h"
#include "deferred_events.h"
#include "fix_xfer_entry.h"
#include "op.h"
#include "gtmimagename.h"

/* ------------------------------------------------------------------
 * Set flags and transfer table for synchronous handling of cntl-C.
 * Should be called only from set_xfer_handlers.
 *
 * Note: dummy parameter is for calling compatibility.
 * ------------------------------------------------------------------
 */
GBLREF volatile boolean_t		ctrlc_on;
GBLREF volatile int4			ctrap_action_is, outofband;
GBLREF xfer_entry_t			xfer_table[];

void ctrlc_set(int4 dummy_param)
{
	if (!outofband && IS_MCODE_RUNNING && ctrlc_on)
	{
		ctrap_action_is = 0;
		SET_OUTOFBAND(ctrlc);
	}
}
