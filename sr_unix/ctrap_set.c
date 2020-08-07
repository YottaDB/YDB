/****************************************************************
 *								*
 * Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

#include <sys/types.h>

#include "xfer_enum.h"
#include "outofband.h"
#include "deferred_events.h"
#include "fix_xfer_entry.h"

/* ------------------------------------------------------------------
 * Set flags and transfer table for synchronous handling of  ctrap.
 * Should be called only from set_xfer_handlers.
 * ------------------------------------------------------------------
 */
GBLREF volatile int4 	ctrap_action_is;
GBLREF xfer_entry_t	xfer_table[];
GBLREF volatile int4 	outofband;

void ctrap_set(int4 ob_char)
{
	int   op_fetchintrrpt(), op_startintrrpt(), op_forintrrpt();

	if (!outofband)
	{
		SET_OUTOFBAND(ctrap);
		ctrap_action_is = ob_char;
	}
}
