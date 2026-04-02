/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

/*
 * Process parameters.  If we have a consumer for a 64 bit parameter
 * mask, provide it as two triples, one with the low 32 bits, one with the
 * high 32 bits.
 *
 * If we do not have such a consumer, provide the historical 32 bit mask triple
 */
int actuallist (oprtype *opr, boolean_t do_mask64)
{
	boolean_t	se_warn;
	int		i, j;
	gtm_uint8	parmcount, mask;
	uint4		masklo, maskhi;
	oprtype		ot;
	triple		*counttrip, *ref0, *ref1, *ref2;
	triple		*maskhitrip, *masklotrip;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TK_LPAREN == TREF(window_token));
	advancewindow();

	counttrip = newtriple(OC_PARAMETER);
	masklotrip = newtriple(OC_PARAMETER);
	maskhitrip = NULL;
	mask = 0;

	if (do_mask64)
	{
		maskhitrip = newtriple(OC_PARAMETER);
		maskhitrip->operand[1] = put_tref(masklotrip);
	}
	masklotrip->operand[1] = put_tref(counttrip);

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
					mask |= (1ul << (parmcount - 1));
					advancewindow();
				} else if (TK_ATSIGN == TREF(window_token))
				{
					if (!indirection(&ot))
						return FALSE;
					ref2 = newtriple(OC_INDLVNAMADR);
					ref2->operand[0] = ot;
					ot = put_tref(ref2);
					mask |= (1ul << (parmcount - 1));
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
			se_warn = SE_WARN_ON;
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

	maskhi = (uint4) (mask >> 32);
	masklo = (uint4) (mask & 0x00000000ffffffff);

	if (do_mask64)
	{
		maskhitrip->operand[0] = put_ilit((uint4) maskhi);
	}
	masklotrip->operand[0] = put_ilit((uint4) masklo);

	counttrip->operand[0] = put_ilit(parmcount);
	if (do_mask64)
	{
		parmcount += 3;
		*opr = put_tref(maskhitrip);
	}
	else
	{
		parmcount += 2;
		*opr = put_tref(masklotrip);
	}
	return parmcount;
}
