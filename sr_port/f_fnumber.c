/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "mdq.h"
#include "advancewindow.h"

error_def(ERR_COMMA);

int f_fnumber(oprtype *a, opctype op)
{
	triple	*r, *ref, *ref1;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	if (EXPR_FAIL == expr(&r->operand[0], MUMPS_NUM))
		return FALSE;
	if (TK_COMMA != TREF(window_token))
	{
		stx_error(ERR_COMMA);
		return FALSE;
	}
	advancewindow();
	ref = newtriple(OC_PARAMETER);
	r->operand[1] = put_tref(ref);
	if (EXPR_FAIL == expr(&ref->operand[0], MUMPS_STR))
		return FALSE;
	ref1 = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(ref1);
	if (TK_COMMA == TREF(window_token))
	{
		advancewindow();
		if (EXPR_FAIL == expr(&ref1->operand[1], MUMPS_INT))
			return FALSE;
		ref1->operand[0] = put_ilit((mint)(1));				/* flag that the 3rd argument is real */
	} else
		ref1->operand[0] = ref1->operand[1] = put_ilit((mint)0);	/* flag no 3rd argument and give it default value */
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
