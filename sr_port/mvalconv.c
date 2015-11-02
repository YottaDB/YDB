/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "gtm_stdio.h"	/* this is here due to the need for an sprintf,
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

void	i2flt(mflt *v, int i)
{
	int	exp;
	int4	n;

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
		v->e = 0;
		v->sgn = 0;
		v->m[1] = MV_BIAS * i;
	} else if (n < MANT_HI)
	{
		exp = 8;
		while (n < ten_pwr[exp])
			exp--;
		v->m[1] = n * ten_pwr[8 - exp];
		v->m[0] = 0;
		v->e = exp + MV_XBIAS;
	} else
	{
		v->m[0] = (n % 10) * MANT_LO;
		v->m[1] = n / 10;
		v->e = EXP_IDX_BIAL;
	}
	assert(v->m[1] < MANT_HI);
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

/* isint == v can be represented as a 9 digit (or less) integer */
bool isint (mval *v)
{
	int	exp;
	int4	div;

	if (!(MV_IS_NUMERIC(v)))
		return FALSE;
	assert(v->m[1] < MANT_HI);
	if (v->mvtype & MV_INT)
		return (v->m[1]/MV_BIAS * MV_BIAS) == v->m[1];
	else
	{
		exp = v->e;
		if (exp < MV_XBIAS || exp > EXP_IDX_BIAL || v->m[0]!=0)
			return FALSE;
		div = ten_pwr[EXP_IDX_BIAL - exp];
		return (v->m[1] / div * div) == v->m[1];
	}
}
