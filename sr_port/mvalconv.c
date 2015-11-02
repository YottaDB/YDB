/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "mvalconv.h"
#include "gtm_stdio.h"	/* this is here due to the need for an SPRINTF,
			 * which is in turn due the kudge that is the current double2mval routine
			 */

LITREF int4 ten_pwr[];

void i2smval(mval *v, uint4 i)
{
	char	*c;
	int	exp;
	int4	n;

	v->mvtype = MV_NM | MV_STR;
	v->m[1] = n = i;
	v->sgn = 0;
	c = v->str.addr;
	exp = 100000000;
	while (exp && !(n = i / exp))
		exp /= 10;
	if (!exp)
		*c++ = 0;
	else for (;;)
	{
		*c++ = n + '0';
		i -= n * exp;
		exp /= 10;
		if (!exp)
			break;
		n = i / exp;
	}
	if (v->m[1] < INT_HI)
	{
		v->mvtype |= MV_INT;
		v->m[1] = MV_BIAS * i;
	} else if (v->m[1] < MANT_HI)
	{
		v->m[1] *= ten_pwr[NUM_DEC_DG_1L - exp];
		v->m[0] = 0;
	} else
	{
		v->m[0] = (v->m[1] % 10) * MANT_LO;
		v->m[1] /= 10;
	}
	v->str.len = INTCAST(c - v->str.addr);
	v->e = v->str.len + MV_XBIAS;
	assert(v->m[1] < MANT_HI);
}

void	i2usmval(mval *v, unsigned int i)
{
	int	exp;

	v->mvtype = MV_NM;
	v->sgn = 0;
	if (i < INT_HI)
	{
		v->mvtype |= MV_INT;
		v->m[1] = MV_BIAS * i;
	} else
	{
		if (i < MANT_HI)
		{
			for (exp = EXP_IDX_BIAL; i < MANT_LO; exp--)
				i *= 10;
			v->e = exp;
			v->m[0] = 0;
			v->m[1] = i;
		} else
		{
			v->m[0] = (i % 10) * MANT_LO;
			v->m[1] = i / 10;
			v->e = EXP_IDX_BIAL + 1;
		}
		assert(v->m[1] < MANT_HI);
		assert(v->m[1] >= MANT_LO);
	}
}

void	i2mval(mval *v, int i)
{
	int	exp;
	int4	n;

	v->mvtype = MV_NM;
	if (i < 0)
	{
		v->sgn = 1;
		n = -i;
	} else
	{
		n = i;
		v->sgn = 0;
	}
	if (n < INT_HI)
	{
		v->mvtype |= MV_INT;
		v->m[1] = MV_BIAS * i;
	} else
	{
		if (n < MANT_HI)
		{
			for (exp = EXP_IDX_BIAL; n < MANT_LO; exp--)
				n *= 10;
			v->e = exp;
			v->m[0] = 0;
			v->m[1] = n;
		} else
		{
			v->m[0] = (n % 10) * MANT_LO;
			v->m[1] = n / 10;
			v->e = EXP_IDX_BIAL + 1;
		}
		assert(v->m[1] < MANT_HI);
		assert(v->m[1] >= MANT_LO);
	}
}

double mval2double(mval *v)
{
	double	x, y;
	int	exp;

	MV_FORCE_NUM(v);
	x = v->m[1];
	if (v->mvtype & MV_INT)
		x /= MV_BIAS;
	else
	{
		exp = v->e;
		y = v->m[0];
		y = y/MANT_HI;
		while (exp > EXP_IDX_BIAL)
		{
			x *= MANT_HI;
			y *= MANT_HI;
			exp -= 9;
		}
		while (exp < MV_XBIAS)
		{
			x /= MANT_HI;
			y /= MANT_HI;
			exp += 9;
		}
		x /= ten_pwr[EXP_IDX_BIAL - exp];
		y /= ten_pwr[EXP_IDX_BIAL - exp];
		x += y;
		x = (v->sgn ? -x : x);
	}
	return x;
}

