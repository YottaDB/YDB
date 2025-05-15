/****************************************************************
 *								*
 * Copyright (c) 2024-2025 YottaDB LLC and/or its subsidiaries.	*
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
#include "copy.h"
#include "patcode.h"

/* Note: This C function is called by "opp_npattern_retbool.s". */
int	op_npattern_retbool(mval *u, mval *v)
{
	boolean_t	result;
	uint4		tempuint;

	if (MV_IS_SQLNULL(u))
	{
		assert(MV_DEFINED(v));
		return FALSE;
	}
	assert(!MV_IS_SQLNULL(v));
	assert(MV_IS_STRING(v));
	GET_ULONG(tempuint, v->str.addr);
	result = (tempuint ? do_patfixed(u, v) : do_pattern(u, v));
	return !result;
}
