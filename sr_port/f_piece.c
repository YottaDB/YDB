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

int f_piece( oprtype *a, opctype op)
{
	triple *delimiter, *first, *last, *r, *srcislit;
	oprtype x;
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
	r->operand[1] = put_tref(delimiter);
	first = newtriple(OC_PARAMETER);
	delimiter ->operand[1] = put_tref(first);
	if (!strexpr(&x))
		return FALSE;
	if (window_token != TK_COMMA)
		first->operand[0] = put_ilit(1);
	else
	{
		advancewindow();
		if (!intexpr(&(first->operand[0])))
		return FALSE;
	}
	assert(x.oprclass == TRIP_REF);
	if (window_token != TK_COMMA && x.oprval.tref->opcode == OC_LIT &&
		x.oprval.tref->operand[0].oprval.mlit->v.str.len == 1)
	{
		r->opcode = OC_FNP1;
		delimiter->operand[0] =
		put_ilit((uint4) *x.oprval.tref->operand[0].oprval.mlit->v.str.addr);
		srcislit = newtriple(OC_PARAMETER);
		first->operand[1] = put_tref(srcislit);
	} else
	{
		delimiter->operand[0] = x;
		last = newtriple(OC_PARAMETER);
		first->operand[1] = put_tref(last);
		if (window_token != TK_COMMA)
			last->operand[0] = first->operand[0];
		else
		{
			advancewindow();
			if (!intexpr(&(last->operand[0])))
				return FALSE;
		}
		srcislit = newtriple(OC_PARAMETER);
		last->operand[1] = put_tref(srcislit);
	}
	/* Pass value 1 (TRUE) if src string is a literal, else 0 (FALSE) */
	srcislit->operand[0] = put_ilit((OC_LIT == r->operand[0].oprval.tref->opcode) ? TRUE : FALSE);
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
