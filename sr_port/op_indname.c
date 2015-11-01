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
#include "compiler.h"
#include "stringpool.h"
#include <varargs.h>
#include "op.h"

GBLREF spdesc stringpool;

void op_indname(va_alist)
va_dcl
{
	va_list	var, sublst;
	mval *dst, *src;
	mval  *subs;
	int argcnt, i;
	unsigned char *ch, *ctop, *out, *double_chk;
	unsigned char outch;
	error_def(ERR_INDRMAXLEN);
	error_def(ERR_STRINGOFLOW);
	error_def(ERR_VAREXPECTED);

	VAR_START(var);
	argcnt = va_arg(var, int4);
	dst = va_arg(var, mval *);

	VAR_COPY(sublst,  var);
	src = va_arg(var, mval *);
	for (i = argcnt - 1 ; i > 0 ; i-- )
	{
		subs = va_arg(sublst, mval *);
		MV_FORCE_STR(subs);
	}
	if (stringpool.free + MAX_SRCLINE > stringpool.top)
		stp_gcol(MAX_SRCLINE);
	out = stringpool.free;
	if (out + src->str.len > stringpool.top)
		rts_error(VARLSTCNT(1) ERR_INDRMAXLEN);
	if (src->str.len < 1)
		rts_error(VARLSTCNT(1) ERR_VAREXPECTED);
	memcpy(out , src->str.addr, src->str.len);
	out += src->str.len - 1;
	if (*stringpool.free == '@')
	{
		double_chk = stringpool.free + 1;
		if (*double_chk == '@')
		{
			out++;
			*out++ = '@';
			outch = '(';
		}
		else
		{
			while(double_chk <= out)
				if (*double_chk == '@')
					break;
				else
					double_chk++;

			if (double_chk > out)
			{
				out++;
				*out++ = '@';
				outch = '(';
			}
			else if (*out != ')')
			{
				outch = '(';
				out++;
			}
			else
			{
				outch = ',';
			}
		}
	}
	else if (*out != ')')
	{
		outch = '(';
		out++;
	}
	else
	{
		outch = ',';
	}
	VAR_COPY(sublst,  var);
	for (i = argcnt - 2 ; i > 0 ; i--)
	{
		subs = va_arg(sublst, mval * );
		ch = (unsigned char *)subs->str.addr;
		ctop = ch + subs->str.len;
		if (stringpool.top - out < subs->str.len + 5)
			rts_error(VARLSTCNT(1) ERR_STRINGOFLOW);
		*out++ = outch;
		outch = ',';
		*out++ = '"';
		for ( ; ch < ctop ; ch++)
		{
			if (*ch < 32)
			{
				if (stringpool.top - out < ctop - ch + 11)
					rts_error(VARLSTCNT(1) ERR_STRINGOFLOW);
				*out++ = '"';
				*out++ = '_';
				*out++ = '$';
				*out++ = 'C';
				*out++ = '(';
				if (*ch > 9)
					*out++ = (*ch / 10) + '0';
				*out++ = (*ch % 10) + '0';
				*out++ = ')';
				*out++ = '_';
				*out++ = '"';
			} else
			{
				if (*ch == '"')
				{
					if (stringpool.top - out < ctop - ch + 3)
						rts_error(VARLSTCNT(1) ERR_STRINGOFLOW);
					*out++ = '"';
				}
				*out++ = *ch;
			}
		}
		*out++ = '"';
	}
	assert(out < stringpool.top);
	assert(out > stringpool.free);
	*out++ = ')';
	dst->mvtype = MV_STR;
	dst->str.addr = (char *)stringpool.free;
	dst->str.len = (char *)out - dst->str.addr;
	stringpool.free = out;
	return;
}
