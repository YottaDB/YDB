/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "toktyp.h"

LITREF int4	ten_pwr[];
LITREF mval	literal_zero;
LITREF mval	literal_sqlnull;

void	op_mul(mval *u, mval *v, mval *p)
{
	boolean_t	promo;
	int4		c, exp;
	mval		w, z;
	int		u_mvtype, v_mvtype;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* If u or v is $ZYSQLNULL, the result is $ZYSQLNULL */
	if (MV_IS_SQLNULL(u) || MV_IS_SQLNULL(v))
	{
		MV_FORCE_DEFINED(u);
		MV_FORCE_DEFINED(v);
		*p = literal_sqlnull;
		return;
	}
	MV_FORCE_NUM(u);
	MV_FORCE_NUM(v);
	u_mvtype = u->mvtype;
	v_mvtype = v->mvtype;
	if (u_mvtype & MV_INT & v_mvtype)
	{
		promo = eb_int_mul(u->m[1], v->m[1], p->m);
		if (!promo)
		{
			p->mvtype = MV_NM | MV_INT;
			return;
		} else
		{
			w = *u;      z = *v;
			promote(&w); promote(&z);
			u = &w;	     v = &z;
		}
<<<<<<< HEAD
	} else if (u_mvtype & MV_INT)
=======
	} else if (u->mvtype & MV_INT)
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
	{
		w = *u;
		promote(&w);
		u = &w;
<<<<<<< HEAD
	} else if (v_mvtype & MV_INT)
=======
	} else if (v->mvtype & MV_INT)
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
	{
		w = *v;
		promote(&w);
		v = &w;
	}
	c = eb_mul(u->m, v->m, p->m);
	exp = u->e + v->e + c - MV_XBIAS;
	if (EXPHI <= exp)
	{
		TREF(last_source_column) += (TK_EOL == TREF(director_token)) ? -2 : 2;	/* improve hints */
		rts_error_csa(NULL, VARLSTCNT(1) ERR_NUMOFLOW); /* BYPASSRTSABT */
	} else if (EXPLO > exp)
		*p = literal_zero;
	else if (exp < EXP_INT_OVERF  &&  exp > EXP_INT_UNDERF  &&  p->m[0] == 0  &&  (p->m[1]%ten_pwr[EXP_INT_OVERF-1-exp]==0))
		demote(p, exp, u->sgn ^ v->sgn);
	else if ((0 == p->m[0]) && (0 == p->m[1]))
		*p = literal_zero;
	else
	{
		if (p->m[1] || p->m[0])
		{
			p->mvtype = MV_NM;
			p->sgn = u->sgn ^ v->sgn;
			p->e = exp;
		} else
			*p = literal_zero;
	}
	assert(p->m[1] < MANT_HI);
}
