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
#include "mdq.h"
#include "advancewindow.h"

error_def(ERR_COMMA);

int f_fnumber(oprtype *a, opctype op)
{
	triple *ref, *next, *r;
	oprtype z;
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
	if (EXPR_FAIL == expr(&r->operand[1], MUMPS_STR))
		return FALSE;
	if (TK_COMMA != TREF(window_token))
	{
		ref = newtriple(OC_FORCENUM);
		ref->operand[0] = r->operand[0];
		r->operand[0] = put_tref(ref);
	} else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&z, MUMPS_INT))
			return FALSE;
		ref = newtriple(OC_FNJ3);
		ref->operand[0] = r->operand[0];
		r->operand[0] = put_tref(ref);
		next = newtriple(OC_PARAMETER);
		ref->operand[1] = put_tref(next);
		next->operand[0] = put_ilit((mint) 0);
		next->operand[1] = z;
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
