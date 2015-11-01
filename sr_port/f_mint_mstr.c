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

int f_mint_mstr( oprtype *a, opctype op)
{
	triple *r;
	error_def(ERR_COMMA);

	r = maketriple(op);
	if (!intexpr(&(r->operand[0])))
		return FALSE;
	if (window_token != TK_COMMA)
	{
		stx_error(ERR_COMMA);
		return FALSE;
	}
	advancewindow();
	if (!strexpr(&r->operand[1]))
	{	return FALSE;
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
