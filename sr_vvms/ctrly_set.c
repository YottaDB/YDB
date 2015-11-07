/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "efn.h"
#include <ssdef.h>
#include "xfer_enum.h"
#include "outofband.h"
#include "op.h"
#include "deferred_events.h"
#include "gtmimagename.h"

/* ------------------------------------------------------------------
 * Set flags and transfer table for synchronous handling of cntl-Y.
 * Should be called only from set_xfer_handlers.
 *
 * Note: dummy parameter is for calling compatibility.
 * ------------------------------------------------------------------
 */

GBLREF int			(* volatile xfer_table[])();
GBLREF volatile int4		outofband, ctrap_action_is;

void ctrly_set(int4 dummy_param)
{
	int4 status;

	if (!IS_MCODE_RUNNING)
	{
		outofband = ctrly;
		outofband_clear();
	} else
	{
		status = sys$setef(efn_outofband);
		assert(SS$_WASCLR == status);
		if (status != SS$_WASCLR && status != SS$_WASSET)
			GTMASSERT;
		ctrap_action_is = 0;
		outofband = ctrly;
		xfer_table[xf_linefetch] = op_fetchintrrpt;
		xfer_table[xf_linestart] = op_startintrrpt;
		xfer_table[xf_zbfetch] = op_fetchintrrpt;
		xfer_table[xf_zbstart] = op_startintrrpt;
		xfer_table[xf_forchk1] = op_startintrrpt;
		xfer_table[xf_forloop] = op_forintrrpt;
		sys$wake(0,0);
	}
}
