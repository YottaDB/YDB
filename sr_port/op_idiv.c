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
#include "eb_muldiv.h"
#include "promodemo.h"
#include "op.h"

LITREF int4	ten_pwr[];
LITREF mval	literal_zero;

error_def(ERR_NUMOFLOW);

void	op_idiv(mval *u, mval *v, mval *q)
{
	bool		promo;
	int4		z, c, exp;
	mval		w, y;

	error_def	(ERR_DIVZERO);

	MV_FORCE_NUM(u);
	MV_FORCE_NUM(v);
	if ((v->mvtype & MV_INT)  &&  v->m[1] == 0)
		rts_error(VARLSTCNT(1) ERR_DIVZERO);
	if (u->mvtype & MV_INT & v->mvtype)
	{
		promo = eb_int_div(u->m[1], v->m[1], q->m);
		if (!promo)
		{
			q->mvtype = MV_NM | MV_INT;
			return;
		} else
		{
			w = *u;
			y = *v;
			promote(&w);
			promote(&y);
			u = &w;
			v = &y;
		}
	} else  if (u->mvtype & MV_INT)
	{
		w = *u;
		promote(&w);
		u = &w;
	} else  if (v->mvtype & MV_INT)
	{
		w = *v;
		promote(&w);
		v = &w;
	}
	exp = u->e - v->e + MV_XBIAS;
	if (exp < MV_XBIAS)
		*q = literal_zero;
	else
	{
		c = eb_div(v->m, u->m, q->m);
		exp += c;
		if (exp <= MV_XBIAS)
			*q = literal_zero;
		else  if (exp < EXP_IDX_BIAL)
		{
			assert(EXP_IDX_BIAL - exp >= 0);
			z = ten_pwr[EXP_IDX_BIAL - exp];
			q->m[1] = (q->m[1] / z) * z;
			q->m[0] = 0;
		}
		else  if (exp < EXP_IDX_BIAQ)
		{
			assert(EXP_IDX_BIAQ - exp >= 0);
			z = ten_pwr[EXP_IDX_BIAQ - exp];
			q->m[0] = (q->m[0] / z) * z;
		}

		if (exp < EXP_INT_OVERF  &&  exp > EXP_INT_UNDERF
		    &&  q->m[0] == 0  &&  (q->m[1]%ten_pwr[EXP_INT_OVERF-1-exp] == 0))
			demote(q, exp, u->sgn ^ v->sgn);
		else
		{
			assert(EXPLO <= exp);
			if (EXPHI <= exp)
				rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
			q->e = exp;
			q->sgn = u->sgn ^ v->sgn;
			q->mvtype = MV_NM;
		}
	}
	assert(q->m[1] < MANT_HI);
}
