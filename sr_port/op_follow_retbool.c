/****************************************************************
 *								*
 * Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "op.h"
#include "gtm_string.h"

/* Note: This C function is called by "opp_follow_retbool.s". */
int	op_follow_retbool(mval *u, mval *v)
{
	boolean_t	result;

	if (MV_IS_SQLNULL(u))
	{
		MV_FORCE_DEFINED(v);
		return FALSE;
	}
	if (MV_IS_SQLNULL(v))
	{
		MV_FORCE_DEFINED(u);
		return FALSE;
	}
	/* The below code is similar to that in "bxrelop_operator.c" (for OC_FOLLOW case) */
	MV_FORCE_STR(u);
	MV_FORCE_STR(v);
	/* Use MEMVCMP macro and not "memvcmp()" function to avoid overhead of function call */
	MEMVCMP(u->str.addr, u->str.len, v->str.addr, v->str.len, result);
	return (0 < result);
}
