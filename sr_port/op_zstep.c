/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "zstep.h"
#include "stack_frame.h"
#include "xfer_enum.h"
#include "indir_enum.h"
#include "op.h"
#include "fix_xfer_entry.h"
#include "restrict.h"

GBLREF xfer_entry_t     xfer_table[];
GBLREF stack_frame	*frame_pointer;
GBLDEF unsigned char	*zstep_level;
GBLREF mval		zstep_action;
GBLREF bool		neterr_pending;
GBLREF int4		outofband;
GBLREF int		iott_write_error;
GBLREF int4		gtm_trigger_depth;

void op_zstep(uint4 code, mval *action)
{
	int4		status;
	stack_frame	*fp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((0 < gtm_trigger_depth) && RESTRICTED(trigger_mod))
		return;
	if (neterr_pending || outofband || iott_write_error)
		return;
	if (!action)
		zstep_action = TREF(dollar_zstep);
	else
	{	op_commarg(action,indir_linetail);
		op_unwind();
		zstep_action = *action;
	}
	switch(code)
	{
		case ZSTEP_INTO:
			FIX_XFER_ENTRY(xf_linefetch, op_zstepfetch);
			FIX_XFER_ENTRY(xf_linestart, op_zstepstart);
			FIX_XFER_ENTRY(xf_zbfetch, op_zstzbfetch);
			FIX_XFER_ENTRY(xf_zbstart, op_zstzbstart);
			break;
		case ZSTEP_OVER:
		case ZSTEP_OUTOF:

			for (fp = frame_pointer; fp && !(fp->type & SFT_COUNT); fp = fp->old_frame_pointer)
				; /* skip uncounted frames (if any) in storing "zstep_level" (when ZSTEP OVER/OUTOF action needs to kick in) */
			zstep_level = (unsigned char *)fp;
			if (ZSTEP_OVER == code)
			{
				FIX_XFER_ENTRY(xf_linefetch, op_zst_fet_over);
				FIX_XFER_ENTRY(xf_linestart, op_zst_st_over);
				FIX_XFER_ENTRY(xf_zbfetch, op_zstzb_fet_over);
				FIX_XFER_ENTRY(xf_zbstart, op_zstzb_st_over);
			} else
			{
				FIX_XFER_ENTRY(xf_ret, opp_zstepret);
				FIX_XFER_ENTRY(xf_retarg, opp_zstepretarg);
				FIX_XFER_ENTRY(xf_linefetch, op_linefetch);
				FIX_XFER_ENTRY(xf_linestart, op_linestart);
				FIX_XFER_ENTRY(xf_zbfetch, op_zbfetch);
				FIX_XFER_ENTRY(xf_zbstart, op_zbstart);
			}
			break;
		default:
			assertpro(FALSE && code);
	}
	return;
}
