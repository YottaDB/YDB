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

#include "gtm_string.h"
#include "xfer_enum.h"
#include "outofband.h"
#include "msg.h"
#include "op.h"
#include "gtmimagename.h"

/* ------------------------------------------------------------------
 * Set flags and transfer table for synchronous handling of cntl-C.
 * Should be called only from set_xfer_handlers.
 *
 * Note: dummy parameter is for calling compatibility.
 * ------------------------------------------------------------------
 */

GBLREF volatile char 			source_file_name[];
GBLREF int 				(* volatile xfer_table[])();
GBLREF volatile bool			ctrlc_on;
GBLREF volatile int4			ctrap_action_is, outofband;

ctrlc_set(int4 dummy_param)
{
	int4		status;
	msgtype		message;
	error_def(ERR_LASTFILCMPLD);

	if (!IS_MCODE_RUNNING)
	{
		message.arg_cnt = 4;
		message.def_opts = message.new_opts = 0;
		message.msg_number = ERR_LASTFILCMPLD;
		message.fp_cnt = 2;
		message.fp[0].n = strlen(source_file_name);
		message.fp[1].cp = source_file_name;
		sys$putmsg(&message, 0, 0, 0);
	} else if (!outofband)
	{
		if (ctrlc_on)
		{
			status = sys$setef(efn_outofband);
			assert(SS$_WASCLR == status);
			if (status != SS$_WASCLR && status != SS$_WASSET)
				GTMASSERT;
			ctrap_action_is = 0;
			outofband = ctrlc;
			xfer_table[xf_linefetch] = op_fetchintrrpt;
			xfer_table[xf_linestart] = op_startintrrpt;
			xfer_table[xf_zbfetch] = op_fetchintrrpt;
			xfer_table[xf_zbstart] = op_startintrrpt;
			xfer_table[xf_forchk1] = op_startintrrpt;
			xfer_table[xf_forloop] = op_forintrrpt;
			sys$wake(0,0);
		}
	}
}
