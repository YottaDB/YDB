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
#include "gtm_string.h"

LITREF	mval	literal_zero, literal_one;
LITREF	mval	literal_sqlnull;

void op_follow_retmval(mval *u, mval *v, mval *ret)
{
	boolean_t	result;

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
	MV_FORCE_STR(u);
	MV_FORCE_STR(v);
	/* Use MEMVCMP macro and not "memvcmp()" function to avoid overhead of function call */
	MEMVCMP(u->str.addr, u->str.len, v->str.addr, v->str.len, result);
	*ret = ((0 < result) ? literal_one : literal_zero);
	return;
}
