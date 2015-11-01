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
#include "zstep.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "xfer_enum.h"
#include "indir_enum.h"
#include "op.h"

GBLREF int		(* volatile xfer_table[])();
GBLREF stack_frame	*frame_pointer;
GBLDEF unsigned char	*zstep_level;
GBLREF mval		zstep_action;
GBLREF mval		dollar_zstep;
GBLREF bool		neterr_pending;
GBLREF int4		outofband;

void op_zstep(uint4 code, mval *action)
{
	stack_frame	*fp;
	int4		status;

	if (!action)
		zstep_action = dollar_zstep;
	else
	{	op_commarg(action,indir_linetail);
		op_unwind();
		zstep_action = *action;
	}
	switch(code)
	{
		case ZSTEP_INTO:
			if (!neterr_pending && !outofband)
			{
				xfer_table[xf_linefetch] = op_zstepfetch;
				xfer_table[xf_linestart] = op_zstepstart;
				xfer_table[xf_zbfetch] = op_zstzbfetch;
				xfer_table[xf_zbstart] = op_zstzbstart;
			}
			break;
		case ZSTEP_OVER:
		case ZSTEP_OUTOF:

			fp = frame_pointer;
			for (fp = frame_pointer; fp ; fp = fp->old_frame_pointer)
			{	if (fp->type & SFT_COUNT)
				{	break;
				}
			}
			zstep_level = (unsigned char *) fp;
			if (!neterr_pending && !outofband)
			{
				if (code == ZSTEP_OVER)
				{
					xfer_table[xf_linefetch] = op_zst_fet_over;
					xfer_table[xf_linestart] = op_zst_st_over;
					xfer_table[xf_zbfetch] = op_zstzb_fet_over;
					xfer_table[xf_zbstart] = op_zstzb_st_over;
				}
				else
				{
					xfer_table[xf_ret] = opp_zstepret;
					xfer_table[xf_retarg] = opp_zstepretarg;
					xfer_table[xf_linefetch] = op_linefetch;
					xfer_table[xf_linestart] = op_linestart;
					xfer_table[xf_zbfetch] = op_zbfetch;
					xfer_table[xf_zbstart] = op_zbstart;
				}
			}
			break;
		default:
			GTMASSERT;
	}
}
