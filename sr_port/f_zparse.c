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
#include "toktyp.h"
#include "advancewindow.h"

int f_zparse(oprtype *a, opctype op)
{
	boolean_t	again;
	int		i;
	triple		*last, *r, *ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	last = r = maketriple(op);
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
		return FALSE;
	again = TRUE;
	for (i = 0; i < 4 ;i++)
	{
		ref = newtriple(OC_PARAMETER);
		last->operand[1] = put_tref(ref);
		if (again && TK_COMMA == TREF(window_token))
		{
			advancewindow();
			if (TK_COMMA == TREF(window_token))
				ref->operand[0] = put_str("", 0);
			else if (EXPR_FAIL == expr(&ref->operand[0], MUMPS_STR))
				return FALSE;
		} else
		{
			again = FALSE;
			ref->operand[0] = put_str("", 0);
		}
		last = ref;
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
