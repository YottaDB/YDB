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

#include "matchc.h"
#include "op.h"

/* Note: This C function is called by "opp_ncontain_retbool.s". */
int	op_ncontain_retbool(mval *u, mval *v)
{
	int		numpcs;
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
	/* The below code is similar to that in "bxrelop_operator.c" (for OC_NCONTAIN case) */
	MV_FORCE_STR(u);
	MV_FORCE_STR(v);
	numpcs = 1;
	matchc(v->str.len, (uchar_ptr_t)v->str.addr, u->str.len, (uchar_ptr_t)u->str.addr, &result, &numpcs);
	return !result;
}
