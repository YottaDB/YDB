/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
		outofband = (CTRLC == ob_char) ? ctrap : sighup;
		ctrap_action_is = ob_char;
	        FIX_XFER_ENTRY(xf_linefetch, op_fetchintrrpt);
       		FIX_XFER_ENTRY(xf_linestart, op_startintrrpt);
	        FIX_XFER_ENTRY(xf_zbfetch, op_fetchintrrpt);
	        FIX_XFER_ENTRY(xf_zbstart, op_startintrrpt);
	        FIX_XFER_ENTRY(xf_forchk1, op_startintrrpt);
	        FIX_XFER_ENTRY(xf_forloop, op_forintrrpt);
	}
}
