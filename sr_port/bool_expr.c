/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

int bool_expr(boolean_t op, oprtype *addr)
{
	oprtype x;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!(TREF(expr_depth))++)
		TREF(expr_start) = TREF(expr_start_orig) = NULL;
	if (!eval_expr(&x))
	{
		TREF(expr_depth) = 0;
		return FALSE;
	}
	assert(TRIP_REF == x.oprclass);
	coerce(&x, OCT_BOOL);
	bx_tail(x.oprval.tref, op, addr);
	if (!(--(TREF(expr_depth))))
		TREF(saw_side_effect) = TREF(shift_side_effects) = FALSE;
	return TRUE;
}
