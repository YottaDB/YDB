/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
error_def(ERR_VAREXPECTED);

void op_indname(mval *dst, mval *target, mval *subs)
{
	boolean_t	quoted;
	int		i;
	unsigned char	*cp, *end, *out, *start;

	MV_FORCE_STR(target);
	if (0 == target->str.len)
		stx_error(ERR_VAREXPECTED);
	MV_FORCE_STR(subs);
	if ((target->str.len + subs->str.len) > MAX_SRCLINE)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_INDRMAXLEN, 1, MAX_SRCLINE);
	ENSURE_STP_FREE_SPACE(MAX_SRCLINE);
	start = out = stringpool.free;
	for (cp = (unsigned char *)target->str.addr, i = target->str.len, quoted = 0; i; i--)
	{	/* simple parsing transfer rather than memcpy so an embedded "comment" doesn't cause premature truncation */
		if ('"' == *cp)
			quoted	= !quoted;
		if ((';' == *cp) && !quoted)
			break;		/* comment delimiter means the value is complete */
		*out++ = *cp++;
	}
	assert(!quoted);
	if ((*start == '@') && (0 != subs->str.len))
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
