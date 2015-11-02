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
#include <rtnhdr.h>
#include "stack_frame.h"
#include "xfer_enum.h"
#include "indir_enum.h"
#include "op.h"
#include "io.h"
#include "fix_xfer_entry.h"

GBLREF stack_frame	*frame_pointer;
GBLREF mval 		zstep_action;
GBLREF xfer_entry_t	xfer_table[];

void op_zst_break(void)
{
	FIX_XFER_ENTRY(xf_linefetch, op_linefetch);
	FIX_XFER_ENTRY(xf_linestart, op_linestart);
	FIX_XFER_ENTRY(xf_zbfetch, op_zbfetch);
	FIX_XFER_ENTRY(xf_zbstart, op_zbstart);
	FIX_XFER_ENTRY(xf_ret, opp_ret);
	FIX_XFER_ENTRY(xf_retarg, op_retarg);

	flush_pio();
	op_commarg(&zstep_action,indir_linetail);
	zstep_action.mvtype = 0;
	frame_pointer->type = SFT_ZSTEP_ACT;
}

