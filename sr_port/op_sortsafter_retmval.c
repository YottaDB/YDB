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
#include "sorts_after.h"

LITREF	mval	literal_zero, literal_one;
LITREF	mval	literal_sqlnull;

void op_sortsafter_retmval(mval *u, mval *v, mval *ret)
{
	boolean_t	result;
	uint4		tempuint;

	if (MV_IS_SQLNULL(u))
	{
		MV_FORCE_DEFINED(v);
		*ret = literal_sqlnull;
		return;
	}
	if (MV_IS_SQLNULL(v))
	{
		MV_FORCE_DEFINED(u);
		*ret = literal_sqlnull;
		return;
	}
	/* The below code is similar to that in "bxrelop_operator.c" (for OC_SORTSAFTER case) */
	result = sorts_after(u, v);
	*ret = ((0 < result) ? literal_one : literal_zero);
	return;
}
