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

#include "arit.h"
#include "stringpool.h"
#include "op.h"

GBLREF spdesc stringpool;
LITREF int4 ten_pwr[];


void op_fnj3(mval *src,int width,int fract,mval *dst)
{
	int4 n, n1, m;
	int w, digs, digs_used;
	int sign;
	static readonly int4 fives_table[9] =
	{ 500000000, 50000000, 5000000, 500000, 50000, 5000, 500, 50, 5};
	unsigned char *cp;
	error_def(ERR_JUSTFRACT);
	error_def(ERR_MAXSTRLEN);

	if (width < 0)
		width = 0;
	else	if (width > MAX_STRLEN)
			rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
	if (fract < 0)
		rts_error(VARLSTCNT(1) ERR_JUSTFRACT);
	w = width + MAX_NUM_SIZE + 2 + fract;
	/* the literal two above accounts for the possibility
	of inserting a zero and/or a minus with a width of zero */
	if  (w > MAX_STRLEN)
		rts_error(VARLSTCNT(1) ERR_MAXSTRLEN);
	MV_FORCE_NUM(src);
	/* need to guarantee that the n2s call will not cause string pool overflow */
	ENSURE_STP_FREE_SPACE(w);
	sign = 0;
	cp = stringpool.free;
	if (src->mvtype & MV_INT)
	{
		n = src->m[1];
                if (n < 0)
                {
			sign = 1;
			n = -n;
		}
		/* Round if necessary */
		if (fract < 3)
			n += fives_table[fract + 6];
		/* Compute digs, the number of non-zero leading digits */
		if (n < 1000)
		{
			digs = 0;
			/* if we have something like $j(-.01,0,1), the answer should be 0.0, not -0.0
			   so lets check for that here */
			if (sign && fract < 4 && n / ten_pwr[3 - fract] == 0)
			{
				sign = 0;
				n = 0;
			} else
				n *= 1000000;
		} else if (n >= 1000000000)
		{
			digs = 7;
		} else
		{
			for (digs = 6; n < 100000000 ; n *= 10 , digs--)
				;
		}
		/* Do we need leading spaces? */
		w = width - sign - (fract != 0) - fract - digs;
		if (digs == 0)
			w--;
		if (w > 0)
		{
			memset(cp, ' ', w);
			cp += w;
		}
		if (sign)
			*cp++ = '-';
		if (digs == 0)
			*cp++ = '0';
		else
		{
			/* It is possible that when rounding, that
			   we overflowed by one digit.  In this case,
			   the left-most digit must be a "1".
			   Take care of this case first.
			*/
			if (digs == 7)
			{
				*cp++ = '1';
				n -= 1000000000;
				digs = 6;
			}
			for ( ; digs > 0 ; digs--)
			{
				n1 = n / 100000000;
				*cp++ = n1 + '0';
				n = (n - n1 * 100000000) * 10;
			}
		}
		if (fract)
		{
			*cp++ = '.';
			for (digs = fract ; digs > 0 && n != 0; digs--)
			{
				n1 = n / 100000000;
				*cp++ = n1 + '0';
				n = (n - n1 * 100000000) * 10;
			}
			if (digs)
			{
				memset(cp, '0', digs);
				cp += digs;
			}
		}
	} else
	{
		digs = src->e - MV_XBIAS;
		m = src->m[0];
		n = src->m[1];
		sign = src->sgn;
		w = digs + fract;
		if (w < 18 && w >= 0)
		{
			if (w < 9)
			{
				n += fives_table[w];
				if (n >= MANT_HI)
				{
					n1 = n / 10;
					m = m / 10 + ((n - n1 * 10) * MANT_LO);
					n = n1;
					digs++;
				}
			}
			else
			{
				m += fives_table[w - 9];
				if (m >= MANT_HI)
				{
					m -= MANT_HI;
					n++;
					if (n >= MANT_HI)
					{
						n1 = n / 10;
						m = m / 10 + ((n - n1 * 10) * MANT_LO);
						n = n1;
						digs++;
					}
				}
			}
		}
		/* if we have something like $j(-.0001,0,1), the answer should be 0.0, not -0.0 */
		if (digs <= - fract)
		{
			sign = 0;
			n = m = 0;
		}
		w = width - fract - (fract != 0) - sign - (digs < 1 ? 1 : digs);
		if (w > 0)
		{
			memset(cp, ' ', w);
			cp += w;
		}
		if (sign)
			*cp++ = '-';
		digs_used = 0;
		if (digs < 1)
			*cp++ = '0';
		else
		{
			for ( ; digs > 0 && (n != 0 || m != 0); digs--)
			{
				n1 = n / 100000000;
				*cp++ = n1 + '0';
				digs_used++;
				if (digs_used == 9)
				{
					n = m;
					m = 0;
				} else
					n = (n - n1 * 100000000) * 10;
			}
			if (digs > 0)
			{
				memset(cp, '0', digs);
				cp += digs;
			}
		}
		if (fract)
		{
			*cp++ = '.';
			if (digs < 0)
			{
				digs = - digs;
				if (digs > fract)
					digs = fract;
				memset(cp, '0', digs);
				cp += digs;
				fract -= digs;
			}
			for (digs = fract ; digs > 0 && (n != 0 || m != 0); digs--)
			{
				n1 = n / 100000000;
				*cp++ = n1 + '0';
				digs_used++;
				if (digs_used == 9)
				{
					n = m;
					m = 0;
				} else
					n = (n - n1 * 100000000) * 10;
			}
			if (digs)
			{
				memset(cp, '0', digs);
				cp += digs;
			}
		}
	}
	dst->mvtype = MV_STR;
	dst->str.addr = (char *)stringpool.free;
	dst->str.len = INTCAST((char *)cp - dst->str.addr);
	stringpool.free = cp;
	return;
}
