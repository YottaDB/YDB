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

int f_zjobexam(oprtype *a, opctype op)
{
	triple *r;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	if (TK_RPAREN == TREF(window_token))
	{	/* No argument specified - default to null */
		r->operand[0] = put_str("",0);
	} else if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
		return FALSE;	/* Improper string argument */
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
