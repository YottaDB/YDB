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

#include "mdef.h"
#include "arit.h"
#include "promodemo.h"

LITREF int4 ten_pwr[] ;
LITREF mval literal_zero ;

void promote (mval *v)
{
	int	exp ;
	if ( v->m[1] > 0 )
	{
		v->sgn = 0 ;
	}
	else if ( v->m[1] < 0 )
	{ 	v->sgn = 1 ;
		v->m[1] = -v->m[1] ;
	}
	else
	{	*v = literal_zero ;
		return ;
	}
	v->m[0] = exp = 0 ;
	while ( v->m[1] >= ten_pwr[exp] )
	{
		exp++ ;
	}
	v->m[1] *= ten_pwr[NUM_DEC_DG_1L - exp] ;
	v->mvtype = MV_NM ;
	v->e = EXP_INT_UNDERF + exp ;
}

void demote (mval *v,int exp,int sign)
{
	if (sign==0)
	{
		v->m[1] /= ten_pwr[EXP_INT_OVERF - 1 - exp] ;
	}
	else
	{	v->m[1] /= -ten_pwr[EXP_INT_OVERF - 1 - exp] ;
	}
	v->mvtype = MV_NM | MV_INT ;

}
