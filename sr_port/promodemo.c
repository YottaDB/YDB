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
#include "promodemo.h"

LITREF	int4	ten_pwr[NUM_DEC_DG_1L+1];
LITREF	mval	literal_zero;

void	promote(mval *v)
{
	int	exp ;
	int	m1;
	int4	*pwr;

	m1 = v->m[1];
	if (0 < m1)
		v->sgn = 0 ;
	else if (0 > m1)
	{
		v->sgn = 1 ;
		m1 = -m1;
	} else
	{
		*v = literal_zero ;
		return ;
	}
	v->m[0] = exp = 0 ;
	pwr = (int4 *)&ten_pwr[0];
	while (m1 >= *pwr)
	{
		exp++;
		pwr++;
		assert(pwr < ARRAYTOP(ten_pwr));
	}
	v->m[1] = m1 * ten_pwr[NUM_DEC_DG_1L - exp] ;
	v->mvtype = MV_NM ;
	v->e = EXP_INT_UNDERF + exp ;
}

void	demote(mval *v, int exp, int sign)
{
	assert((0 == exp) || ((EXP_INT_UNDERF < exp) && (EXP_INT_OVERF > exp) && ((EXP_INT_OVERF - 1 - exp) < ARRAYSIZE(ten_pwr))));
	if (0 == exp)
		assert(0 == v->m[1]);
	else if (sign==0)
		v->m[1] /= ten_pwr[EXP_INT_OVERF - 1 - exp] ;
	else
		v->m[1] /= -ten_pwr[EXP_INT_OVERF - 1 - exp] ;
	v->mvtype = MV_NM | MV_INT ;
}
