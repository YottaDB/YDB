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
#ifdef UNIX
#include <math.h>
#endif
#include "stringpool.h"
#include "op.h"
#include "mvalconv.h"

#define ACCUR_PERCENT	0.00000000000000055
#define TEN_DEG_INT 1000000000
#define MAX_M_INT 999999999

LITREF mval literal_one;
LITREF mval literal_zero;

void op_exp(mval *u, mval* v, mval *p)
{
	mval 	u1, *u1_p;
	double 	accuracy;
	double 	x, x1, y, z, z2, z3, z4, z5, id, il;
#ifndef UNIX
	double 	pow();
#endif
	int	 im0, im1, ie, i, j, j1;
	bool 	fraction = FALSE, in = FALSE;
	bool 	neg = FALSE, change_sn = FALSE, even = TRUE;
        mval    w, zmv;
        int4    n, n1;
#ifdef UNIX
	double	infinity, np = 20, inf = 9999999999999999.0;
#endif

	error_def(ERR_NUMOFLOW);
	error_def(ERR_NEGFRACPWR);

	u1_p = &u1;
	memcpy(u1_p, u, SIZEOF(mval));

        MV_FORCE_NUM(u1_p);
        MV_FORCE_NUM(v);

        if (v->m[1] == 0 && v->m[0] == 0)
        {
                *p = literal_one;
                return;
        }

        if ((v->mvtype & MV_INT) != 0)
        {
                n = v->m[1];
		if (n == 0)
		{
			*p = literal_one;
			return;
		}
	        if ((u1_p->mvtype & MV_INT) != 0)
        	{
			if (u1_p->m[1] == 0)
			{
                		*p = literal_zero;
		                return;
			}
        	} else if (u1_p->m[1] == 0 && u1_p->m[0] == 0)
		{
			*p = literal_zero;
			return;
		}
                n1 = n / MV_BIAS;
                if (n == n1 * MV_BIAS)
                {
                        if (v->m[1]==0)
                        {
                                *p = literal_one;
                                return;
                        }
                        if (n1 < 0)
                        {
                                op_div((mval *)&literal_one, u1_p, &w);
                                n1 = -n1;
                        } else
				w = *u1_p;
                        zmv = literal_one;
                        for ( ; ; )
                        {
                                if (n1 & 1)
                                        op_mul(&zmv,&w,&zmv);
                                n1 /= 2;
                                if (!n1)
                                        break;
                                op_mul(&w,&w,&w);
                        }
                        *p = zmv;
                        return;
                } else
                {
			if ((u1_p->mvtype & MV_INT) != 0)
			{
                        	if (u1_p->m[1] < 0)
                        	{
                                	rts_error(VARLSTCNT(1) ERR_NEGFRACPWR);
                                	return;
                        	}
			} else
			{
				if (u1_p->sgn)
				{
					rts_error(VARLSTCNT(1) ERR_NEGFRACPWR);
					return;
				}
			}
                }
        } else
	{
	        if ((u1_p->mvtype & MV_INT) != 0)
        	{
                	if (u1_p->m[1] < 0)
                	{
                        	u1_p->m[1] = - u1_p->m[1];
	                        neg = TRUE;
        	        }
			if (u1_p->m[1] == 0)
			{
				*p = literal_zero;
				return;
			}
        	} else if (u1_p->sgn)
        	{
                	u1_p->sgn = 0;
	                neg = TRUE;
			if (u1_p->m[1] == 0 && u1_p->m[0] == 0)
			{
				*p = literal_zero;
				return;
			}
        	}
		if ((ie = v->e - MV_XBIAS) < NUM_DEC_DG_2L)
		{
			if (ie > 0)
			{
				if (ie < NUM_DEC_DG_1L)
				{
					for (i=1,j=10; i < NUM_DEC_DG_1L - ie; j *= 10, i++);
					im1 = v->m[1];
					if ((i = im1 % j) != 0)
						fraction = TRUE;
					else
						if ((i = im1 % (2 * j)) != 0)
							even = FALSE;
				} else
				{
					im0 = v->m[0];
					if (ie == NUM_DEC_DG_1L)
					{
						if (im0 == 0)
						{
							im1 = v->m[1];
							if ((i = im1 % 2) != 0)
								even = FALSE;
						} else
							fraction = TRUE;
					} else
					{
						for (i=1,j=10; i < NUM_DEC_DG_2L - ie; j *= 10, i++);
						im0 = v->m[0];
						if ((i = im0 % j) == 0)
						{
							if ((i = im0 % (2 * j)) != 0)
								even = FALSE;
						} else
							fraction = TRUE;
					}
				}
			} else
				fraction = TRUE;
		} else
		{
			if (ie == NUM_DEC_DG_2L)
			{
				im0 = v->m[0];
				if ((i = im0 % 2) != 0)
					even = FALSE;
			}
		}
		if (!fraction)
		{
			if (neg)
			{
				if (!even)
					change_sn = TRUE;
			}
		} else
		{
			if (neg)
			{
				rts_error(VARLSTCNT(1) ERR_NEGFRACPWR);
				return;
			}
		}
	}

	x = mval2double(u1_p);
	y = mval2double(v);

	z = pow(x, y);

#ifdef UNIX
#ifdef __sun
	/* Solaris doesn't define the more portable HUGE_VAL, but all the other Unix platforms do, so we have to use pow() to
	 * cause an overflow and capture that value for the comparison. */
	infinity = pow(inf, np);
#else
	infinity = HUGE_VAL;
#endif
	if (z == infinity)
	{
		rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
	        return;
	}
#endif

        if (change_sn)
                z = -z;

	if (z < 0)
	{
		z = -z;
		p->sgn = 1;
	} else
		p->sgn = 0;

	if (z == 0)
	{
		*p = literal_zero;
		return;
	}

	accuracy = z * ACCUR_PERCENT;
	for (z2 = ACCUR_PERCENT; accuracy == 0; z2 *= 10, accuracy = z * z2);

	if (z <= accuracy)
	{
		*p = literal_zero;
		return;
	}

	if (z < 1)
	{
        	if (1 - z <= accuracy)
	        {
                	*p = literal_one;
        	        return;
	        }
		for(id = 1, j = 0; z < id; id /=10, j++)
		{
	                if ((id - z) < accuracy)
			{
				in = TRUE;
				if (j <= (i = MV_XBIAS - EXP_INT_UNDERF))
                        	{
                               		*p = literal_one;
					p->mvtype = MV_NM | MV_INT;
					p->m[1] = (int)(p->m[1] * id);
					if (p->m[1] == TEN_DEG_INT)
						p->m[1] = MAX_M_INT;
					if (p->sgn)
						p->m[1] = - p->m[1];
                	               	return;
				}
				j++;
				break;
			}
		}
                if (!in && (z - id < accuracy))
                {
                        if (j <= (i = MV_XBIAS - EXP_INT_UNDERF))
                        {
                                *p = literal_one;
                                p->mvtype = MV_NM | MV_INT;
                                p->m[1] = (int)(p->m[1] * id);
                                if (p->m[1] == TEN_DEG_INT)
                                        p->m[1] = MAX_M_INT;
                                if (p->sgn)
                                        p->m[1] = - p->m[1];
                                return;
                        }
                }
		p->mvtype = MV_NM;
		if (MV_XBIAS - j + 1 < EXPLO)
		{
			*p = literal_zero;
			return;
		}
		p->e = MV_XBIAS - j +1;
		j1 = NUM_DEC_DG_2L - 1 + j;
		for (i = 0,z2 = z; i < j1; z2 *= 10,i++);
		j1 = NUM_DEC_DG_1L - 1 + j;
		for (i = 0,z4 = z; i < j1; z4 *= 10,i++);
		p->m[1] = (int)z4;
                if (p->m[1] == TEN_DEG_INT)
                	p->m[1] = MAX_M_INT;
		z4 = ((double)(p->m[1]))*MANT_HI;
		z3 = z2 - z4;
		p->m[0] = (int)z3;
                if (p->m[0] == TEN_DEG_INT)
                        p->m[0] = MAX_M_INT;
		return;
	}

	if (z - 1 <= accuracy)
	{
		*p = literal_one;
		return;
	}

	if (z > 1)
	{
		for(il = 10, j = 1; z > il; il *=10, j++)
		{
			if ((z - il) < accuracy)
			{
				in = TRUE;
                		if (++j <= (i = EXP_INT_OVERF - MV_XBIAS))
                        	{
        	                	i2mval(p, (int)il);
	                                if (p->sgn)
        	                                p->m[1] = - p->m[1];
                	                return;
                        	}
				break;
			}
		}
                if (!in && (il - z < accuracy))
                {
                        if (j <= (i = EXP_INT_OVERF - MV_XBIAS))
                        {
                                i2mval(p, (int)il);
                                if (p->sgn)
                                        p->m[1] = - p->m[1];
                                return;
			}
                }
		p->mvtype = MV_NM;
                if ((j + MV_XBIAS) >= EXPHI)
                {
                        rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
                        return;
                }
		p->e = j + MV_XBIAS;
		if (j < NUM_DEC_DG_2L)
		{
			j1 = NUM_DEC_DG_2L - j;
			for (i = 0,z2 = z; i < j1; z2 *= 10,i++);
		} else
		{
			if (j == NUM_DEC_DG_2L)
				z2 = z;
			else
			{
				j1 = j - NUM_DEC_DG_2L;
				for (i = 0,z2 = z; i < j1; z2 /= 10,i++);
			}
		}
		p->m[1] = z2 / MANT_HI;
                if (p->m[1] == TEN_DEG_INT)
                        p->m[1] = MAX_M_INT;
		z4 = ((double)(p->m[1]))*MANT_HI;
		p->m[0] = (int)(z2 - z4);
                if (p->m[0] == TEN_DEG_INT)
                        p->m[0] = MAX_M_INT;
		return;
	}
}








