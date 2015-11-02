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
#include "op.h"
#include "fix_xfer_entry.h"

GBLREF xfer_entry_t	xfer_table[];
GBLREF bool 		neterr_pending;
GBLREF int4 		outofband;
GBLREF int 		iott_write_error;

void op_zst_over(void)
{
	if (!neterr_pending && 0 == outofband && 0 == iott_write_error)
	{
                    FIX_XFER_ENTRY(xf_linefetch, op_linefetch);
                    FIX_XFER_ENTRY(xf_linestart, op_linestart);
                    FIX_XFER_ENTRY(xf_zbfetch, op_zbfetch);
                    FIX_XFER_ENTRY(xf_zbstart, op_zbstart);
                    FIX_XFER_ENTRY(xf_ret,opp_zst_over_ret);
                    FIX_XFER_ENTRY(xf_retarg, opp_zst_over_retarg);
	}
}
