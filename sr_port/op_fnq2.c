/****************************************************************
 *								*
 * Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Code in this module is based on op_fno2.c and hence has an
 * FIS copyright even though this module was not created by FIS.
 */

#include "mdef.h"

#include "lv_val.h"
#include "mvalconv.h"
#include "op.h"

error_def(ERR_QUERY2);

LITREF	mval	literal_one;
LITREF	mval	literal_minusone;

/* This function is basically a 2-argument $query(lvn,dir) call where the first argument is a lvn
 * and the 2nd argument dir is not a literal constant (so direction is not known at compile time in "f_query").
 * In this case, "f_query" generates an OC_FNQ2 opcode that invokes "op_fnq2" with the direction parameter evaluated
 * and so we can now decide whether to go with forward or reverse query of lvn.
 */
void op_fnq2(int sbscnt, mval *dst, mval *direct, ...)
{
	int4		dummy_intval;
	va_list		var;

	MV_FORCE_NUM(direct);
	if (!MV_IS_TRUEINT(direct, &dummy_intval)
			|| ((literal_one.m[1] != direct->m[1]) && (literal_minusone.m[1] != direct->m[1])))
		rts_error(VARLSTCNT(1) ERR_QUERY2);
	else
	{
		VAR_START(var, direct);
		if (literal_one.m[1] == direct->m[1])
			op_fnquery_va(sbscnt, dst, var);
		else
			op_fnreversequery_va(sbscnt, dst, var);
	}
}
