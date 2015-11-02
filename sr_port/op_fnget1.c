/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "op.h"

/* This code is similar in function to opp_fnget (assembler) except, if src is undefined, this returns an undefined value to signal
 * op_fnget2, which, in turn, returns a specified "default" value; this slight of hand deals with order of evaluation issues.
 */
void op_fnget1(mval *src, mval *dst)
{
	if (src && MV_DEFINED(src))
	{
		*dst = *src;
		dst->mvtype &= ~MV_ALIASCONT;		/* Make sure alias container property does not pass */
	}
	else
		dst->mvtype = 0;
	return;
}
