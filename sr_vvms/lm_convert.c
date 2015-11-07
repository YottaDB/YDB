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

/* lm_convert.c : translates a sequence of A - P char to sequence of bits
   used in      : lm_verify, lp_licensed
*/
#include "mdef.h"

#define lo(x)  (x & mask)
#define hi(x)  (x<<4)
#define mask   15

void	lm_convert (cs,bcs)
int4	bcs[] ;					/* result - binary form */
char 	*cs ;					/* check sum A - P form */
{
	unsigned char	*h ;			/* bcs scaled in char	*/
	int 		k  ;

	h= (char *)bcs ;
	k= 0 ;
	while (k!=8)
	{
		h[k] =  lo(*(cs++)-'A') ;
		h[k] |= hi(*(cs++)-'A') ;
		k++ ;
	}
}
