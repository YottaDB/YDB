/****************************************************************
*								*
* Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
* All rights reserved.						*
*								*
*	This source code contains the intellectual property	*
*	of its copyright holder(s), and is made available	*
*	under a license.  If you do not know the terms of	*
*	the license, please stop and do not read further.	*
*								*
****************************************************************/

#include "mdef.h"

#include "op.h"
#include "numcmp.h"

LITREF	mval	literal_zero, literal_one;
LITREF	mval	literal_sqlnull;

void op_gt_retmval(mval *u, mval *v, mval *result)
{
	long	ret;

	if (MV_IS_SQLNULL(u))
	{
		MV_FORCE_DEFINED(v);
		*result = literal_sqlnull;
		return;
	}
	if (MV_IS_SQLNULL(v))
	{
		MV_FORCE_DEFINED(u);
		*result = literal_sqlnull;
		return;
	}
	/* We avoid a call to "numcmp()" but instead do a "NUMCMP_SKIP_SQLNULL_CHECK" macro call since we have already
	 * checked for "MV_IS_SQLNULL" above. Avoiding that and the "numcmp()" function call helps cut down a few instructions.
	 */
	NUMCMP_SKIP_SQLNULL_CHECK(u, v, ret);	/* sets "ret" to the result of the numeric comparison of "u" and "v" */
	*result = ((0 < ret) ? literal_one : literal_zero);
	return;
}
