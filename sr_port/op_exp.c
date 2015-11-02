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
#include <math.h>

#include "arit.h"
#include "stringpool.h"
#include "op.h"
#include "mvalconv.h"

#define ACCUR_PERCENT	0.00000000000000055
#define MAX_M_INT 999999999

LITREF int4 ten_pwr[];
LITREF mval literal_one;
LITREF mval literal_zero;

STATICFNDEF void op_exp_flgchk(mval *mv);

error_def(ERR_NEGFRACPWR);
error_def(ERR_NUMOFLOW);

void op_exp(mval *u, mval* v, mval *p)
{
	mval 		u1, *u1_p;
	double 		accuracy, exponent;
	double 		x, x1, y, z, z2, z3, z4, z5, id, il;
	int		im0, im1, ie, i, j, j1;
	boolean_t	fraction = FALSE, in = FALSE;
	boolean_t	neg = FALSE, even = TRUE;
	mval    	w, zmv;
	int4    	n, n1;
	int4		z1_rnd, z2_rnd, pten;

	u1_p = &u1;
	memcpy(u1_p, u, SIZEOF(mval));
	MV_FORCE_NUM(u1_p);
	MV_FORCE_NUM(v);
	if ((0 == v->m[1]) && (0 == v->m[0]))
	{	/* 0**0 = 1 */
		*p = literal_one;
		return;
	}
	if (0 != (v->mvtype & MV_INT))
	{	/* Integer-ish exponent (could have up to 3 digits to right of decimal pt) */
		n = v->m[1];
		if (0 == n)
		{	/* anything**0 = 1 where anything != 0 */
			*p = literal_one;
			return;
		}
		if (0 != (u1_p->mvtype & MV_INT))
		{	/* Integer-ish base */
			if (0 == u1_p->m[1])
			{	/* 0**anything = 0 */
				*p = literal_zero;
		                return;
			}
		} else if ((0 == u1_p->m[1]) && (0 == u1_p->m[0]))
		{	/* 0**anything = 0 */
			*p = literal_zero;
			return;
		}
		n1 = n / MV_BIAS;
		if ((n1 * MV_BIAS) == n)
		{	/* True non-fractional exponent */
			if (0 == v->m[1])
			{	/* Duplicate of check on line 58? */
				*p = literal_one;
				return;
			}
			if (0 > n1)
			{	/* Create inverse due to negative exponent */
				op_div((mval *)&literal_one, u1_p, &w);
				n1 = -n1;
			} else
				w = *u1_p;
			zmv = literal_one;
			for ( ; ; )
			{	/* Compute integer exponent */
				if (n1 & 1)
					op_mul(&zmv, &w, &zmv);
				n1 /= 2;
				if (!n1)
					break;
				op_mul(&w, &w, &w);
			}
			*p = zmv;
			return;
		} else
		{	/* Have non-integer exponent (has fractional component) */
			if (0 != (u1_p->mvtype & MV_INT))
			{	/* Base is integer-ish */
                        	if (0 > u1_p->m[1])
				{	/* Base is negative, invalid exponent expression */
					rts_error(VARLSTCNT(1) ERR_NEGFRACPWR);
					return;
				}
			} else
			{	/* Base is NOT integer-sh */
				if (u1_p->sgn)
				{	/* Base is negative, invalid exponent expression */
					rts_error(VARLSTCNT(1) ERR_NEGFRACPWR);
					return;
				}
			}
                }
        } else
	{	/* Exponent NOT integer-ish */
	        if (0 != (u1_p->mvtype & MV_INT))
        	{	/* Base is integer-ish */
                	if (0 > u1_p->m[1])
                	{	/* Base is negative - make positive but record was negative */
                        	u1_p->m[1] = -u1_p->m[1];
	                        neg = TRUE;
        	        }
			if (0 == u1_p->m[1])
			{	/* 0**anything is 0 */
				*p = literal_zero;
				return;
			}
        	} else if (u1_p->sgn)
        	{	/* Base is NOT integer-ish and is negative - clear sign and record with flag */
                	u1_p->sgn = 0;
	                neg = TRUE;
			if ((0 == u1_p->m[1]) && (0 == u1_p->m[0]))
			{	/* 0**anything is zero */
				*p = literal_zero;
				return;
			}
        	}
		if (NUM_DEC_DG_2L > (ie = (v->e - MV_XBIAS)))		/* Note assignment */
		{	/* Need to determine 2 things:
			 *   1. Whether this exponent has a fractional part (vs just being large)
			 *   2. If the base value is negative, result is negative based on whether exponent is even/odd
			 * Since all exponent digits are visible, determine these items.
			 */
			if (0 < ie)
			{	/* Decimal point shifting to right - see if fully integer*/
				if (NUM_DEC_DG_1L > ie)
				{	/* Exponent fits in int4 */
					for (i = 1, j = 10; (NUM_DEC_DG_1L - ie) > i; j *= 10, i++)
						;			/* Determine exponential scale */
					im1 = v->m[1];
					if (0 != (i = im1 % j))		/* Note assignment */
						fraction = TRUE;
					else
						if (0 != (i = im1 % (2 * j)))
							even = FALSE;
				} else
				{
					im0 = v->m[0];
					if (NUM_DEC_DG_1L == ie)
					{	/* Var ie is at max for no loss in single int */
						if (0 == im0)
						{
							im1 = v->m[1];
							if (0 != (i = im1 % 2))		/* Note assignment */
								even = FALSE;
						} else
							fraction = TRUE;
					} else
					{
						for (i=1, j=10; (NUM_DEC_DG_2L - ie) > i; j *= 10, i++)
							;
						if (0 == (i = im0 % j))			/* Note assignment */
						{
							if (0 != (i = im0 % (2 * j)))	/* Note assignment */
								even = FALSE;
						} else
							fraction = TRUE;
					}
				}
			} else	/* Var ie is negative (fraction) or 0. This is a non-integerish number so ie
				 * must be a fraction. ie must be > 0 to be non-integer.
				 */
				fraction = TRUE;
		} else
		{
			if (NUM_DEC_DG_2L == ie)
			{	/* Var ie is max to have no significant digit loss */
				im0 = v->m[0];
				if (0 != (i = im0 % 2))			/* Note assignment */
					even = FALSE;
			}
		}
		if (fraction && neg)
		{	/* Fractional exponent and negative base not valid */
			rts_error(VARLSTCNT(1) ERR_NEGFRACPWR);
			return;
		}
	}
	x = mval2double(u1_p);		/* Convert base and exponent to double for feeding to pow*() function */
	y = mval2double(v);
	z = pow(x, y);
#	ifdef UNIX
	if (HUGE_VAL == z)		/* Infinity return value check */
	{
		rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
	        return;
	}
#	endif
	assert(!neg);	/* Should be taken care of in one of the op_mul() using sections dealing with whole exponents */
	p->sgn = 0;	/* Positive numbers only from here on out */
	if (0 == z)
	{
		*p = literal_zero;
		return;
	}
	/* Remaining code's main purpose is to convert the double float value to our internal format taking care that
	 * actual integer values (e.g. 4**.5 is 2) are correctly rendered. This can be tricky not only because floating
	 * point values are often imprecise (e.g. 3.0 represented as 2.999999999999999) but because the double-float
	 * type has fewer digits of accuracy than GTM carries.
	 */
	accuracy = z * ACCUR_PERCENT;
	/* Scale accuracy up in case it turned to zero above */
	for (z2 = ACCUR_PERCENT; (0.0 == accuracy); z2 *= 10, accuracy = z * z2)
		;
	if ((fabs(z) <= accuracy) || (0 >= z))
	{	/* Zero equivalency test - some small chance pow() could return a very small negative number. Any
		 * possible negative numbers must have whole number exponent which is handled earlier via op_mul().
		 */
		*p = literal_zero;
		return;
	}
	/* Integer check - GT.M pseudo int MV_INT check - must be decimal form 999999.999 */
	z2 = floor((z + .0005) * MV_BIAS);
	if (MANT_HI > z2)
	{	/* Have proper range, check if accuracy warrants conversion */
		n1 = (int)z2;
		z2 = (double)n1 / MV_BIAS;
		if (fabs(z - z2) < accuracy)
		{	/* We can treat this as a GT.M int */
			((mval_b *)p)->sgne = 0;
			p->mvtype = (MV_NM | MV_INT);
			p->m[0] = 0;
			p->m[1] = (p->sgn) ? -n1 : n1;
			return;
		}
	}
	/* Store as a type-2 non-'integer', i.e. put z in the form z1_rnd * 10^n + z2_rnd * 10^(n-9).
	 * Could add checks for zero/infinity here to avoid lengthy (300ish iterations) while loops below.
	 */
	n = 0;
        while (1e16 <= z)
	{
                n += 5;
		z *= 1e-5;
	}
        while (1e1 > z)
	{
                n -= 5;
		z *= 1e5;
	}
	while (1e9 <= z)
	{
		n++;
		z *= .1;
	}
	while (1e8 > z)
	{
		n--;
		z *= 10.0;
	}
	z1_rnd = (int)z; /* casts should keep z1_rnd and z2_rnd in proper range, hence the assert below */
	z -= (double)z1_rnd;
	z *= 1e9;
	z2_rnd = (int)z;
	assert(((MANT_LO <= z1_rnd) && (MANT_HI > z1_rnd)) && ((0 <= z2_rnd) && (MANT_HI > z2_rnd)));
	if ((0 == z2_rnd) && (-11 <= n) && (-3 >= n) && (0 == (z1_rnd % ten_pwr[-3 - n])))
	{	/* This is a second-chance at detection an integer in case something slipped through the earlier
		 * check. Not expecting it to ever be invoked but is here as a safety net.
		 */
		z1_rnd /= ten_pwr[-3-n];
		((mval_b *)p)->sgne = 0;
		p->mvtype = (MV_NM | MV_INT);
		p->m[1] = z1_rnd;
		return;
	}
	exponent = MV_XBIAS + n + 9;
	if (exponent >= EXPHI)
	{
		rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
		return;
	}
	if (exponent < EXPLO)
	{
		*p = literal_zero;
		return;
	}
	p->mvtype = MV_NM;
	p->e = exponent;
	p->m[1] = z1_rnd;
	p->m[0] = z2_rnd;
	return;
}
