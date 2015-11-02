/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"

#include "compiler.h"
#include "stringpool.h"
#include "op.h"

GBLREF spdesc stringpool;

void op_indname(UNIX_ONLY_COMMA(int argcnt) mval *dst, ...)
{
	va_list		var, sublst;
	mval		*src, *subs;
	int		i;
	VMS_ONLY(int	argcnt;)
	unsigned char	*ch, *ctop, *out, *double_chk;
	unsigned char	outch;

	error_def(ERR_INDRMAXLEN);
	error_def(ERR_STRINGOFLOW);
	error_def(ERR_VAREXPECTED);

	VAR_START(var, dst);
	VMS_ONLY(va_count(argcnt);)

	VAR_COPY(sublst,  var);
	src = va_arg(var, mval *);
	for (i = argcnt - 1 ; i > 0 ; i-- )
	{
		subs = va_arg(sublst, mval *);
		MV_FORCE_STR(subs);
	}
	va_end(sublst);
	ENSURE_STP_FREE_SPACE(MAX_SRCLINE);
	out = stringpool.free;
	if (out + src->str.len > stringpool.top)
		rts_error(VARLSTCNT(3) ERR_INDRMAXLEN, 1, MAX_SRCLINE);
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
		} else
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
			} else if (*out != ')')
			{
				outch = '(';
				out++;
			} else
				outch = ',';
		}
	} else if (*out != ')')
	{
		outch = '(';
		out++;
	} else
		outch = ',';
	VAR_COPY(sublst,  var);
	for (i = argcnt - 2 ; i > 0 ; i--)
	{
		subs = va_arg(sublst, mval * );
		/* Note that in this second pass of the mvals, if the mval was undefined in the first pass and we are in
		   NOUNDEF mode, that the mval was not modified and is again undefined. Make sure this incarnation is defined..
		*/
		MV_FORCE_DEFINED(subs);
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
	va_end(var);
	va_end(sublst);
	assert(out < stringpool.top);
	assert(out > stringpool.free);
	*out++ = ')';
	dst->mvtype = MV_STR;
	dst->str.addr = (char *)stringpool.free;
	dst->str.len = INTCAST((char *)out - dst->str.addr);
	stringpool.free = out;
	return;
}
