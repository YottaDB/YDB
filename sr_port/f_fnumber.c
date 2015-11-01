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
#include "mdq.h"
#include "advancewindow.h"

GBLREF char window_token;

int f_fnumber( oprtype *a, opctype op)
{
	triple *ref, *next, *r;
	oprtype z;
	error_def(ERR_COMMA);

	r = maketriple(op);
	if (!numexpr(&r->operand[0]))
		return FALSE;
	if (window_token != TK_COMMA)
	{
		stx_error(ERR_COMMA);
		return FALSE;
	}
	advancewindow();
	if (!strexpr(&r->operand[1]))
		return FALSE;
	if (window_token != TK_COMMA)
	{
		ref = newtriple(OC_FORCENUM);
		ref->operand[0] = r->operand[0];
		r->operand[0] = put_tref(ref);
	} else
	{
		advancewindow();
		if (!intexpr(&z))
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
