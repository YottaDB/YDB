/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cmd_qlf.h"
#include "rtnhdr.h"
#include "op.h"
#include "job_addr.h"
#include "zbreak.h"

void job_addr(mstr *rtn, mstr *label, int4 offset, char **hdr, char **labaddr)
{
	rhdtyp		*rt_hdr;
	int4		*lp;
	error_def	(ERR_JOBLABOFF);

	if ((rt_hdr = find_rtn_hdr(rtn)) == 0)
	{
		mval rt;

		rt.mvtype = MV_STR;
		rt.str = *rtn;
		op_zlink(&rt,0);
		if ((rt_hdr = find_rtn_hdr (rtn)) == 0)
			GTMASSERT;
	}
	lp = NULL;
	if ((rt_hdr->compiler_qlf & CQ_LINE_ENTRY) || 0 == offset)
	{ /* label offset with routine compiled with NOLINE_ENTRY should cause error */
		lp = find_line_addr(rt_hdr, label, offset, NULL);
	}
	if (!lp)
		rts_error(VARLSTCNT(1) ERR_JOBLABOFF);
	*labaddr = (char *) LINE_NUMBER_ADDR(rt_hdr, lp);
	*hdr = (char *)rt_hdr;
}
