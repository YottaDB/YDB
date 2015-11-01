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
#include "stringpool.h"
#include "underr.h"

#define DIGIT(x)	( x >='0' && x <= '9' )
#define NUM_MASK	( MV_NM | MV_INT | MV_NUM_APPROX )

GBLDEF bool s2n_intlit ;
GBLREF bool compile_time ;
LITREF mval literal_null ;
LITREF int4 ten_pwr[] ;

char *s2n (mval *u)
{	error_def (ERR_NUMOFLOW) ;
	char	*c, *d, *w, *eos ;
	int	i, j, k, x, y, z, sign, zero ;
	bool	digit, dot, exp, exneg, tail ;

	i = 0;
	MV_FORCE_DEFINED(u);
	c = u->str.addr;
	if ( u->str.len==0 )
	{
		s2n_intlit = 1;
		*u = literal_null ;
		return c ;
	}
	eos = u->str.addr + u->str.len;
	sign = 0 ;
	while ( c < eos && ( *c == '-' || *c == '+' ))
	{
		sign += ( *c++ == '-' ? 1 : 2 ) ;
	}
	zero = 0 ;
	while ( c < eos && *c == '0' )
	{
		zero++ ; c++ ;
	}
	dot = ( c < eos && *c == '.' ) ;
	if (dot)
		c++ ;
	y = 0 ;
	while ( c < eos && *c == '0' )
	{
		c++ ; y-- ;
	}
	z = u->m[0] = u->m[1] = 0 ;				/* R0 */
	d = c + 9 ;
	w = ( d < eos ? d : eos ) ;
	while ( c < w && DIGIT(*c) )
	{
		if ( *c == '0' )
		{
			u->m[1] *= 10 ; i++ ;
		}
		else
		{	i = 0 ;
			u->m[1] = u->m[1] * 10 + (*c - '0') ;
		}
		c++ ; z++ ;
	}							/* R1 */
	if ( c < w && *c == '.' && !dot )
	{
		y = z ; c++ ; d++ ; dot = TRUE ;
		if ( w < eos )
			w++ ;
	}
	while ( c < w && DIGIT(*c) )
	{
		if ( *c == '0' )
		{
			u->m[1] *= 10 ; i++ ;
		}
		else
		{	i = 0 ;
			u->m[1] = u->m[1] * 10 + (*c - '0') ;
		}
		c++ ;
	}							/* R2 */
	k = d - c ;
	if ( c < eos )
	{	d = c + 9 ;
		w = ( d < eos ? d : eos ) ;
		while ( c < w && DIGIT(*c) )
		{
			u->m[0] = u->m[0] * 10 + (*c++ - '0') ; z++ ;
		}
		if ( c < w && *c == '.' && !dot )
		{
			y = z ; c++ ; d++ ; dot = TRUE ;
			if ( w < eos )
				w++ ;
		}
		while ( c < w && DIGIT(*c) )
		{
			u->m[0] = u->m[0] * 10 + (*c++ - '0') ;
		}
		u->m[0] *= ten_pwr[d - c] ;
		while ( c < eos && *c == '0' )
		{
			c++ ; z++ ;
		}
	}
	tail = ( dot && ( *(c-1)=='0' || *(c-1)=='.')) || c != eos ;
	while ( c < eos && DIGIT(*c) )
	{
		c++ ; z++ ;
	}
	digit = z!=0 || y!=0 || zero!=0 ;
	x = 0 ; exp = ( *c=='E' && digit ) ;
	if ( exp && c+1 < eos )
	{
		c++ ;
		exneg = *c == '-' ;
		if ( exneg || *c == '+' )
		{
			c++ ;
		}
		while ( c < eos && DIGIT(*c) )
		{
			x = x * 10 + (*c++ - '0') ;
		}
		if ( exneg )
		{
			x = -x ;
		}
	}
	s2n_intlit = sign!=0 || dot || exp ;
	if ( digit )
	{
		x += ( dot ? y : z ) ;
		j = x+k-6 ; i += j ;
		if ( u->m[0]==0 && x<=6 && i>=0 )
		{
			u->mvtype |= ( tail || sign > 1 || (zero!=0 && u->str.len!=1)  ?
				MV_NM | MV_INT | MV_NUM_APPROX : MV_NM | MV_INT ) ;
			if ( j < 0 )
			{
				u->m[1] /= ( sign & 1 ? -ten_pwr[-j] : ten_pwr[-j] ) ;
			}
			else
			{	u->m[1] *= ( sign & 1 ? -ten_pwr[j] : ten_pwr[j] ) ;
			}
		}
		else
		{	u->m[1] *= ten_pwr[k] ;
			x += MV_XBIAS ;
			if ( x < EXPLO || u->m[1] == 0 )
			{
				u->mvtype |= MV_NM | MV_INT | MV_NUM_APPROX ; u->m[1] = 0 ;
			}
			else if ( x >= EXPHI )
			{
				u->mvtype &= ~NUM_MASK ;
				if ( !compile_time )
					rts_error(VARLSTCNT(1) ERR_NUMOFLOW) ;
			} else
			{	u->e = x ; u->sgn = sign & 1 ;
				u->mvtype |= ( tail || sign > 1 || (zero!=0 && u->str.len!=1)  ? MV_NM | MV_NUM_APPROX : MV_NM ) ;
			}
		}
		assert(u->m[1] < MANT_HI);
	}
	else
	{	u->mvtype |= MV_NM | MV_INT | MV_NUM_APPROX ;
		u->m[1] = 0 ;
	}
	return c ;
}
