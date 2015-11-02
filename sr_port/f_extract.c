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

/* $EXTRACT, $ZEXTRACT, and $ZSUBSTR use this compiler routine as all have similar function and identical invocation signatures */
int f_extract(oprtype *a, opctype op)
{
	triple *first, *last, *r;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
		return FALSE;
	first = newtriple(OC_PARAMETER);
	last = newtriple(OC_PARAMETER);
	r->operand[1] = put_tref(first);
	first->operand[1] = put_tref(last);
	if (TK_COMMA != TREF(window_token))
	{
		first->operand[0] = put_ilit(1);
		last->operand[0] = put_ilit((OC_FNZSUBSTR == op) ? MAXPOSINT4 : 1);
	} else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(first->operand[0]), MUMPS_INT))
			return FALSE;
		if (TK_COMMA != TREF(window_token))
			last->operand[0] = (OC_FNZSUBSTR == op) ? put_ilit(MAXPOSINT4) : first->operand[0];
		else
		{
			advancewindow();
			if (EXPR_FAIL == expr(&(last->operand[0]), MUMPS_INT))
				return FALSE;
		}
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
