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
#include "mvalconv.h"
#include "op.h"

void	op_fnascii(int4 num, mval *in, mval *out)
{	int	k ;
	MV_FORCE_STR(in);
	num--;
	if ( num >= in->str.len || num < 0 )
	{
		k = -1 ;
	}
	else
		k = *(unsigned char *)(in->str.addr + num) ;
	MV_FORCE_MVAL(out,k) ;
}
