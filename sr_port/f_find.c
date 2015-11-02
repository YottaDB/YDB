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

error_def(ERR_COMMA);

int f_find(oprtype *a, opctype op)
{
	triple *delimiter, *r, *start;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
		return FALSE;
	if (TK_COMMA != TREF(window_token))
	{
		stx_error(ERR_COMMA);
		return FALSE;
	}
	advancewindow();
	delimiter = newtriple(OC_PARAMETER);
	start = newtriple(OC_PARAMETER);
	r->operand[1] = put_tref(delimiter);
	delimiter->operand[1] = put_tref(start);
	if (EXPR_FAIL == expr(&(delimiter->operand[0]), MUMPS_STR))
		return FALSE;
	if (TK_COMMA != TREF(window_token))
		start->operand[0] = put_ilit(1);
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(start->operand[0]), MUMPS_INT))
			return FALSE;
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
