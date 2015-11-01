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
#include "op.h"
#include "mupip_put_gvdata.h"

void mupip_put_gvdata(char *cp,int len)
{
	mval v;

	v.mvtype = MV_STR;
	v.str.addr = cp;
	v.str.len = len;
	op_gvput(&v);
	return;
}
