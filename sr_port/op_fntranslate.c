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

void op_fntranslate(mval *src,mval *in_str,mval *out_str,mval *dst)
{
	short int xlate[256];
	unsigned char *inpt,*intop,*outpt,*dstp;
	int n;
	unsigned char ch;
	MV_FORCE_STR(src);
	MV_FORCE_STR(in_str);
	MV_FORCE_STR(out_str);
	if (stringpool.top - stringpool.free < src->str.len)
		stp_gcol(src->str.len);
	memset(xlate,0xFF,sizeof(xlate));
	n = in_str->str.len < out_str->str.len ? in_str->str.len : out_str->str.len;
	for (inpt = (unsigned char *)in_str->str.addr,
		outpt = (unsigned char *)out_str->str.addr,
		intop = inpt + n; inpt < intop ; inpt++, outpt++ )
		if(xlate[*inpt] == -1)
		    xlate[*inpt] = *outpt;
	for (intop = (unsigned char *)in_str->str.addr + in_str->str.len ; inpt < intop ; inpt++)
		if(xlate[*inpt] == -1)
		    xlate[*inpt] = -2;
	dstp = outpt = stringpool.free;
	for (inpt = (unsigned char *)src->str.addr, intop = inpt + src->str.len ; inpt < intop ; )
	{
		n = xlate[ch = *inpt++];
		if (n >= 0)
			*outpt++ = n;
		else if (n == -1)
			*outpt++ = ch;
	}
	dst->str.addr = (char *)dstp;
	dst->str.len = outpt - dstp;
	dst->mvtype = MV_STR;
	stringpool.free = outpt;
}
