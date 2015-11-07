/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "fullbool.h"
#include "advancewindow.h"
#include "show_source_line.h"

GBLREF	boolean_t	run_time;

error_def(ERR_COMMAORRPAREXP);
error_def(ERR_MAXACTARG);
error_def(ERR_NAMEEXPECTED);
error_def(ERR_SIDEEFFECTEVAL);

	int actuallist (oprtype *opr)
{
	boolean_t	se_warn;
	int		i, j, mask, parmcount;
	oprtype		ot;
	triple		*counttrip, *masktrip, *ref0, *ref1, *ref2;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TK_LPAREN == TREF(window_token));
	advancewindow();
	masktrip = newtriple(OC_PARAMETER);
	mask = 0;
	counttrip = newtriple(OC_PARAMETER);
	masktrip->operand[1] = put_tref(counttrip);
	ref0 = counttrip;
	if (TK_RPAREN == TREF(window_token))
		parmcount = 0;
	else
	{
		for (parmcount = 1; ; parmcount++)
		{
			if (MAX_ACTUALS < parmcount)
			{
				stx_error (ERR_MAXACTARG);
				return FALSE;
			}
			if (TK_PERIOD == TREF(window_token))
			{
				advancewindow ();
				if (TK_IDENT == TREF(window_token))
				{
					ot = put_mvar(&(TREF(window_ident)));
					mask |= (1 << parmcount - 1);
					advancewindow();
				} else if (TK_ATSIGN == TREF(window_token))
				{
					if (!indirection(&ot))
						return FALSE;
					ref2 = newtriple(OC_INDLVNAMADR);
					ref2->operand[0] = ot;
					ot = put_tref(ref2);
					mask |= (1 << parmcount - 1);
				} else
				{
					stx_error(ERR_NAMEEXPECTED);
					return FALSE;
				}
			} else if (TK_COMMA == TREF(window_token))
			{
				ref2 = newtriple(OC_NULLEXP);
				ot = put_tref(ref2);
			} else if (EXPR_FAIL == expr(&ot, MUMPS_EXPR))
				return FALSE;
			ref1 = newtriple(OC_PARAMETER);
			ref0->operand[1] = put_tref(ref1);
			ref1->operand[0] = ot;
			if (TK_COMMA == TREF(window_token))
			{	advancewindow ();
				if (TK_RPAREN == TREF(window_token))
				{	ref0 = ref1;
					ref2 = newtriple(OC_NULLEXP);
					ot = put_tref(ref2);
					ref1 = newtriple(OC_PARAMETER);
					ref0->operand[1] = put_tref(ref1);
					ref1->operand[0] = ot;
					parmcount++;
					break;
				}
			} else if (TREF(window_token) == TK_RPAREN)
				break;
			else
			{
				stx_error (ERR_COMMAORRPAREXP);
				return FALSE;
			}
			ref0 = ref1;
		}
		if ((1 < parmcount) && (TREF(side_effect_base))[TREF(expr_depth)])
		{	/* at least two arguments and at least one side effect - look for lvns needing protection */
			assert(OLD_SE != TREF(side_effect_handling));
			se_warn = (!run_time && (SE_WARN == TREF(side_effect_handling)));
			for (i = 0, j = parmcount, ref0 = counttrip->operand[1].oprval.tref; --j;
				ref0 = ref0->operand[1].oprval.tref)
			{	/* no need to do the last argument - can't have a side effect after it */
				assert(OC_PARAMETER == ref0->opcode);
				assert((TRIP_REF == ref0->operand[0].oprclass) && (TRIP_REF == ref0->operand[1].oprclass));
				if (!((1 << i++) & mask) && (OC_VAR == ref0->operand[0].oprval.tref->opcode))
				{	/* can only protect pass-by-value (not pass-by-reference) */
					ref1 = maketriple(OC_STOTEMP);
					ref1->operand[0] = put_tref(ref0->operand[0].oprval.tref);
					ref0->operand[0].oprval.tref = ref1;
					dqins(ref0, exorder, ref1); 	/* NOTE:this violates information hiding */
					if (se_warn)
						ISSUE_SIDEEFFECTEVAL_WARNING(ref0->src.column);
				}
			}
			/* the following asserts check we're getting only TRIP_REF or empty operands */
			assert((NO_REF == ref0->operand[0].oprclass) || (TRIP_REF == ref0->operand[0].oprclass));
			assert(((NO_REF == ref0->operand[0].oprclass) ? TRIP_REF : NO_REF) == ref0->operand[1].oprclass);
		}
	}
	advancewindow();
	masktrip->operand[0] = put_ilit(mask);
	counttrip->operand[0] = put_ilit(parmcount);
	parmcount += 2;
	*opr = put_tref(masktrip);
	return parmcount;
}
