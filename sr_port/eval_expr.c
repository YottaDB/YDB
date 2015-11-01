/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include "compile_pattern.h"

GBLDEF triple *expr_start, *expr_start_orig;
GBLDEF bool shift_gvrefs;

LITREF toktabtype tokentable[];
GBLREF char window_token, director_token;
GBLREF triple *curtchain;
LITREF octabstruct oc_tab[];

int eval_expr(oprtype *a)
{
	triple *ref, *parm, *ref1;
	int op_count;
	oprtype x1, x2;
	opctype i;
	unsigned short type;
	bool ind_pat;
	error_def(ERR_RHMISSING);
	error_def(ERR_EXPR);

	if (!expratom(&x1))
	{	/* If didn't already add an error of our own, do so now with catch all expression error */
		if (curtchain->exorder.bl->exorder.bl->exorder.bl->opcode != OC_RTERROR)
			stx_error(ERR_EXPR);
		return EXPR_FAIL;
	}
	while (i = tokentable[window_token].bo_type)
	{
		type = tokentable[window_token].opr_type;
		if (oc_tab[i].octype & OCT_BOOL && !shift_gvrefs)
		{
			shift_gvrefs = TRUE;
			for (ref = curtchain->exorder.bl; oc_tab[ref->opcode].octype & OCT_BOOL ; ref = ref->exorder.bl)
				;
			expr_start = expr_start_orig = ref;
		}
		coerce(&x1,type);
		if (i == OC_CAT)
		{
			ref1 = ref = maketriple(OC_CAT);
			for (op_count = 2 ;; op_count++) /* op_count = first operand plus destination */
			{
				parm = newtriple(OC_PARAMETER);
				ref1->operand[1] = put_tref(parm);
				ref1 = parm;
				ref1->operand[0] = x1;
				if (window_token != TK_UNDERSCORE)
				{
					assert(op_count > 1);
					ref->operand[0] = put_ilit(op_count);
					ins_triple(ref);
					break;
				}
				advancewindow();
				if (!expratom(&x1))
				{
					stx_error(ERR_RHMISSING);
					return EXPR_FAIL;
				}
				coerce(&x1,type);
			}
		} else
		{
			if (window_token == TK_QUESTION || window_token == TK_NQUESTION)
			{
				ind_pat = FALSE;
				if (director_token == TK_ATSIGN)
				{
					ind_pat = TRUE;
					advancewindow();
				}
				if (!compile_pattern(&x2,ind_pat))
					return EXPR_FAIL;
			} else
			{
				advancewindow();
				if (!expratom(&x2))
				{
					stx_error(ERR_RHMISSING);
					return EXPR_FAIL;
				}
			}
			coerce(&x2,type);
			ref = newtriple(i);
			ref->operand[0] = x1;
			ref->operand[1] = x2;
		}
		x1 = put_tref(ref);
	}
	*a = x1;
	return (curtchain->exorder.bl->opcode == OC_INDGLVN) ? EXPR_INDR : EXPR_GOOD;
}
