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
#include "stringpool.h"
#include "eb_muldiv.h"
#include "promodemo.h"
#include "op.h"

LITREF int4	ten_pwr[];
LITREF mval	literal_zero;

error_def(ERR_DIVZERO);
error_def(ERR_NUMOFLOW);

void	op_div (mval *u, mval *v, mval *q)
{
	bool		promo;
	int4		c, exp;
	mval		w, z;

	MV_FORCE_NUM(u);
	MV_FORCE_NUM(v);
	if ((v->mvtype & MV_INT)  &&  v->m[1] == 0)
		rts_error(VARLSTCNT(1) ERR_DIVZERO);
	if (u->mvtype & MV_INT & v->mvtype)
	{
		promo = eb_mvint_div(u->m[1], v->m[1], q->m);
		if (!promo)
		{
			q->mvtype = MV_NM | MV_INT;
			return;
		} else
		{
			w = *u;	     z = *v;
			promote(&w); promote(&z);
			u = &w;	     v = &z;
		}
	} else if (u->mvtype & MV_INT)
	{
		w = *u;
		promote(&w);
		u = &w;
	} else if (v->mvtype & MV_INT)
	{
		w = *v;
		promote(&w);
		v = &w;
	}
	c = eb_div(v->m, u->m, q->m);
	exp = u->e - v->e + c + MV_XBIAS;
	if (EXPHI <= exp)
		rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
	else if (exp < EXPLO)
		*q = literal_zero;
	else if (exp < EXP_INT_OVERF  &&  exp > EXP_INT_UNDERF  &&  q->m[0] == 0  &&  (q->m[1]%ten_pwr[EXP_INT_OVERF-1-exp] == 0))
		demote(q, exp, u->sgn ^ v->sgn);
	else
	{
		q->mvtype = MV_NM;
		q->sgn = u->sgn ^ v->sgn;
		q->e = exp;
	}
	assert(q->m[1] < MANT_HI);
}
