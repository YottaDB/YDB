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
#include "stringpool.h"
#include "op.h"

GBLREF spdesc stringpool;

void op_fnj2(mval *src,int len,mval *dst)
{
	register unsigned char *cp;
	int n;
	error_def	(ERR_MAXSTRLEN);

	if (len > MAX_STRLEN)
		rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);

	MV_FORCE_STR(src);
	n = len - src->str.len;
	if (n <= 0)
	{	*dst = *src;
	}
	else
	{
		if (stringpool.top - stringpool.free < len)
			stp_gcol(len);
		cp = stringpool.free;
		stringpool.free += len;
		memset(cp,SP,n);
		memcpy(cp + n, src->str.addr, src->str.len);
		dst->mvtype = MV_STR;
		dst->str.len = len;
		dst->str.addr = (char *)cp;
	}
	return;
}
