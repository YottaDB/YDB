/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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

/* Note this function compiler routine is used for all of $EXTRACT,
   $ZEXTRACT, and $ZSUBSTR since the format of the call is identical
   in all instances as the function is similar.
*/
int f_extract(oprtype *a, opctype op)
{
	triple *first, *last, *r;

	r = maketriple(op);
	if (!strexpr(&(r->operand[0])))
		return FALSE;
	first = newtriple(OC_PARAMETER);
	last = newtriple(OC_PARAMETER);
	r->operand[1] = put_tref(first);
	first->operand[1] = put_tref(last);
	if (window_token != TK_COMMA)
	{
		first->operand[0] = put_ilit(1);
		last->operand[0] = put_ilit((OC_FNZSUBSTR == op) ? MAXPOSINT4 : 1);
	} else
	{
		advancewindow();
		if (!intexpr(&(first->operand[0])))
			return FALSE;
		if (window_token != TK_COMMA)
			last->operand[0] = (OC_FNZSUBSTR == op) ? put_ilit(MAXPOSINT4) : first->operand[0];
		else
		{
			advancewindow();
			if (!intexpr(&(last->operand[0])))
				return FALSE;
		}
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
