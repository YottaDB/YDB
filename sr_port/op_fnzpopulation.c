/****************************************************************
 *								*
 *	Copyright 2006 Fidelity Information Services, Inc	*
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

/* Called for $LENGTH() when a second argument is supplied. Returns the
   number of "pieces" in the string given the supplied delimiter.
*/
void	op_fnzpopulation(mval *arg1, mval *arg2, mval *dst)
{
	int 	x, y;
	mval	dummy;

	y = 0;
	MV_FORCE_STR(arg1);
	MV_FORCE_STR(arg2);
	if (arg2->str.len)
		for (x = 1; x ; y++) x = op_fnzfind(arg1, arg2, x, &dummy);

	MV_FORCE_MVAL(dst,y) ;
}
