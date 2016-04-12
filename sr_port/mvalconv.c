/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
/* we return strings for >18 digit 64-bit numbers, so pull in stringpool */
#include "stringpool.h"

GBLREF spdesc   stringpool;

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

void xi2mval(mval *v, unsigned int i);

void i2usmval(mval *v, unsigned int i)
{
	v->mvtype = MV_NM;
	v->sgn = 0;

	xi2mval(v, i);
}

void i2mval(mval *v, int i)
{
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

	xi2mval(v, n);
}

/* xi2mval does the bulk of the conversion for i2mval and i2usmval.
 * The primary routines set the sgn flag and pass the absolute value
 * to xi2mval. */

void xi2mval(mval *v, unsigned int i)
{
	int	exp;

	if (i < INT_HI)
	{
		v->mvtype |= MV_INT;
		v->m[1] = MV_BIAS * (v->sgn ? -(int)i : i);
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

void xi82mval(mval *v, gtm_uint64_t i);

void ui82mval(mval *v, gtm_uint64_t i)
{
	v->mvtype = MV_NM;
	v->sgn = 0;

	xi82mval(v, i);
}

void i82mval(mval *v, gtm_int64_t i)
{
	gtm_uint64_t	absi;

	v->mvtype = MV_NM;
	if (i < 0)
	{
		v->sgn = 1;
		absi = -i;
	} else
	{
		v->sgn = 0;
		absi = i;
	}

	xi82mval(v, absi);
}

/* This function does the bulk of the conversion for i82mval and ui82mval. The primary routines set the sgn flag and pass the
 * absolute value to xi82mval(). In the case of a >18 digit number, xi82mval() examines the sgn flag to determine whether to
 * switch back to a signed value before string conversion.
 */
void xi82mval(mval *v, gtm_uint64_t i)
{
	int	exp;
	uint4	low;
	uint4	high;
	char    buf[21];	/* [possible] sign, [up to] 19L/20UL digits, and terminator. */
	int	len;

	if (i < INT_HI)
	{
		v->mvtype |= MV_INT;
		v->m[1] = MV_BIAS * (v->sgn ? -(int4)i : (uint4)i);
	} else
	{
		if (i < MANT_HI)
		{
			low = 0;
			high = i;
			exp = EXP_IDX_BIAL;
			while (high < MANT_LO)
			{
				high *= 10;
				exp--;
			}
			v->e = exp;
			v->m[0] = low;
			v->m[1] = high;
		} else if (i < (gtm_uint64_t)MANT_HI * MANT_HI)
		{
			low = i % MANT_HI;
			high = i / MANT_HI;
			exp = EXP_IDX_BIAL + 9;
			while (high < MANT_LO)
			{
				high = (high * 10) + (low / MANT_LO);
				low = (low % MANT_LO) * 10;
				exp--;
			}
			v->e = exp;
			v->m[0] = low;
			v->m[1] = high;
		} else
		{	/* The value won't fit in 18 digits, so return a string. */
			if (v->sgn)
				len = SPRINTF(buf, "%lld", -(gtm_int64_t)i);
			else
				len = SPRINTF(buf, "%llu", i);
			assert(18 < len);
			ENSURE_STP_FREE_SPACE(len);
			memcpy(stringpool.free, buf, len);
			v->mvtype = MV_STR;
			v->str.len = len;
			v->str.addr = (char *)stringpool.free;
			stringpool.free += len;
		}
		assert((v->mvtype != MV_NM) || (v->m[1] < MANT_HI));
		assert((v->mvtype != MV_NM) || (v->m[1] >= MANT_LO));
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
		y = y / MANT_HI;
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

void float2mval(mval *dst, float src)
{
        char    buf[16];	/* The maximum-length value in the 'F' representation would look like -0.123457 (that is,
				 * sign[1] + zero[1] + dot[1] + digits[6] = 9). Note that for values below -1 the number would
				 * be shorter, since the integer part would be counted against the precision capacity. However,
				 * a longer representation is possible in the 'E' format: -1.23456E+12 (that is, sign[1] +
				 * digit[1] + dot[1] + digits[5] + E[1] + sign[1] + exp[2] = 12). Although we only need to
				 * additionally worry about one termination character, we will be safe and allocate 16 bytes
				 * instead of 13. */

	SPRINTF(buf, "%.6G", src);
	dst->mvtype = MV_STR;
	dst->str.len = STRLEN(buf);
	dst->str.addr = buf;
	s2n(dst);
	dst->mvtype &= ~MV_STR;
	return;
}

void double2mval(mval *dst, double src)
{
        char    buf[32];		/* The maximum-length value in the 'F' representation would look like -0.123456789012345
					 * (that is, sign[1] + zero[1] + dot[1] + digits[15] = 18). Note that for values below -1
					 * the number would be shorter, since the integer part would be counted against the
					 * precision capacity. However, a longer representation is possible in the 'E' format:
                                         * -1.234567890123456E+123 (that is, sign[1] + digit[1] + dot[1] + digits[14] + E[1] +
					 * sign[1] + exp[3] = 23). Although we only need to additionally worry about one termination
                                         * character, we will be safe and allocate 32 bytes instead of 24. */

	SPRINTF(buf, "%.15G", src);
	dst->mvtype = MV_STR;
	dst->str.len = STRLEN(buf);
	dst->str.addr = buf;
	s2n(dst);
	dst->mvtype &= ~MV_STR;
	return;
}

/* Converts an mval into a 32-bit signed integer, or MAXPOSINT4 on overflow. */
int4 mval2i(mval *v)
{
	int4	i;
	double	j;
	int	exp;

	MV_FORCE_NUM(v);
	if (v->mvtype & MV_INT)
		i = v->m[1] / MV_BIAS;
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

/* Converts an mval into a 32-bit unsigned integer, or MAXUINT4 on overflow. */
uint4 mval2ui(mval *v)
{
	uint4	i;
	double	j;
	int	exp;

	MV_FORCE_NUM(v);
	if (v->mvtype & MV_INT)
		i = v->m[1] / MV_BIAS;
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


/* Converts an mval into a 64-bit unsigned integer. */
gtm_uint64_t mval2ui8(mval *v)
{
	return (gtm_uint64_t)mval2i8(v);
}

gtm_int64_t mval2i8(mval *v)
{
	gtm_int64_t	x, y;
	int		exp;

	MV_FORCE_NUM(v);
	if (v->mvtype & MV_INT)
		x = v->m[1] / MV_BIAS;
	else
	{
		exp = v->e;
		if (exp > EXP_IDX_BIAL)
		{	/* Case where to get the actual value we need to multiply by power of exponent. */
			x = v->m[1];
			y = v->m[0];
			if (y > 0)
			{	/* Both m[0] and m[1] are used, so multiply in parallel, but first ensure that the m[1] part has
				 * a decimal exponent of MANT_HI order.
				 */
				x *= MANT_HI;
				while (exp > EXP_IDX_BIAL + 18)
				{	/* Keep multiplying by 10^9, but keep a precision "buffer" of 18 to prevent further
					 * divisions, as we might otherwise compromise the available precision of mval.
					 */
					x *= MANT_HI;
					y *= MANT_HI;
					exp -= 9;
				}
				if (exp >= EXP_IDX_BIAL + 9)
				{	/* Multiply by the remaining power of the exponent. */
					x *= ten_pwr[exp - EXP_IDX_BIAL - 9];
					y *= ten_pwr[exp - EXP_IDX_BIAL - 9];
				} else
				{	/* Case where exponent indicates a total power of less than 10^9, which, given that both
					 * m[0] and m[1] are used and that x has already been multiplied by 10^9, requires a
					 * division to make the sum of m[0] and m[1] represent the right number.
					 */
					x /= ten_pwr[EXP_IDX_BIAL + 9 - exp];
					y /= ten_pwr[EXP_IDX_BIAL + 9 - exp];
				}
			} else
			{	/* Since m[0] is not used, just multiply x by the excess power of the exponent. */
				while (exp > EXP_IDX_BIAL + 9)
				{
					x *= MANT_HI;
					exp -= 9;
				}
				x *= ten_pwr[exp - EXP_IDX_BIAL];
			}

			x = (v->sgn ? -x - y : x + y);
		} else if (exp < MV_XBIAS)
			x = 0;
		else
			x = (v->sgn ? -v->m[1] : v->m[1]) / ten_pwr[EXP_IDX_BIAL - exp];
	}
	return x;
}

/* isint == v can be represented as a 9 digit (or less) integer (positive or negative).
 * If return value is TRUE, then "*intval" contains the integer value stored in "v".
 * Note: "*intval" could have been updated even if return value is FALSE.
 */
boolean_t isint(mval *v, int4 *intval)
{
	int			exp, m1, mvtype, divisor, m1_div;
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
