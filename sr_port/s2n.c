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

#include "arit.h"
#include "stringpool.h"

#define DIGIT(x)	((x >='0') && (x <= '9'))
#define NUM_MASK	(MV_NM | MV_INT | MV_NUM_APPROX)

error_def(ERR_NUMOFLOW);

LITREF mval literal_null;
LITREF int4 ten_pwr[];

char *s2n (mval *u)
{
	boolean_t	digit, dot, dotseen, exp, exneg, isdot, tail;
	char		*c, *d, *eos, *w;
	int		expdigits, i, j, k, sign, x, y, z, zero;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	i = 0;
	if (!MV_DEFINED(u))
		GTMASSERT;
	c = u->str.addr;
	if (0 == u->str.len)
	{	/* Substitute pre-converted NULL/0 value */
		TREF(s2n_intlit) = 1;
		*u = literal_null;
		return c;
	}
	eos = u->str.addr + u->str.len;				/* End of string marker */
	sign = 0;
	while (c < eos && (('-'== *c) || ('+' == *c)))
		sign += (('-' == *c++) ? 1 : 2);		/* Sign is odd: negative, even: positive */
	for (zero = 0; (c < eos) && ('0'== *c ); zero++, c++)	/* Eliminate leading zeroes */
		 ;
	dot = ((c < eos) && ('.' == *c));
	if (dot)
		c++;
	for (y = 0; (c < eos) && ('0' == *c ); c++, y--)	/* Eliminate leading zeroes of possible fractional part */
		;
	z = u->m[0] = u->m[1] = 0;				/* R0 */
	d = c + 9;
	for  (w = ((d < eos) ? d : eos); (c < w) && DIGIT(*c); c++, z++)
	{
		if ('0' == *c)
		{
			u->m[1] *= 10;
			i++;
		} else
		{
			i = 0;
			u->m[1] = (u->m[1] * 10) + (*c - '0');
		}
	}							/* R1 */
	if ((c < w) && ('.' == *c) && !dot)
	{
		y = z;
		c++;
		d++;
		dot = TRUE;
		if (w < eos)
			w++;
	}
	for (; (c < w) && DIGIT(*c); c++)
	{
		if ('0' == *c)
		{
			u->m[1] *= 10;
			i++;
		} else
		{
			i = 0;
			u->m[1] = (u->m[1] * 10) + (*c - '0');
		}
	}							/* R2 */
	k = (int4)(d - c);
	if (c < eos)
	{
		d = c + 9;
		for (w = ((d < eos) ? d : eos); (c < w) && DIGIT(*c); z++)
			u->m[0] = (u->m[0] * 10) + (*c++ - '0');
		if ((c < w) && ('.' == *c) && !dot )
		{
			y = z;
			c++;
			d++;
			dot = TRUE;
			if (w < eos)
				w++;
		}
		while ((c < w) && DIGIT(*c))
			u->m[0] = (u->m[0] * 10) + (*c++ - '0');
		u->m[0] *= ten_pwr[d - c];
		for (; (c < eos) && ('0' == *c); c++, z++)
			;
	}
	tail = (c != eos) || (dot && (('0' == *(c - 1)) || ('.' == *(c - 1))));
	for (dotseen = dot; (c < eos) && (((isdot = ('.' == *c)) && !dot) || DIGIT(*c)); c++)
	{
		dotseen = (dotseen || isdot);
		if (!dotseen)
			z++;
	}
	digit = (0 != z) || (0 != y) || (0 != zero);
	x = 0;
	exp = ('E' == *c) && digit;
	if (exp && ((c + 1) < eos))
	{
		c++;
		exneg = ('-' == *c);
		if (exneg || ('+' == *c))
			c++;
		for (; (c < eos) && ('0' == *c); c++)
			;	/* Do not count leading 0s towards MAX_DIGITS_IN_EXP */
		for (expdigits = 0; (c < eos) && DIGIT(*c); c++)
		{
			if ((MAX_DIGITS_IN_EXP + 1) > expdigits)
			{
				x = (x * 10) + (*c - '0');
				expdigits++;
			}
		}
		if (exneg)
			x = -x;
	}
	TREF(s2n_intlit) = (0 != sign) || dot || exp;
	if (digit)
	{
		x += (dot ? y : z);
		j = x + k - 6;
		i += j;
		if ((0 == u->m[0]) && (6 >= x) && (0 <= i))
		{
			u->mvtype |= (tail || (1 < sign) || ((0 != zero) && (1 != u->str.len)))
				? (MV_NM | MV_INT | MV_NUM_APPROX) : (MV_NM | MV_INT);
			if (0 > j)
				u->m[1] /= ((sign & 1) ? -ten_pwr[-j] : ten_pwr[-j]);
			else
				u->m[1] *= ((sign & 1) ? -ten_pwr[j] : ten_pwr[j]);
		} else
		{
			u->m[1] *= ten_pwr[k];
			x += MV_XBIAS;
			if ((EXPLO > x) || (0 == u->m[1]))
			{
				u->mvtype |= MV_NM | MV_INT | MV_NUM_APPROX;
				u->m[1] = 0;
			} else if (EXPHI <= x)
			{
				u->mvtype &= ~NUM_MASK;
				if (!TREF(compile_time))
					rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
			} else
			{
				u->e = x;
				u->sgn = sign & 1;
				u->mvtype |= (tail || (1 < sign) || ((0 != zero) && (1 != u->str.len)))
					? (MV_NM | MV_NUM_APPROX) : MV_NM;
			}
		}
		assert(MANT_HI > u->m[1]);
	} else
	{
		u->mvtype |= (MV_NM | MV_INT | MV_NUM_APPROX);
		u->m[1] = 0;
	}
	return c;
}
