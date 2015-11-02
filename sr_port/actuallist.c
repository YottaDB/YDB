/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
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

GBLREF char	window_token;
GBLREF mident	window_ident;

int actuallist (oprtype *opr)
{
	triple		*ref0, *ref1, *ref2, *masktrip, *counttrip;
	oprtype		ot;
	int		mask, parmcount;
	error_def	(ERR_MAXACTARG);
	error_def	(ERR_NAMEEXPECTED);
	error_def	(ERR_COMMAORRPAREXP);

	assert (window_token == TK_LPAREN);
	advancewindow ();
	masktrip = newtriple (OC_PARAMETER);
	mask = 0;
	counttrip = newtriple (OC_PARAMETER);
	masktrip->operand[1] = put_tref (counttrip);
	ref0 = counttrip;
	if (window_token == TK_RPAREN)
		parmcount = 0;
	else
	for (parmcount = 1; ; parmcount++)
	{
		if (parmcount > MAX_ACTUALS)
		{
			stx_error (ERR_MAXACTARG);
			return FALSE;
		}
		if (window_token == TK_PERIOD)
		{
			advancewindow ();
			if (window_token == TK_IDENT)
			{
				ot = put_mvar (&window_ident);
				mask |= (1 << parmcount - 1);
				advancewindow ();
			}
			else if (window_token == TK_ATSIGN)
			{
				if (!indirection(&ot))
					return FALSE;
				ref2 = newtriple(OC_INDLVNAMADR);
				ref2->operand[0] = ot;
				ot = put_tref(ref2);
				mask |= (1 << parmcount - 1);
			}
			else
			{
				stx_error (ERR_NAMEEXPECTED);
				return FALSE;
			}
		}
		else if (window_token == TK_COMMA)
		{
			ref2 = newtriple(OC_NULLEXP);
			ot = put_tref(ref2);
		}
		else
			if (!expr (&ot)) return FALSE;
		ref1 = newtriple (OC_PARAMETER);
		ref0->operand[1] = put_tref (ref1);
		ref1->operand[0] = ot;
		if (window_token == TK_COMMA)
		{	advancewindow ();
			if (window_token == TK_RPAREN)
			{	ref0 = ref1;
				ref2 = newtriple(OC_NULLEXP);
				ot = put_tref(ref2);
				ref1 = newtriple (OC_PARAMETER);
				ref0->operand[1] = put_tref (ref1);
				ref1->operand[0] = ot;
				parmcount++;
				break;
			}
		}
		else
		if (window_token == TK_RPAREN)
			break;
		else
		{
			stx_error (ERR_COMMAORRPAREXP);
			return FALSE;
		}
		ref0 = ref1;
	}
	advancewindow ();
	masktrip->operand[0] = put_ilit (mask);
	counttrip->operand[0] = put_ilit (parmcount);
	parmcount += 2;
	*opr = put_tref (masktrip);
	return parmcount;
}
