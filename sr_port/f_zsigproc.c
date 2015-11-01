/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

GBLREF char window_token;

int f_zsigproc(oprtype *a, opctype op)
{
	triple *r;
	error_def(ERR_COMMA);

	r = maketriple(op);
	/* First argument is integer process id */
	if (!intexpr(&(r->operand[0])))
		return FALSE;	/* Improper process id argument */
	if (window_token != TK_COMMA)
	{	/* 2nd argument (for now) required */
		stx_error(ERR_COMMA);
		return FALSE;
	}
	advancewindow();
	/* 2nd argument is the signal number to send */
	if (!intexpr(&(r->operand[1])))
		return FALSE;	/* Improper signal number argument */
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
