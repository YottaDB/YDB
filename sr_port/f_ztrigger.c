/****************************************************************
 *								*
 *	Copyright 2010, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "toktyp.h"
#include "opcode.h"
#include "advancewindow.h"

LITREF mval	literal_null ;

int f_ztrigger(oprtype *a, opctype op)
{
	triple	*r, *arg1, *arg2;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	arg1 = newtriple(OC_PARAMETER);
	arg2 = newtriple(OC_PARAMETER);
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
		return FALSE;
	if (TK_COMMA == TREF(window_token))
	{	/* Looking for a 2nd argument */
		advancewindow();
		if (EXPR_FAIL == expr(&(arg1->operand[0]), MUMPS_STR))
			return FALSE;
		if (TK_COMMA == TREF(window_token))
		{
			advancewindow();
			if (EXPR_FAIL == expr(&(arg2->operand[0]), MUMPS_STR))
				return FALSE;
		} else
			arg2->operand[0] = put_lit((mval *)&literal_null);
	} else
	{
		arg1->operand[0] = put_lit((mval *)&literal_null);
		arg2->operand[0] = put_lit((mval *)&literal_null);
	}
	r->operand[1] = put_tref(arg1);
	arg1->operand[1] = put_tref(arg2);
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
