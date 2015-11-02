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
#include "subscript.h"
#include "advancewindow.h"

GBLREF char window_token;

int indirection(oprtype *a)
{
	triple		*ref,*next;
	oprtype		subs[MAX_INDSUBSCRIPTS];
	oprtype		x,*sb1,*sb2;
	char		c;
	error_def(ERR_MAXNRSUBSCRIPTS);
	error_def(ERR_RPARENMISSING);
	error_def(ERR_LPARENMISSING);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(window_token == TK_ATSIGN);
	if (!(TREF(expr_depth))++)
		TREF(expr_start) = TREF(expr_start_orig) = NULL;
	advancewindow();
	if (!expratom(a))
	{
		TREF(expr_depth) = 0;
		return FALSE;
	}
	coerce(a,OCT_MVAL);
	ex_tail(a);
	if (!(--(TREF(expr_depth))))
		TREF(shift_side_effects) = FALSE;
	if (window_token == TK_ATSIGN)
	{
		advancewindow();
		if (window_token != TK_LPAREN)
		{
			stx_error(ERR_LPARENMISSING);
			return FALSE;
		}
		ref = maketriple(OC_INDNAME);
		sb1 = sb2 = subs;
		for (;;)
		{
			if (sb1 >= ARRAYTOP(subs))
			{
				stx_error(ERR_MAXNRSUBSCRIPTS);
				return FALSE;
			}
			advancewindow();
			if (!expr(sb1++))
				return FALSE;
			if ((c = window_token) == TK_RPAREN)
			{
				advancewindow();
				break;
			}
			if (c != TK_COMMA)
			{
				stx_error(ERR_RPARENMISSING);
				return FALSE;
			}
		}
		/* store argument count...n args plus the name plus the dst*/
		ref->operand[0] = put_ilit((mint)(sb1 - sb2) + 2);
		ins_triple(ref);
		next = newtriple(OC_PARAMETER);
		next->operand[0] = *a;
		ref->operand[1] = put_tref(next);
		*a = put_tref(ref);
		while (sb2 < sb1)
		{
			ref = newtriple(OC_PARAMETER);
			next->operand[1] = put_tref(ref);
			ref->operand[0] = *sb2++;
			next = ref;
		}
	}
	return TRUE;

}
