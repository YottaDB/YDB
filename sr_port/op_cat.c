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

#include "gtm_string.h"

#include "stringpool.h"
#include "op.h"
#include <stdarg.h>

#define MAX_NUM_LEN 64

GBLREF spdesc stringpool;

void op_cat(UNIX_ONLY_COMMA(int srcargs) mval *dst, ...)
{
	va_list var;
	mval *in, *src;
	int maxlen, i;
	VMS_ONLY(int srcargs;)
	unsigned char *cp, *base;
	error_def(ERR_MAXSTRLEN);

	VAR_START(var, dst);
	VMS_ONLY(va_count(srcargs);)
	srcargs -= 1;			/* account for dst */
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
		{
			va_end(var);
			rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
		}
	}
	va_end(var);
	if (stringpool.free + maxlen > stringpool.top)
		stp_gcol(maxlen);
	base = cp = stringpool.free;
	/* if the first string is at the end of the stringpool, then don't recopy the first string */
	VAR_START(var, dst);
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
	va_end(var);
	dst->mvtype = MV_STR;
	dst->str.addr = (char *) base;
	dst->str.len = INTCAST(cp - base);
	stringpool.free = cp;
	return;
}
