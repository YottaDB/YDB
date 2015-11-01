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

#define PLUS 	1
#define MINUS 	2
#define TRAIL	4
#define COMMA	8
#define PAREN 	16
#define FNERROR 7

GBLREF spdesc stringpool;

void op_fnfnumber(mval *src,mval *fmt,mval *dst)
{
	mval temp;
	unsigned char fncode,*ch,*cp, *ff, *ff_top;
	short int t;
	unsigned char sign;
	short int ct,paren;
	error_def(ERR_FNARGINC);
	error_def(ERR_FNUMARG);

	assert (stringpool.free >= stringpool.base);
	assert (stringpool.free <= stringpool.top);
	/* assure that there is adequate space for two string forms of a number
	   as a local version of the src must be operated upon in order to get
	   a canonical number
	*/
	if ((stringpool.top - stringpool.free) < MAX_NUM_SIZE * 2)
		stp_gcol(MAX_NUM_SIZE * 2);
	/* operate on the src operand in a temp, so that
	   conversions are possible without destroying the source
	*/
	temp = *src;
	/* if the source operand is not a cannonical number, force conversion
	*/
	MV_FORCE_STR(&temp);
	MV_FORCE_STR(fmt);
	if (fmt->str.len == 0)
	{	*dst = temp;
		return;
	}
	temp.mvtype = MV_STR;
	ch = (unsigned char *)temp.str.addr;
	ct = temp.str.len;
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
	if ((fncode & PAREN) != 0 && (fncode & FNERROR) != 0)
	{	rts_error(VARLSTCNT(4) ERR_FNARGINC, 2, fmt->str.len, fmt->str.addr);
	}
	else
	{
		sign = 0;
		paren = FALSE;
		if (*ch == '-')
		{
			sign = '-';
			ch++;
			ct--;
		}
		if ((fncode & PAREN) != 0)
		{
			if (sign == '-')
			{
				*cp++ = '(';
				sign = 0;
				paren = TRUE;
			}
			else *cp++ = ' ';
		}
		if ((fncode & PLUS) != 0 && sign == 0)
			sign = '+';
		if ((fncode & MINUS) != 0 && sign == '-')
			sign = 0;
		if ((fncode & TRAIL) == 0 && sign != 0)
			*cp++ = sign;
		if ((fncode & COMMA) != 0)
		{
			unsigned char *t;
			short int x,y,z,xx;
			short int comma = FALSE;

			for (x = 0, t = ch; *t != '.' && ++x < ct; t++) ;
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
				memcpy (cp,ch,3);
			}
			if (z < ct)
			{
				xx = ct - z;
				memcpy (cp,ch,xx);
				cp += xx;
			}
		}
		else
		{
			memcpy (cp,ch,ct);
			cp += ct;
		}
		if ((fncode & TRAIL) != 0)
		{
			if (sign != 0) *cp++ = sign;
			else *cp++ = ' ';
		}
		if ((fncode & PAREN) != 0)
		{
			if (paren) *cp++ = ')';
			else *cp++ = ' ';
		}
		dst->mvtype = MV_STR;
		dst->str.addr = (char *)stringpool.free;
		dst->str.len = cp - stringpool.free;
		stringpool.free = cp;
		return;
	}
	GTMASSERT;
}
