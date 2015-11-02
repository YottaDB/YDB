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
#include "gtm_string.h"
#include "stringpool.h"
#include "op.h"

#define PLUS 	1
#define MINUS 	2
#define TRAIL	4
#define COMMA	8
#define PAREN 	16
#define FNERROR 7

GBLREF spdesc stringpool;

void op_fnfnumber(mval *src, mval *fmt, mval *dst)
{
	mval		temp, *temp_p;
	unsigned char	fncode, sign, *ch, *cp, *ff, *ff_top, *t;
	int 		ct, x, y, z, xx;
	boolean_t	comma, paren;
	error_def(ERR_FNARGINC);
	error_def(ERR_FNUMARG);

	assert (stringpool.free >= stringpool.base);
	assert (stringpool.free <= stringpool.top);
	/* assure that there is adequate space for two string forms of a number
	   as a local version of the src must be operated upon in order to get
	   a canonical number
	*/
	ENSURE_STP_FREE_SPACE(MAX_NUM_SIZE * 2);
	/* operate on the src operand in a temp, so that
	   conversions are possible without destroying the source
	*/
	temp_p = &temp;
	*temp_p = *src;
	/* if the source operand is not a canonical number, force conversion
	*/
	MV_FORCE_STR(temp_p);
	MV_FORCE_STR(fmt);
	if (fmt->str.len == 0)
	{
		*dst = *temp_p;
		return;
	}
	temp_p->mvtype = MV_STR;
	ch = (unsigned char *)temp_p->str.addr;
	ct = temp_p->str.len;
	cp = stringpool.free;
	fncode = 0;
	for (ff = (unsigned char *)fmt->str.addr , ff_top = ff + fmt->str.len ; ff < ff_top ; )
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
				rts_error(VARLSTCNT(6) ERR_FNUMARG, 4, fmt->str.len, fmt->str.addr, 1, --ff);
			break;
		}
	}
	if (0 != (fncode & PAREN) && 0 != (fncode & FNERROR))
		rts_error(VARLSTCNT(4) ERR_FNARGINC, 2, fmt->str.len, fmt->str.addr);
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
		if (0 != (fncode & PLUS) && 0 == sign)
		{	/* Need to make into num and check for int 0 in case was preprocessed by op_fnj3() */
			MV_FORCE_NUM(temp_p);
			if (0 == (temp_p->mvtype & MV_INT) || 0 != temp_p->m[1])
				sign = '+';
		}
		if (0 != (fncode & MINUS) && '-' == sign)
			sign = 0;
		if (0 == (fncode & TRAIL) && 0 != sign)
			*cp++ = sign;
		if (0 != (fncode & COMMA))
		{
			comma = FALSE;
			for (x = 0, t = ch; '.' != *t && ++x < ct; t++) ;
			z = x;
			if ((y = x % 3) > 0)
			{
				while (y-- > 0)
					*cp++ = *ch++;
				comma = TRUE;
			}
			for ( ; x / 3 != 0 ; x -= 3, cp += 3, ch +=3)
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
			if (paren) *cp++ = ')';
			else *cp++ = ' ';
		}
		dst->mvtype = MV_STR;
		dst->str.addr = (char *)stringpool.free;
		dst->str.len = INTCAST(cp - stringpool.free);
		stringpool.free = cp;
		return;
	}
	GTMASSERT;
}
