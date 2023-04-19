/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
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
#include "min_max.h"

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
	mval		*t_src_p;

	MV_FORCE_DEFINED(src);
	PUSH_MV_STENT(MVST_MVAL);			/* Create a temporary on M stack so garbage collection can see it */
	t_src_p = &mv_chain->mv_st_cont.mvs_mval;	/* Operate on copy of src so can modify without changing original */
	*t_src_p = *src;
	if (use_fract)
		op_fnj3(t_src_p, 0, fract, t_src_p);
	else
	{
		MV_FORCE_NUM(t_src_p);
		MV_FORCE_CANONICAL(t_src_p);	/* if the source operand is not a canonical number, force conversion */
	}
	assert (stringpool.free >= stringpool.base);
	assert (stringpool.free <= stringpool.top);
	MV_FORCE_STR(fmt);
	MV_FORCE_STR(t_src_p);
	if (0 == fmt->str.len)
	{
		*dst = *t_src_p;
		POP_MV_STENT(); 	/* Done with temporary */
		return;
<<<<<<< HEAD
	}
	/* Reserve space in string pool to hold the destination string plus the commas,periods etc. that could get added.
	 * Since the number of commas,periods etc. that get added is proportional to the length of the string (1/3 of the length)
	 * to be safe, just reserve twice the space in t_src_p->str.len. But if t_src_p->str.len is too small, minor overheads
	 * like adding "+" at beginning and parentheses around the value could become more than twice the length so take
	 * MAX_NUM_SIZE in that case before doing the twice calculation. Hence the MAX usage below.
	 */
	ENSURE_STP_FREE_SPACE(MAX(MAX_NUM_SIZE, t_src_p->str.len) * 2);
	ch = (unsigned char *)t_src_p->str.addr;
	ct = t_src_p->str.len;
=======
	ENSURE_STP_FREE_SPACE((MAX_NUM_SIZE * 2) + fract);
	ch = (unsigned char *)dst->str.addr;
	ct = dst->str.len;
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
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
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_FNUMARG, 4, fmt->str.len, fmt->str.addr, 1, --ff);
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
		MV_FORCE_NUM(t_src_p);
		if ((0 == (t_src_p->mvtype & MV_INT)) || (0 != t_src_p->m[1]))
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
	intlen = INTCAST(cp - stringpool.free);
	/* Before modifying "dst" to point to the result, do MAX_STRLEN check on the result */
	if (MAX_STRLEN < intlen)
		rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
	dst->mvtype = MV_STR;
	dst->str.addr = (char *)stringpool.free;
	dst->str.len = intlen;
	stringpool.free = cp;
	POP_MV_STENT(); 	/* Done with temporary */
	return;
}
