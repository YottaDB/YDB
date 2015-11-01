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

LITREF mval		literal_null;

int op_fngvget2(mval *res, mval *val, mval *optional)
{
	if (MV_DEFINED(val))
		*res = *val;
	else
		*res = *optional;
	return TRUE;
}
