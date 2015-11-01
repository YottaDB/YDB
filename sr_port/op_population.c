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
#include "op.h"
#include "mvalconv.h"

void	op_population(mval *arg1, mval *arg2, mval *dst)
{
	int 	x, y;
	mval	dummy;

	y = 0;
	MV_FORCE_STR(arg1);
	MV_FORCE_STR(arg2);
	if (arg2->str.len)
	{	for (x = 1; x ; y++)
		{	x = op_fnfind(arg1, arg2, x, &dummy);
		}
	}
	MV_FORCE_MVAL(dst,y) ;
}
