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

#include "mmemory.h"
#include "op.h"

LITREF	mval	literal_zero, literal_one;
LITREF	mval	literal_sqlnull;

void op_follow_retmval(mval *u, mval *v, mval *ret)
{
	int		utyp, vtyp;
	boolean_t	result;

	utyp = u->mvtype;
	vtyp = v->mvtype;
	if (MVTYPE_IS_SQLNULL(utyp))
	{
		MV_FORCE_DEFINED(v);
		*ret = literal_sqlnull;
		return;
	}
	if (MVTYPE_IS_SQLNULL(vtyp))
	{
		MV_FORCE_DEFINED(u);
		*ret = literal_sqlnull;
		return;
	}
	/* The below code is similar to that in "bxrelop_operator.c" (for OC_FOLLOW case) */
	MV_FORCE_STR(u);
	MV_FORCE_STR(v);
	result = memvcmp(u->str.addr, u->str.len, v->str.addr, v->str.len);
	*ret = ((0 < result) ? literal_one : literal_zero);
	return;
}
