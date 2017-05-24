/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
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
#include "stringpool.h"
#include "op.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "mv_stent.h"

#define PLUS 	1
#define MINUS 	2
#define TRAIL	4
#define COMMA	8
#define PAREN 	16
#define FNERROR 7

GBLREF spdesc	stringpool;
GBLREF mv_stent	*mv_chain;

LITREF mval	literal_zero;

error_def(ERR_FNARGINC);
error_def(ERR_FNUMARG);

void op_fnfnumber(mval *src, mval *fmt, boolean_t use_fract, int fract, mval *dst)
{
	boolean_t	comma, paren;
	int 		ct, x, xx, y, z;
	unsigned char	*ch, *cp, *ff, *ff_top, fncode, sign, *t;

	if (!MV_DEFINED(fmt))		/* catch this up front so noundef mode can't cause trouble - so fmt no empty context */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(2) ERR_FNUMARG, 0);
	/* if the dst will be different than the src we'll build the new value in the string pool and repoint dst there,
	 * otherwise, dst will anyway become the same as src, therefore we can safely use dst as a "temporary" copy of src
	 */
	*dst = *src;
	if (use_fract)
		op_fnj3(dst, 0, fract, dst);
	else
	{
		MV_FORCE_NUM(dst);
		MV_FORCE_CANONICAL(dst);	/* if the source operand is not a canonical number, force conversion */
	}
	assert (stringpool.free >= stringpool.base);
	assert (stringpool.free <= stringpool.top);
	/* assure there is adequate space for two string forms of a number as a local
	 * version of the src must be operated upon in order to get a canonical number
	 */
	MV_FORCE_STR(fmt);
	MV_FORCE_STR(dst);
	if (0 == fmt->str.len)
		return;
	ENSURE_STP_FREE_SPACE(MAX_NUM_SIZE * 2);
	ch = (unsigned char *)dst->str.addr;
	ct = dst->str.len;
	cp = stringpool.free;
	fncode = 0;
	for (ff = (unsigned char *)fmt->str.addr, ff_top = ff + fmt->str.len; ff < ff_top;)
	{
		switch(*ff++)
		{
			case '+':
				fncode |= PLUS;
				break;
			case  '-':
				fncode |= MINUS;
				break;
			case  ',':
				fncode |= COMMA;
				break;
			case  'T':
			case  't':
				fncode |= TRAIL;
				break;
			case  'P':
			case  'p':
				fncode |= PAREN;
				break;
			default:
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_FNUMARG, 4, fmt->str.len, fmt->str.addr, 1, --ff);
			break;
		}
	}
	if ((0 != (fncode & PAREN)) && (0 != (fncode & FNERROR)))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_FNARGINC, 2, fmt->str.len, fmt->str.addr);
	else
	{
		sign = 0;
		paren = FALSE;
		if ('-' == *ch)
		{
			sign = '-';
			ch++;
			ct--;
		}
		if (0 != (fncode & PAREN))
		{
			if ('-' == sign)
			{
				*cp++ = '(';
				sign = 0;
				paren = TRUE;
			}
			else *cp++ = ' ';
		}
		/* Only add '+' if > 0 */
		if ((0 != (fncode & PLUS)) && (0 == sign))
		{	/* Need to make into num and check for int 0 in case was preprocessed by op_fnj3() */
			MV_FORCE_NUM(dst);
			if ((0 == (dst->mvtype & MV_INT)) || (0 != dst->m[1]))
				sign = '+';
		}
		if ((0 != (fncode & MINUS)) && ('-' == sign))
			sign = 0;
		if ((0 == (fncode & TRAIL)) && (0 != sign))
			*cp++ = sign;
		if (0 != (fncode & COMMA))
		{
			comma = FALSE;
			for (x = 0, t = ch; (('.' != *t) && (++x < ct)); t++)
				;
			z = x;
			if ((y = x % 3) > 0)
			{
				while (y-- > 0)
					*cp++ = *ch++;
				comma = TRUE;
			}
			for ( ; (0 != (x / 3)); x -= 3, cp += 3, ch +=3)
			{
				if (comma)
					*cp++ = ',';
				else
					comma = TRUE;
				memcpy(cp, ch, 3);
			}
			if (z < ct)
			{
				xx = ct - z;
				memcpy(cp, ch, xx);
				cp += xx;
			}
		} else
		{
			memcpy(cp, ch, ct);
			cp += ct;
		}
		if (0 != (fncode & TRAIL))
		{
			if (sign != 0) *cp++ = sign;
			else *cp++ = ' ';
		}
		if (0 != (fncode & PAREN))
		{
			if (paren)*cp++ = ')';
			else *cp++ = ' ';
		}
		dst->mvtype = MV_STR;
		dst->str.addr = (char *)stringpool.free;
		dst->str.len = INTCAST(cp - stringpool.free);
		stringpool.free = cp;
		return;
	}
	assertpro(FALSE);
}
