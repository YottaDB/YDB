/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "stack_frame.h"
#include "mv_stent.h"

#define PLUS 		0x01
#define MINUS 		0x02
#define TRAIL		0x04
#define COMMA		0x08
#define DOT		0x10
#define PAREN 		0x20

#define NOTWITHPAREN	(PLUS | MINUS | TRAIL)
#define NOTWITHDOT	COMMA

GBLREF spdesc	stringpool;
GBLREF mv_stent	*mv_chain;

LITREF mval	literal_zero;

error_def(ERR_FNARGINC);
error_def(ERR_FNUMARG);

void op_fnfnumber(mval *src, mval *fmt, boolean_t use_fract, int fract, mval *dst)
{
	boolean_t	needsep, paren;
	int 		ct, x, xx, y, intlen;
	unsigned char	*ch, *cp, *ff, *ff_top, fncode, sign, *t, sepchar, decptchar;

	MV_FORCE_DEFINED(src);
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
			case  '.':
				fncode |= DOT;
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
	/* Error checks */
	if ((0 != (fncode & PAREN)) && (0 != (fncode & NOTWITHPAREN))		/* Test for invalid options given with PAREN */
	    || ((0 != (fncode & DOT)) && (0 != (fncode & NOTWITHDOT))))		/* Test for invalid option(s) given with DOT */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_FNARGINC, 2, fmt->str.len, fmt->str.addr);
	/* Formatting - the number is already formatted as a standard numeric string by the MV_FORCE_STR() macros above so
	 * we just need to rearrange it according to the formatting options.
	 */
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
		} else
			*cp++ = ' ';
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
	if ((0 != (fncode & COMMA)) || (0 != (fncode & DOT)))
	{
		/* Decide which characters we are using for the decimal point and the separator chars */
		decptchar = '.';
		sepchar = ',';
		if (0 != (fncode & DOT))
		{	/* Reverse the separator and decimal point characters */
			decptchar = ',';
			sepchar = '.';
		}
		/* Find end of decimal chars to left of decimal point */
		for (intlen = 0, t = ch; (('.' != *t) && (++intlen < ct)); t++)
			;
		x = intlen;			/* x is working copy of the length of integer part of number */
		needsep = FALSE;
		if (0 < (y = x % 3))		/* Note assignment */
		{
			while (0 < y--)
				*cp++ = *ch++;
			needsep = TRUE;
		}
		for ( ; (0 != (x / 3)); x -= 3, cp += 3, ch +=3)
		{
			if (needsep)
				*cp++ = sepchar;
			else
				needsep = TRUE;
			memcpy(cp, ch, 3);
		}
		/* Done with the integer part of the value - now finish up with the decimal point (only if a
		 * fractional part exists) and the fractional part - again if it exists.
		 */
		if (intlen < ct)
		{	/* Add chars contain decimal point char plus any digits to the right of it */
			assert('.' == *ch);
			ch++;			/* Get past the '.' as we are changing it to our separator */
			*cp++ = decptchar;
			xx = ct - intlen - 1;
			memcpy(cp, ch, xx);
			cp += xx;
		}
	} else
	{
		memcpy(cp, ch, ct);
		cp += ct;
	}
	if (0 != (fncode & TRAIL))
		*cp++ = (sign != 0) ? sign : ' ';
	if (0 != (fncode & PAREN))
		*cp++ = paren ? ')' : ' ';
	dst->mvtype = MV_STR;
	dst->str.addr = (char *)stringpool.free;
	dst->str.len = INTCAST(cp - stringpool.free);
	stringpool.free = cp;
	return;
}
