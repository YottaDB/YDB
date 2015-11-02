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

/*	flt_mod.(u, v) = u - (v*floor.(u/v))  where  x-1 < floor.x <= x ^ int.x */

#include "mdef.h"

#include "arit.h"
#include "op.h"
#include "eb_muldiv.h"
#include "promodemo.h"
#include "flt_mod.h"

LITREF mval	literal_zero;
LITREF int4	ten_pwr[];

void	flt_mod (mval *u, mval *v, mval *q)
{
	int	exp;
	int4	z, x;
	mval	w;			/* temporary mval for division result */
	mval	y;			/* temporary mval for extended precision promotion
					   to prevent modifying caller's data */
	mval	*u_orig;		/* original (caller's) value of u */
	error_def(ERR_DIVZERO);

	u_orig = u;
	MV_FORCE_NUM(u);
	MV_FORCE_NUM(v);

	if ((v->mvtype & MV_INT) != 0  &&  v->m[1] == 0)
		rts_error(VARLSTCNT(1) ERR_DIVZERO);

	if ((u->mvtype & MV_INT & v->mvtype) != 0)
	{
		/* Both are INT's; use shortcut.  */
		q->mvtype = MV_NM | MV_INT;
		eb_int_mod(u->m[1], v->m[1], q->m);
		return;
	}
	else if ((u->mvtype & MV_INT) != 0)
	{
		/* u is INT; promote to extended precision for compatibility with v.  */
		y = *u;
		promote(&y);		/* y will be normalized, but not in canonical form */
		u = &y;			/* this is why we need u_orig */
	}
	else if ((v->mvtype & MV_INT) != 0)
	{
		/* v is INT; promote to extended precision for compatibility with u.  */
		y = *v;
		promote(&y);
		v = &y;
	}

	/* At this point, both u and v are in extended precision format.  */

	/* Set w = floor(u/v).  */
	op_div (u, v, &w);
	if ((w.mvtype & MV_INT) != 0)
		promote(&w);
	exp = w.e;
	if (exp <= MV_XBIAS)
	{
		/* Magnitude of w, floor(u/v), is < 1.  */
		if (u->sgn != v->sgn  &&  w.m[1] != 0  &&  exp >= EXPLO)
		{
			/* Signs differ (=> floor(u/v) < 0) and (w != 0) and (no underflow) => floor(u/v) == -1 */
			w.sgn = 1;
			w.e = MV_XBIAS + 1;
			w.m[1] = MANT_LO;
			w.m[0] = 0;
		}
		else
		{
			/* Signs same (=> floor(u/v) >= 0) or (w == 0) or (underflow) => floor(u/v) == 0 */
			*q = *u_orig;	/* u - floor(u/v)*v == u - 0*v == u */
			return;
		}
	}
	else if (exp < EXP_IDX_BIAL)
	{
		z = ten_pwr[EXP_IDX_BIAL - exp];
		x = (w.m[1]/z)*z;
		if (u->sgn != v->sgn  &&  (w.m[1] != x  ||  w.m[0] != 0))
		{
			w.m[0] = 0;
			w.m[1] = x + z;
			if (w.m[1] >= MANT_HI)
			{
				w.m[0] = w.m[0]/10 + (w.m[1]%10)*MANT_LO;
				w.m[1] /= 10;
				w.e++;
			}
		}
		else
		{
			w.m[0] = 0;
			w.m[1] = x;
		}
	}
	else if (exp < EXP_IDX_BIAQ)
	{
		z = ten_pwr[EXP_IDX_BIAQ - exp];
		x = (w.m[0]/z)*z;
		if (u->sgn != v->sgn  &&  w.m[0] != x)
		{
			w.m[0] = x + z;
			if (w.m[0] >= MANT_HI)
			{
				w.m[0] -= MANT_HI;
				w.m[1]++;
			}
		}
		else
		{
			w.m[0] = x;
		}
	}

	op_mul (&w, v, &w);		/* w = w*v = floor(u/v)*v       */
	op_sub (u_orig, &w, q);		/* q = u - w = u - floor(u/v)*v */
}