/* a (barely suitable) double2mval */
void     double2mval(mval *dst, double src)
{
        char    buf[67];    /* [possible] sign, decimal-point, [up to] 64 digits, and terminator */
	SPRINTF(buf, "%lf", src);
	dst->mvtype = MV_STR;
	dst->str.len = STRLEN(buf);
	dst->str.addr = buf;
	s2n(dst);
	dst->mvtype &= ~MV_STR;
	return;
}


/* converts an mval into a 32-bit signed integer, or MAXPOSINT4 on overflow */
int4 mval2i(mval *v)
{
	int4	i;
	double	j;
	int	exp;

	MV_FORCE_NUM(v);
	if (v->mvtype & MV_INT)
		i = v->m[1]/MV_BIAS;
	else
	{
		exp = v->e;
		if (exp > EXP_IDX_BIAL)
		{
			j = mval2double(v);
			i = (MAXPOSINT4 >= j) ? (int4)j : MAXPOSINT4;
		} else if (exp < MV_XBIAS)
			i = 0;
		else
			i = (v->sgn ? -v->m[1] : v->m[1]) / ten_pwr[EXP_IDX_BIAL - exp];
	}
	return i;
}

/* converts an mval into a 32-bit unsigned integer, or MAXUINT4 on overflow */
uint4 mval2ui(mval *v)
{
	uint4	i;
	double	j;
	int	exp;

	MV_FORCE_NUM(v);
	if (v->mvtype & MV_INT)
		i = v->m[1]/MV_BIAS;
	else
	{
		exp = v->e;
		if (exp > EXP_IDX_BIAL)
		{
			j = mval2double(v);
			i = (MAXUINT4 >= j) ? (uint4)j : MAXUINT4;
		} else if (exp < MV_XBIAS)
			i = 0;
		else
			i = (v->sgn ? -v->m[1] : v->m[1]) / ten_pwr[EXP_IDX_BIAL - exp];
	}
	return i;
}

/* isint == v can be represented as a 9 digit (or less) integer (positive or negative).
 * If return value is TRUE, then "*intval" contains the integer value stored in "v".
 * Note: "*intval" could have been updated even if return value is FALSE.
 */
boolean_t isint(mval *v, int4 *intval)
{
	int			exp, m1, mvtype, divisor, m1_div, m1_sgn;
	DEBUG_ONLY(boolean_t	is_canonical;)

	mvtype = v->mvtype;
	/* Note that input mval might have "MV_NM" bit set even though it is not a numeric (i.e. a string).
	 * This is possible in case the input mval is a constant literal string. In this case, since these
	 * might reside in read-only sections of the executable and the MV_FORCE_* macros might not be able
	 * to touch them, we define the numeric portions of the mval to be 0 and set the MV_NM bit as well.
	 * But in addition, the MV_NUM_APPROX bit will be set to indicate this is an approximation. So if we
	 * see the MV_NM bit set, we should also check the MV_NUM_APPROX bit is unset before we go ahead
	 * and check the numeric part of this mval for whether it is an integer.
	 */
	DEBUG_ONLY(is_canonical = MV_IS_CANONICAL(v));
	assert(!is_canonical || (MVTYPE_IS_NUMERIC(mvtype) && !MVTYPE_IS_NUM_APPROX(mvtype)));
	if (!MVTYPE_IS_NUMERIC(mvtype) || MVTYPE_IS_NUM_APPROX(mvtype))
		return FALSE;
	assert(v->m[1] < MANT_HI);
	if (mvtype & MV_INT)
	{
		divisor = MV_BIAS;
		m1 = v->m[1];
	} else
	{
		exp = v->e;
		if ((MV_XBIAS >= exp) || (EXP_IDX_BIAL < exp) || (0 != v->m[0]))
			return FALSE;
		divisor = ten_pwr[EXP_IDX_BIAL - exp];
		if (v->sgn)
			m1 = -v->m[1];
		else
			m1 = v->m[1];
	}
	m1_div = (m1 / divisor);
	assert(NULL != intval);
	*intval = m1_div;
	return ((m1_div * divisor) == m1);
}
