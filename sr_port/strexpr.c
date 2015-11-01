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
#include "compiler.h"
#include "opcode.h"

GBLREF unsigned short int expr_depth;
GBLREF triple *expr_start, *expr_start_orig;
GBLREF bool shift_gvrefs;

int strexpr(oprtype *a)
{

	triple *triptr;
	int4 rval;

	if (!expr_depth++)
		expr_start = expr_start_orig = 0;
	if (!(rval = eval_expr(a)))
	{
		expr_depth = 0;
		return FALSE;
	}
	coerce(a,OCT_MVAL);
	ex_tail(a);
	if (!--expr_depth)
		shift_gvrefs = FALSE;
	if (expr_start != expr_start_orig)
	{
		triptr = newtriple(OC_GVRECTARG);
		triptr->operand[0] = put_tref(expr_start);
	}
	return rval;

}
