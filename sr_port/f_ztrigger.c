/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
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

GBLREF char	window_token;
LITREF mval	literal_null ;

int f_ztrigger(oprtype *a, opctype op)
{
	triple	*r, *arg1, *arg2;

	r = maketriple(op);
	arg1 = newtriple(OC_PARAMETER);
	arg2 = newtriple(OC_PARAMETER);
	if (!strexpr(&(r->operand[0])))
		return FALSE;
	if (TK_COMMA == window_token)
	{	/* Looking for a 2nd argument */
		advancewindow();
		if (!strexpr(&(arg1->operand[0])))
			return FALSE;
		if (TK_COMMA == window_token)
		{
			advancewindow();
			if (!strexpr(&(arg2->operand[0])))
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
