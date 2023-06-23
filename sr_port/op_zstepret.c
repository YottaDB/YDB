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
#include "zstep.h"
#include "xfer_enum.h"
#include "op.h"
#include "fix_xfer_entry.h"

GBLREF xfer_entry_t	xfer_table[];
GBLREF bool	neterr_pending;
GBLREF int4	outofband;
GBLREF int	iott_write_error;

void op_zstepret(void)
{
	if (!neterr_pending && 0 == outofband && 0 == iott_write_error)
	{
                    FIX_XFER_ENTRY(xf_linefetch, op_zst_fet_over);
                    FIX_XFER_ENTRY(xf_linestart, op_zst_st_over);
                    FIX_XFER_ENTRY(xf_zbfetch, op_zstzb_fet_over);
                    FIX_XFER_ENTRY(xf_zbstart, op_zstzb_st_over);
                    FIX_XFER_ENTRY(xf_ret, opp_ret);
                    FIX_XFER_ENTRY(xf_retarg, op_retarg);
	}
}
