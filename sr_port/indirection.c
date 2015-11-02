/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

error_def(ERR_LPARENMISSING);
error_def(ERR_MAXNRSUBSCRIPTS);
error_def(ERR_RPARENMISSING);

int indirection(oprtype *a)
{
	char		c;
	oprtype		*sb1, *sb2, subs[MAX_INDSUBSCRIPTS], x;
	triple		*next, *ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TK_ATSIGN == TREF(window_token));
	if (!(TREF(expr_depth))++)
		TREF(expr_start) = TREF(expr_start_orig) = NULL;
	advancewindow();
	if (!expratom(a))
	{
		TREF(expr_depth) = 0;
		return FALSE;
	}
	coerce(a, OCT_MVAL);
	ex_tail(a);
	if (!(--(TREF(expr_depth))))
		TREF(shift_side_effects) = FALSE;
	TREF(saw_side_effect) = TREF(shift_side_effects);	/* TRUE or FALSE, at this point they're the same */
	if (TK_ATSIGN == TREF(window_token))
	{
		advancewindow();
		if (TK_LPAREN != TREF(window_token))
		{
			stx_error(ERR_LPARENMISSING);
			return FALSE;
		}
		ref = maketriple(OC_INDNAME);
		sb1 = sb2 = subs;
		for (;;)
		{
			if (ARRAYTOP(subs) <= sb1)
			{
				stx_error(ERR_MAXNRSUBSCRIPTS);
				return FALSE;
			}
			advancewindow();
			if (EXPR_FAIL == expr(sb1++, MUMPS_EXPR))
				return FALSE;
			if (TK_RPAREN == (c = TREF(window_token)))	/* NOTE assignment */
			{
				advancewindow();
				break;
			}
			if (TK_COMMA != c)
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
