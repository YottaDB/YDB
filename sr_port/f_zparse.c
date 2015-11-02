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

int f_zparse( oprtype *a, opctype op)
{
	triple *ref, *last, *r;
	bool again;
	int i;

	last = r = maketriple(op);
	if (!strexpr(&(r->operand[0])))
		return FALSE;
	again = TRUE;
	for(i = 0; i < 4 ;i++)
	{
		ref = newtriple(OC_PARAMETER);
		last->operand[1] = put_tref(ref);
		if(again && window_token == TK_COMMA)
		{
			advancewindow();
			if (window_token == TK_COMMA)
			{	ref->operand[0] = put_str("",0);
			}
			else if(!strexpr(&ref->operand[0]))
				return FALSE;
		}
		else
		{
			again = FALSE;
			ref->operand[0] = put_str("",0);
		}
		last = ref;
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
