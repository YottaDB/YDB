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
#include <varargs.h>

#define MAX_NUM_LEN 64

GBLREF spdesc stringpool;

void op_cat(va_alist)
va_dcl
{
	va_list var, argbase;
	mval *in, *dst, *src;
	int maxlen;
	int srcargs, i;
	unsigned char *cp, *base;
	error_def(ERR_MAXSTRLEN);

	VAR_START(var);
	srcargs = va_arg(var,int4) - 1;
	dst = va_arg(var, mval *);
	argbase = var;
	/* determine if garbage collection is required */
	maxlen = 0;
	for (i = 0; i < srcargs ; i++)
	{
		/* Sum the lengths of all the strings;
		   use MAX_NUM_LEN to estimate lengths of strings from to-be-converted numbers.
		*/
		in = va_arg(var, mval *);
		maxlen += (MV_IS_STRING(in)) ? (in)->str.len : MAX_NUM_LEN;
		if (maxlen > MAX_STRLEN)
			rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
	}
	if (stringpool.free + maxlen > stringpool.top)
		stp_gcol(maxlen);
	base = cp = stringpool.free;
	/* if the first string is at the end of the stringpool, then don't recopy the first string */
	var = argbase;
	in = va_arg(var, mval *);
	if (MV_IS_STRING(in)  &&  (unsigned char *)in->str.addr + in->str.len == cp)
	{
		/* If the first string is at the end of the stringpool, no sense in copying it.  */
		base = (unsigned char *)in->str.addr;
		in = va_arg(var, mval *);
		srcargs--;
	}
	for(i = 0; ;)
	{
		if (MV_IS_STRING(in))
			memcpy(cp, in->str.addr, in->str.len);
		else
		{	stringpool.free = cp;
			n2s(in);
			/* Convert to string, rely on the fact that it will be converted exactly at the
				end of the stringpool.
			*/
		}
		cp += in->str.len;
		i++;
		if (i >= srcargs)
			break;
		in = va_arg(var, mval *);
	}
	dst->mvtype = MV_STR;
	dst->str.addr = (char *) base;
	dst->str.len = cp - base;
	stringpool.free = cp;
	return;
}
