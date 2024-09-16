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

#include "mmemory.h"
#include "op.h"

/* Note: This C function is called by "opp_nfollow_retbool.s". */
int	op_nfollow_retbool(mval *u, mval *v)
{
	int		utyp, vtyp;
	boolean_t	result;

	utyp = u->mvtype;
	vtyp = v->mvtype;
	if (MVTYPE_IS_SQLNULL(utyp))
	{
		MV_FORCE_DEFINED(v);
		return FALSE;
	}
	if (MVTYPE_IS_SQLNULL(vtyp))
	{
		MV_FORCE_DEFINED(u);
		return FALSE;
	}
	/* The below code is similar to that in "bxrelop_operator.c" (for OC_NFOLLOW case) */
	MV_FORCE_STR(u);
	MV_FORCE_STR(v);
	result = memvcmp(u->str.addr, u->str.len, v->str.addr, v->str.len);
	return (0 >= result);
}
