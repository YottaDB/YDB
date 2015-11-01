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
#include "rtnhdr.h"
#include "stack_frame.h"
#include "xfer_enum.h"
#include "indir_enum.h"
#include "op.h"
#include "io.h"

GBLREF stack_frame	*frame_pointer;
GBLREF mval 		zstep_action;
GBLREF int 		(* volatile xfer_table[])();

void op_zst_break(void)
{
	xfer_table[xf_linefetch] = op_linefetch;
	xfer_table[xf_linestart] = op_linestart;
	xfer_table[xf_zbfetch] = op_zbfetch;
	xfer_table[xf_zbstart] = op_zbstart;
	xfer_table[xf_ret] = opp_ret;
	xfer_table[xf_retarg] = op_retarg;

	flush_pio();
	op_commarg(&zstep_action,indir_linetail);
	zstep_action.mvtype = 0;
	frame_pointer->type = SFT_ZSTEP_ACT;
}

