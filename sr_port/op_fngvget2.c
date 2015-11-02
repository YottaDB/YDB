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

LITREF mval		literal_null;

int op_fngvget2(mval *res, mval *val, mval *optional)
{
	MV_FORCE_DEFINED(optional);
	if (MV_DEFINED(val))
		*res = *val;
	else
		*res = *optional;
	assert(0 == (res->mvtype & MV_ALIASCONT));	/* Should be no alias container flag in this global */
	return TRUE;
}
