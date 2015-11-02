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
#include "advancewindow.h"
#include "compile_pattern.h"

error_def(ERR_EXPR);
error_def(ERR_RHMISSING);

LITREF octabstruct oc_tab[];
LITREF toktabtype tokentable[];

int eval_expr(oprtype *a)
{
	triple *ref, *parm, *ref1;
	int op_count;
	oprtype x1, x2;
	opctype i;
	unsigned short type;
	boolean_t ind_pat;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!expratom(&x1))
	{	/* If didn't already add an error of our own, do so now with catch all expression error */
		if (OC_RTERROR != (TREF(curtchain))->exorder.bl->exorder.bl->exorder.bl->opcode)
			stx_error(ERR_EXPR);
		return EXPR_FAIL;
	}
	while (i = tokentable[TREF(window_token)].bo_type)	/* NOTE assignment NOT condition */
	{
		type = tokentable[TREF(window_token)].opr_type;
		if (oc_tab[i].octype & OCT_BOOL)
		{
			if (!TREF(shift_side_effects))
			{
				assert(FALSE == TREF(saw_side_effect));
				for (ref = (TREF(curtchain))->exorder.bl; oc_tab[ref->opcode].octype & OCT_BOOL;
					ref = ref->exorder.bl)
						;
				TREF(expr_start) = TREF(expr_start_orig) = ref;
			}
			switch (i)
			{
			case OC_NAND:
			case OC_AND:
			case OC_NOR:
			case OC_OR:
				TREF(shift_side_effects) = TRUE;
			default:
				break;
			}
		}
		coerce(&x1, type);
		if (OC_CAT == i)
		{
			ref1 = ref = maketriple(OC_CAT);
			for (op_count = 2; ; op_count++) /* op_count = first operand plus destination */
			{
				parm = newtriple(OC_PARAMETER);
				ref1->operand[1] = put_tref(parm);
				ref1 = parm;
				ref1->operand[0] = x1;
				if (TK_UNDERSCORE != TREF(window_token))
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
				coerce(&x1, type);
			}
		} else
		{
			if ((TK_QUESTION == TREF(window_token)) || (TK_NQUESTION == TREF(window_token)))
			{
				ind_pat = FALSE;
				if (TK_ATSIGN == TREF(director_token))
				{
					ind_pat = TRUE;
					advancewindow();
				}
				if (!compile_pattern(&x2, ind_pat))
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
			coerce(&x2, type);
			ref = newtriple(i);
			ref->operand[0] = x1;
			ref->operand[1] = x2;
		}
		x1 = put_tref(ref);
	}
	*a = x1;
	return (OC_INDGLVN == (TREF(curtchain))->exorder.bl->opcode) ? EXPR_INDR : EXPR_GOOD;
}
