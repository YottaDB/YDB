/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

int f_find(oprtype *a, opctype op)
{
	triple *delimiter, *start, *r;
	error_def(ERR_COMMA);

	r = maketriple(op);
	if (!strexpr(&(r->operand[0])))
		return FALSE;
	if (window_token != TK_COMMA)
	{
		stx_error(ERR_COMMA);
		return FALSE;
	}
	advancewindow();
	delimiter = newtriple(OC_PARAMETER);
	start = newtriple(OC_PARAMETER);
	r->operand[1] = put_tref(delimiter);
	delimiter->operand[1] = put_tref(start);
	if (!strexpr(&(delimiter->operand[0])))
		return FALSE;
	if (window_token != TK_COMMA)
		start->operand[0] = put_ilit(1);
	else
	{
		advancewindow();
		if (!intexpr(&(start->operand[0])))
			return FALSE;
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
