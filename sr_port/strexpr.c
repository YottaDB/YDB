/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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

int strexpr(oprtype *a)
{

	triple *triptr;
	int4 rval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!(TREF(expr_depth))++)
		TREF(expr_start) = TREF(expr_start_orig) = NULL;
	if (!(rval = eval_expr(a)))
	{
		TREF(expr_depth) = 0;
		return FALSE;
	}
	coerce(a,OCT_MVAL);
	ex_tail(a);
	if (!(--(TREF(expr_depth))))
		TREF(shift_side_effects) = FALSE;
	if (TREF(expr_start) != TREF(expr_start_orig))
	{
		triptr = newtriple(OC_GVRECTARG);
		triptr->operand[0] = put_tref(TREF(expr_start));
	}
	return rval;

}
