/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "op.h"

void op_fnget2(mval *dst, mval *src, mval *defval)
{
	MV_FORCE_DEFINED(defval);
	if (src && MV_DEFINED(src))
		*dst = *src;
	else
		*dst = *defval;
	dst->mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
}
