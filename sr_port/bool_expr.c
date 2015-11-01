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

int bool_expr(bool op,oprtype *addr)
{
	oprtype x;

	if (!expr_depth++)
		expr_start = expr_start_orig = 0;
	if (!eval_expr(&x))
	{
		expr_depth = 0;
		return FALSE;
	}
	coerce(&x, OCT_BOOL);
	if (!--expr_depth)
		shift_gvrefs = FALSE;
	assert(x.oprclass == TRIP_REF);
	bx_tail(x.oprval.tref, op, addr);
	return TRUE;
}
