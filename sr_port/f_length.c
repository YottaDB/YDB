/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
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

int f_length( oprtype *a, opctype op)
{
	triple *r;

	assert((OC_FNLENGTH == op) || (OC_FNZLENGTH == op));
	r = maketriple(op);
	if (!strexpr(&(r->operand[0])))
		return FALSE;
	if (window_token == TK_COMMA)
	{
		advancewindow();
		if (OC_FNLENGTH == op)
			r->opcode = OC_FNPOPULATION;      /* This isn't very go information hiding */
		else
			r->opcode = OC_FNZPOPULATION;      /* This isn't very go information hiding */
		if (!strexpr(&(r->operand[1])))
			return FALSE;
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
