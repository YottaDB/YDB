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
#include "arit.h"
#include "op.h"

LITREF int4	ten_pwr[] ;
LITREF mval	literal_zero;

error_def(ERR_NUMOFLOW);

void add_mvals(mval *u, mval *v, int subtraction, mval *result)
{
        int delta, uexp, vexp, exp;
        int4 m0, m1, n0, n1, x, factor;
	char usign, vsign, rsign;

        m1 = u->m[1];
        if ((u->mvtype & MV_INT) == 0)
        {
		usign = u->sgn;
                m0 = u->m[0];
                uexp = u->e;
        } else
        {
		if (m1 == 0)
			goto result_is_v;
                m0 = 0;
		if (m1 > 0)
			usign = 0;
		else
		{
			usign = 1;
			m1 = -m1;
		}
		for (uexp = EXP_INT_OVERF - 1 ; m1 < MANT_LO ; m1 *= 10 , uexp--)
                        ;
        }
        n1 = v->m[1];
        if ((v->mvtype & MV_INT) == 0)
        {
                n0 = v->m[0];
                vexp = v->e;
		vsign = v->sgn ^ subtraction;
        } else
        {
		if (n1 == 0)
			goto result_is_u;
		else if (n1 > 0)
			vsign = subtraction;
		else
		{
			vsign = !subtraction;
			n1 = -n1;
		}
                n0 = 0;
		for (vexp = EXP_INT_OVERF - 1 ; n1 < MANT_LO ; n1 *= 10 , vexp--)
                        ;
        }
        delta = uexp - vexp;
	if (delta >= 0)
	{
		exp = uexp;
		if (delta >= NUM_DEC_DG_2L)
			goto result_is_u;
        	else if (delta >= NUM_DEC_DG_1L)
        	{
                	n0 = n1 / ten_pwr[delta - NUM_DEC_DG_1L];
                	n1 = 0;
        	} else if (delta > 0)
        	{
                	factor = ten_pwr[delta];
                	x = n1;
                	n1 /= factor;
                	n0 = (x - (n1 * factor)) * ten_pwr[NUM_DEC_DG_1L - delta] + (n0 / factor);
        	}
	} else
        {
		exp = vexp;
		if (delta <= - NUM_DEC_DG_2L)
			goto result_is_v;
        	else if (delta <= - NUM_DEC_DG_1L)
        	{
                	m0 = m1 / ten_pwr[-delta - NUM_DEC_DG_1L];
                	m1 = 0;
        	} else
        	{
                	factor = ten_pwr[-delta];
                	x = m1;
                	m1 /= factor;
                	m0 = (x - (m1 * factor)) * ten_pwr[NUM_DEC_DG_1L + delta] + (m0 / factor);
        	}
        }
	if (usign == vsign)
	{
		/* Perform addition */
		m0 += n0;
		if (m0 >= MANT_HI)
		{
			m1++;
			m0 -= MANT_HI;
		}
		m1 += n1;
		if (m1 >= MANT_HI)
		{
			x = m1 / 10;
			m0 = (m0 / 10) + (m1 - x * 10) * MANT_LO;
			m1 = x;
			exp++;
		}
		rsign = usign;
	} else
	{
		/* perform subtraction */
		if (delta < 0 || (delta == 0 && (m1 < n1  || (m1 == n1 && m0 < n0))))
		{
			x = m1;
			m1 = n1;
			n1 = x;
			x = m0;
			m0 = n0;
			n0 = x;
			rsign = vsign;
		} else
		{
			rsign = usign;
		}
		m0 -= n0;
		if (m0 < 0)
		{
			m1--;
			m0 += MANT_HI;
			assert(m0 > 0 && m0 < MANT_HI);
		}
		m1 -= n1;
		if (m1 == 0)
		{
			exp -= NUM_DEC_DG_1L;
			m1 = m0;
			if (m1 == 0)
			{
				result->mvtype = MV_NM | MV_INT;
				result->m[1] = 0;
				return;
			}
			m0 = 0;
		}
		if (m1 < 0)
		{
			m1 += MANT_HI;
			exp--;
		}
		if (m1 < MANT_LO)
		{
			for (delta = 0 ; m1 < MANT_LO ; delta++)
				m1 *= 10;
			assert(delta > 0 && delta <= NUM_DEC_DG_1L);
			factor = ten_pwr[NUM_DEC_DG_1L - delta];
			x = m0 / factor;
			m1 += x;
			m0 = (m0 - x * factor) * ten_pwr[delta];
			exp -= delta;
		}
	}
	if ( exp < EXP_INT_OVERF && exp > EXP_INT_UNDERF && m0 == 0)
	{
		factor = ten_pwr[EXP_INT_OVERF - 1 - exp];
		x = m1 / factor;
		if (x * factor == m1)
		{
			result->mvtype = MV_NM | MV_INT;
			result->m[1] = rsign ? - x : x;
			return;
		}
	}
	if (EXPHI <= exp)
		rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
	else if (EXPLO > exp)
		*result = literal_zero;
	else
	{
		result->mvtype = MV_NM;
		result->sgn = rsign;
		result->e = exp;
		result->m[0] = m0;
		result->m[1] = m1;
	}
        return;

result_is_u:
	*result = *u;
	MV_FORCE_CANONICAL(result);
	return;

result_is_v:
	if (subtraction)
	{
		if (v->mvtype & MV_INT)
		{
			result->mvtype = (MV_NM | MV_INT);
			result->m[1] = - v->m[1];
		} else
		{
			result->mvtype = MV_NM;
			result->sgn = !v->sgn;
			result->e = v->e;
			result->m[0] = v->m[0];
			result->m[1] = v->m[1];
		}
	} else
	{
		*result = *v;
		MV_FORCE_CANONICAL(result);
	}
	return;
}

