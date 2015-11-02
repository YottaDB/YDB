/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "compiler.h"
#include "stringpool.h"
#include "op.h"

GBLREF spdesc stringpool;

error_def(ERR_INDRMAXLEN);
error_def(ERR_STRINGOFLOW);
error_def(ERR_VAREXPECTED);

void op_indname(mval *dst, mval *target, mval *subs)
{
	int		i;
	unsigned char	*out, *end, *start;

	MV_FORCE_STR(target);
	MV_FORCE_STR(subs);
	ENSURE_STP_FREE_SPACE(MAX_SRCLINE);
	start = out = stringpool.free;
	if ((target->str.len + subs->str.len) > MAX_SRCLINE)
		rts_error(VARLSTCNT(3) ERR_INDRMAXLEN, 1, MAX_SRCLINE);
	memcpy(out, target->str.addr, target->str.len);
	out += target->str.len;
	if (*start == '@')
	{
		*out++ = '@';
		*out++ = '#';
		*out++ = '(';
	} else
	{
		end = out - 1;
		if (*end == ')')
			*end = ',';	/* replace trailing ')' with a comma since we're appending more subscripts */
		else
			*out++ = '(';
	}
	memcpy(out, subs->str.addr, subs->str.len);
	out += subs->str.len;
	dst->mvtype = MV_STR;
	dst->str.addr = (char *)stringpool.free;
	dst->str.len = INTCAST((char *)out - dst->str.addr);
	stringpool.free = out;
	return;
}
