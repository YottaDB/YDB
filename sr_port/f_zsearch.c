/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
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

int f_zsearch(oprtype *a, opctype op)
{
	triple *r, *rop;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
		return FALSE;
	rop = newtriple(OC_PARAMETER);
	r->operand[1] = put_tref(rop);
	if (TK_COMMA != TREF(window_token))
		rop->operand[0] = put_ilit(0);
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(rop->operand[0]), MUMPS_INT))
			return FALSE;
	}
	rop->operand[1] = put_ilit(1);		/* This is an M-function call */
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