void op_add (mval *u, mval *v, mval *s)
{
        int4    m0, m1;
	char	utype, vtype;

	MV_FORCE_NUM(u);
	MV_FORCE_NUM(v);
	utype = u->mvtype;
	vtype = v->mvtype;
	if ( utype & vtype & MV_INT )
	{
		m1 = u->m[1] + v->m[1] ;
		if (m1 < MANT_HI && m1 > -MANT_HI)
                {
			s->mvtype = MV_INT | MV_NM ;
                        s->m[1] = m1;
                        return;
                }
		if ( m1 > 0)
		{
                        s->sgn = 0;
                } else
                {
			s->sgn = 1;
                        m1 = -m1;
                }
                s->mvtype = MV_NM;
                s->e = EXP_INT_OVERF;
                m0 = m1 / 10;
		s->m[0] = (m1 - (m0 * 10)) * MANT_LO;
		s->m[1] = m0;
                return;
	}
	add_mvals(u, v, 0, s);
        return;
}

void op_sub (mval *u, mval *v, mval *s)
{
        int4    m0, m1;
	char	utype, vtype;

	MV_FORCE_NUM(u);
	MV_FORCE_NUM(v);
	utype = u->mvtype;
	vtype = v->mvtype;
	if ( utype & vtype & MV_INT )
	{
		m1 = u->m[1] - v->m[1] ;
		if (m1 < MANT_HI && m1 > -MANT_HI)
                {
			s->mvtype = MV_INT | MV_NM ;
                        s->m[1] = m1;
                        return;
                }
		if ( m1 > 0)
		{
                        s->sgn = 0;
                } else
                {       s->sgn = 1;
                        m1 = -m1;
                }
                s->mvtype = MV_NM;
                s->e = EXP_INT_OVERF;
                m0 = m1 / 10;
		s->m[0] = (m1 - (m0 * 10)) * MANT_LO;
		s->m[1] = m0;
                return;
	}
	add_mvals(u, v, 1, s);
        return;
}
