/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

/* ------------------------------------------------------------------
 * Set flags and transfer table for synchronous handling of  ctrap.
 * Should be called only from set_xfer_handlers.
 * ------------------------------------------------------------------
 */
GBLREF volatile int4 	ctrap_action_is;
GBLREF int		(* volatile xfer_table[])();
GBLREF volatile int4 	outofband;

void ctrap_set(int4 ob_char)
{
	int   op_fetchintrrpt(), op_startintrrpt(), op_forintrrpt();

	if (!outofband)
	{
		outofband = ctrap;
		ctrap_action_is = ob_char;
		xfer_table[xf_linefetch] = op_fetchintrrpt;
		xfer_table[xf_linestart] = op_startintrrpt;
		xfer_table[xf_zbfetch] = op_fetchintrrpt;
		xfer_table[xf_zbstart] = op_startintrrpt;
		xfer_table[xf_forchk1] = op_forintrrpt;
		xfer_table[xf_forloop] = op_forintrrpt;
	}
}
