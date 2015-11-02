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

void	op_mul (mval *u, mval *v, mval *p)
{
	bool		promo;
	int4		c, exp;
	mval		w, z;
	error_def	(ERR_NUMOFLOW);

	MV_FORCE_NUM(u);
	MV_FORCE_NUM(v);
	if (u->mvtype & MV_INT & v->mvtype)
	{
		promo = eb_int_mul(u->m[1], v->m[1], p->m);
		if (!promo)
		{
			p->mvtype = MV_NM | MV_INT;
			return;
		}
		else
		{
			w = *u;      z = *v;
			promote(&w); promote(&z);
			u = &w;	     v = &z;
		}
	}
	else if (u->mvtype & MV_INT)
	{
		w = *u;
		promote(&w);
		u = &w;
	}
	else if (v->mvtype & MV_INT)
	{
		w = *v;
		promote(&w);
		v = &w;
	}
	c = eb_mul(u->m, v->m, p->m);
	exp = u->e + v->e + c - MV_XBIAS;
	if (EXPHI <= exp)
		rts_error(VARLSTCNT(1) ERR_NUMOFLOW);
	else if (EXPLO > exp)
		*p = literal_zero;
	else if (exp < EXP_INT_OVERF  &&  exp > EXP_INT_UNDERF  &&  p->m[0] == 0  &&  (p->m[1]%ten_pwr[EXP_INT_OVERF-1-exp]==0))
		demote(p, exp, u->sgn ^ v->sgn);
	else
	{
		p->mvtype = MV_NM;
		p->sgn = u->sgn ^ v->sgn; p->e = exp;
	}
	assert(p->m[1] < MANT_HI);
}
