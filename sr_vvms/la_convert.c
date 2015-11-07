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

/* la_convert.c : translates sequence of bits in bcs[2] to sequence of A - P char
   used in      : la_create.c
*/
#include "mdef.h"
#include "ladef.h"

#define conv(byte,even)  ((even ? lo(byte):hi(byte)) + 'A')
#define lo(x)  (x & mask)
#define hi(x)  (x>>4)
#define mask   15

void	la_convert (
int4	bcs[] ,
char 	*cs )				/* result, check sum A - P form */
{
	unsigned char	*h   ;			/* bcs scaled in char	*/
	int 		k,j  ;
	bool 		even ;

	h= (char *)bcs ;
	even= TRUE ;
	k= j= 0 ;
	while (j!=8)
	{
		cs[k]= conv(h[j],even) ;
		even= !even ;
		cs[k+1]= conv(h[j],even) ;
		even= !even ;
		k += 2 ; j++ ;
	}
}
