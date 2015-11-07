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
#include "mdq.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "compile_pattern.h"
#include "fullbool.h"
#include "show_source_line.h"

GBLREF	boolean_t	run_time;

error_def(ERR_EXPR);
error_def(ERR_RHMISSING);
error_def(ERR_SIDEEFFECTEVAL);

LITREF octabstruct oc_tab[];
LITREF toktabtype tokentable[];

int eval_expr(oprtype *a)
{
	boolean_t	ind_pat, saw_local, saw_se, se_warn;
	int		op_count, se_handling;
	opctype		bin_opcode;
	oprtype		optyp_1, optyp_2, *optyp_ptr;
	tbp		*catbp, *tripbp;
	triple 		*argtrip, *parm, *ref, *ref1, *t1, *t2;
	unsigned short	type;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!expratom(&optyp_1))
	{	/* If didn't already add an error of our own, do so now with catch all expression error */
		if (OC_RTERROR != (TREF(curtchain))->exorder.bl->exorder.bl->exorder.bl->opcode)
			stx_error(ERR_EXPR);
		return EXPR_FAIL;
	}
	se_handling = TREF(side_effect_handling);
	se_warn = (!run_time && (SE_WARN == se_handling));
	while (bin_opcode = tokentable[TREF(window_token)].bo_type)	/* NOTE assignment NOT condition */
	{
		type = tokentable[TREF(window_token)].opr_type;
		if (oc_tab[bin_opcode].octype & OCT_BOOL)
		{
			if (!TREF(shift_side_effects))
			{
				assert(FALSE == TREF(saw_side_effect));
				for (ref = (TREF(curtchain))->exorder.bl; oc_tab[ref->opcode].octype & OCT_BOOL;
					ref = ref->exorder.bl)
						;
				TREF(expr_start) = TREF(expr_start_orig) = ref;
			}
			switch (bin_opcode)
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
		coerce(&optyp_1, type);
		if (OC_CAT == bin_opcode)
		{
			ref1 = ref = maketriple(OC_CAT);
			catbp = &ref->backptr;		/* borrow backptr to track args */
			saw_se = saw_local = FALSE;
			for (op_count = 2; ; op_count++) /* op_count = first operand plus destination */
			{
				parm = newtriple(OC_PARAMETER);
				ref1->operand[1] = put_tref(parm);
				ref1 = parm;
				ref1->operand[0] = optyp_1;
				if (se_handling)
				{	/* the following code deals with protecting lvn values from change by a following
					 * side effect and thereby produces a standard evaluation order. It is similar to code in
					 * expritem for function arguments, but has slightly different and easier circumstances
					 */
					assert(OLD_SE != TREF(side_effect_handling));
					assert(0 < TREF(expr_depth));
					t1 = optyp_1.oprval.tref;
					t2 = (oc_tab[t1->opcode].octype & OCT_COERCE) ? t1->operand[0].oprval.tref
						: t1;	/* need to step back past coerce of side effects in order to detect them */
					if (((OC_VAR == t2->opcode) || (OC_GETINDX == t2->opcode)) && (t1 == t2))
						saw_local = TRUE;	/* left operand is an lvn */
					if (saw_local)
					{
						if ((TREF(side_effect_base))[TREF(expr_depth)])
							saw_se = TRUE;
						if (saw_se || (OC_VAR == t2->opcode) || (OC_GETINDX == t2->opcode))
						{	/* chain stores args to manage later insert of temps to hold lvn */
							tripbp = &ref1->backptr;
							assert((tripbp == tripbp->que.fl) && (tripbp == tripbp->que.bl));
							tripbp->bpt = ref1;
							dqins(catbp, que, tripbp);
						}
					}
				}
				if (TK_UNDERSCORE != TREF(window_token))
				{
					if (!saw_se)			/* suppressed standard or lucked out on ordering */
						saw_local = FALSE;	/* just clear the backptrs - shut off other processing */
					dqloop(catbp, que, tripbp)
					{	/* work chained arguments which are in reverse order */
						argtrip = tripbp->bpt;
						assert(NULL != argtrip);
						dqdel(tripbp, que);
						tripbp->bpt = NULL;
						if (!saw_local)
							continue;
						/* some need to insert temps */
						for (optyp_ptr = &argtrip->operand[0]; INDR_REF == optyp_ptr->oprclass;
								optyp_ptr = optyp_ptr->oprval.indr)
							;	/* INDR_REFs used by e.g. extrinsics finally end up at a TRIP_REF */
						t1 = optyp_ptr->oprval.tref;
						if ((OC_VAR == t1->opcode) || (OC_GETINDX == t1->opcode))
						{	/* have an lvn that needs a temp because threat from some side effect */
							argtrip = maketriple(OC_STOTEMP);
							argtrip->operand[0] = put_tref(t1);
							dqins(t1,  exorder, argtrip); /* NOTE: violates infomation hiding */
							optyp_ptr->oprval.tref = argtrip;
							if (se_warn)
								ISSUE_SIDEEFFECTEVAL_WARNING(t1->src.column + 1);
						}
					}					/* end of side effect processing */
					assert((catbp == catbp->que.fl) && (catbp == catbp->que.bl) && (NULL == catbp->bpt));
					assert(op_count > 1);
					ref->operand[0] = put_ilit(op_count);
					ins_triple(ref);
					break;
				}
				advancewindow();
				if (!expratom(&optyp_1))
				{
					stx_error(ERR_RHMISSING);
					return EXPR_FAIL;
				}
				coerce(&optyp_1, type);
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
				if (!compile_pattern(&optyp_2, ind_pat))
					return EXPR_FAIL;
			} else
			{
				advancewindow();
				if (!expratom(&optyp_2))
				{
					stx_error(ERR_RHMISSING);
					return EXPR_FAIL;
				}
			}
			coerce(&optyp_2, type);
			ref1 = optyp_1.oprval.tref;
			if (((OC_VAR == ref1->opcode) || (OC_GETINDX == ref1->opcode))
				&& (TREF(side_effect_base))[TREF(expr_depth)])
			{	/* this section is to protect lvns from changes by a following side effect extrinsic or function
				 * by inserting a temporary to capture the lvn evaluation before it's changed by a "later" or
				 * "to-the-right" side effect; a preexisting coerce or temporary might already to the job;
				 * indirects may already have been shifted to evaluate early
				 */
				assert(OLD_SE != TREF(side_effect_handling));
				ref = maketriple(OC_STOTEMP);
				ref->operand[0] = optyp_1;
				optyp_1 = put_tref(ref);
				dqins(ref1, exorder, ref);	/* NOTE: another violation of information hiding */
				if (se_warn)
					ISSUE_SIDEEFFECTEVAL_WARNING(ref1->src.column + 1);
			}
			ref = newtriple(bin_opcode);
			ref->operand[0] = optyp_1;
			ref->operand[1] = optyp_2;
		}
		optyp_1 = put_tref(ref);
	}
	*a = optyp_1;
	return (OC_INDGLVN == (TREF(curtchain))->exorder.bl->opcode) ? EXPR_INDR : EXPR_GOOD;
}
