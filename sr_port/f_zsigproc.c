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

int f_zsigproc(oprtype *a, opctype op)
{
	triple *r;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	/* First argument is integer process id */
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_INT))
		return FALSE;	/* Improper process id argument */
	if (TK_COMMA != TREF(window_token))
	{	/* 2nd argument (for now) required */
		stx_error(ERR_COMMA);
		return FALSE;
	}
	advancewindow();
	/* 2nd argument is the signal number to send */
	if (EXPR_FAIL == expr(&(r->operand[1]), MUMPS_INT))
		return FALSE;	/* Improper signal number argument */
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
