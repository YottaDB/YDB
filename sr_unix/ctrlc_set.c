/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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


/* ------------------------------------------------------------------
 * Set flags and transfer table for synchronous handling of cntl-C.
 * Should be called only from set_xfer_handlers.
 *
 * Note: dummy parameter is for calling compatibility.
 * ------------------------------------------------------------------
 */
GBLREF xfer_entry_t			xfer_table[];
GBLREF volatile bool 			run_time, ctrlc_on;
GBLREF volatile int4			ctrap_action_is,outofband;

void ctrlc_set(int4 dummy_param)
{
	int 		op_fetchintrrpt(), op_startintrrpt(), op_forintrrpt();

	if (!outofband && run_time)
	{	if (ctrlc_on)
		{
			ctrap_action_is = 0;
			outofband = ctrlc;
               		FIX_XFER_ENTRY(xf_linefetch, op_fetchintrrpt);
	                FIX_XFER_ENTRY(xf_linestart, op_startintrrpt);
	                FIX_XFER_ENTRY(xf_zbfetch, op_fetchintrrpt);
        	        FIX_XFER_ENTRY(xf_zbstart, op_startintrrpt);
	                FIX_XFER_ENTRY(xf_forchk1, op_startintrrpt);
	                FIX_XFER_ENTRY(xf_forloop, op_forintrrpt);
		}
	}
}
