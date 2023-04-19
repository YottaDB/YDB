/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "stringpool.h"
#include "gtm_string.h"
#include "gtm_utf8.h"

GBLREF boolean_t	run_time;
GBLREF triple		t_orig;

error_def(ERR_EXPR);
error_def(ERR_MAXARGCNT);
error_def(ERR_RHMISSING);
error_def(ERR_SIDEEFFECTEVAL);

LITREF octabstruct oc_tab[];
LITREF toktabtype tokentable[];

GBLREF  spdesc          stringpool;

/**
 * Given a start token that represents a non-unary operation, consumes tokens and constructs an appropriate triple tree.
 * Adds the triple tree to the chain of execution.
 * @input[out] a A pointer that will be set to the last token seen
 * @returns An integer flag of; EXPR_INDR or EXPR_GOOD or EXPR_FAIL
 * @par Side effects
 *  - Calls advance window multiple times, and consumes tokens accordingly
 *  - Calls expratom multiple times, which (most notably) adds literals to a hash table
 *  - Calls ins_triple, which adds triples to the execution chain
 */
int eval_expr(oprtype *a)
/* process an expression into the operand at *a */
{
	boolean_t	ind_pat, saw_local, saw_se, se_warn, replaced, lh_se, rh_se, start_se;
	int		op_count, se_handling;
	opctype		bin_opcode;
	oprtype		optyp_1, optyp_2, *optyp_ptr;
	tbp		*catbp, *tripbp;
	triple		*argtrip, *parm, *ref, *ref1, *t1, *t2;
	mliteral	*m1, *m2;
	mval		tmp_mval;
<<<<<<< HEAD
	enum octype_t	type;
=======
	unsigned short	type;
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	CHKTCHAIN(TREF(curtchain), exorder, TRUE);	/* defined away in mdq.h except with DEBUG_TRIPLES */
	/* Using TREF(saw_side_effect) to determine, when that information is available, whether to treat a boolean as needing
	 * side-effect processing greatly simplifies things on the side of the ex_* and bx_* line-finishing functions. But there
	 * are some caveats. The code below would ideally only track whether or not side effects occur on the right-hand side by
	 * setting saw_side_effect to false before processing the rhs. After all, no rhs operand could possibly care what kinds
	 * of side effects were occurring for its parents' other child branch. Unfortunately, at least one part of the $Select
	 * logic depends on the saw_side_effect flag that is enabled there to persist down to any global variable references.
	 * Until $select is revised to juggle fewer pieces of global state, this logic must not alter the *truth value* of
	 * saw_side_effect, but it can and will alter the *value* to track whether any callees encounter side effectsthemselves.
	 * The reason for doing this is to see whether or not any logical operands will need to become their side-effect-processing
	 * counterparts: for this purpose we do not care whether there were any side effects in our left-hand subtree - these will
	 * be processedanyway. The logic below is intended to gracefully handle (as yet unknown) direct eval_expr recursion.
	 * It does assume:
	 * 	- The value is set to 0x1 if and only if it is set outside of eval_expr, and the converse
	 * 	- Callees that modify the value always do so by setting it to TRUE, except for callee eval_exprs
	 * 	- TRUE is always identical to 0x1, and 0x1 and 0x10 are always represented by different bits
	 * 	- Any value other than zero evaluates to TRUE if evalueated as a boolean (0 | value).
	 * And guarantees:
	 * 	- It returns with an identical saw_side_effects value as on entry if and only if no callees saw side effects
	 * 	- It returns with a true saw_side_effects value different from on entry if and only if at least one callee
	 * 		saw side effects
	 * */
	lh_se = FALSE;
	rh_se = FALSE;
	assert(TREF(saw_side_effect) == 0x1 || TREF(saw_side_effect) == 0x0 || TREF(saw_side_effect) == 0x10);
	start_se = TREF(saw_side_effect);
	if (TREF(saw_side_effect))
		(TREF(saw_side_effect) = 0x10);
	if (!expratom(&optyp_1))
	{	/* If didn't already add an error of our own, do so now to catch all expression errors */
		if (!ALREADY_RTERROR)
			stx_error(ERR_EXPR);
		return EXPR_FAIL;
	}
	/* If saw_side_effect was false at the start, bit zero is set if and only if the threadgbl is set by a callee
	 * (because we don't set it before calling). If saw_side_effect was true at the start, bit zero is set if
	 * and only if it is set by a callee (because we turn it into 0x10 before calling).
	 */
	lh_se = ((TREF(saw_side_effect)) & 0x1);
	TREF(saw_side_effect) = lh_se ? TRUE : start_se;
	se_handling = TREF(side_effect_handling);
	se_warn = SE_WARN_ON;
	while (bin_opcode = tokentable[TREF(window_token)].bo_type)	/* NOTE assignment NOT condition */
	{
		if (TREF(saw_side_effect))
			(TREF(saw_side_effect)) = 0x10;
		type = tokentable[TREF(window_token)].opr_type;
		if (oc_tab[bin_opcode].octype & OCT_BOOL)
		{
			if (!TREF(shift_side_effects))
			{
				assert(FALSE == TREF(saw_side_effect));
				for (ref = (TREF(curtchain))->exorder.bl; oc_tab[ref->opcode].octype & OCT_BOOL;
					ref = ref->exorder.bl)
						;
				assert(ref->exorder.bl != ref);
				if (&t_orig == ref->exorder.fl)
				{
					ref1 = maketriple(OC_NOOP);
					dqins(ref, exorder, ref1);
				}
				assert(&t_orig != ref->exorder.fl);
				assert(0 < TREF(expr_depth));
				TREF(expr_start) = TREF(expr_start_orig) = ref;
				CHKTCHAIN(TREF(curtchain), exorder, TRUE); /* defined away in mdq.h except with DEBUG_TRIPLES */
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
			for (op_count = 2; (MAX_ARGS - 3) >= op_count ; op_count++) /* op_count = first operand plus destination */
			{	/* If we can, concat string literals at compile-time rather than runtime */
				replaced = FALSE;
				if ((OC_PARAMETER == ref1->opcode)
					&& (TRIP_REF == ref1->operand[0].oprclass)
					&& (OC_LIT == ref1->operand[0].oprval.tref->opcode)
					&& (TRIP_REF == optyp_1.oprclass)
					&& (OC_LIT == optyp_1.oprval.tref->opcode))
				{	/* Copy the string over */
					m1 = ref1->operand[0].oprval.tref->operand[0].oprval.mlit;
					m2 = optyp_1.oprval.tref->operand[0].oprval.mlit;
					tmp_mval.mvtype = MV_STR;
					tmp_mval.str.char_len = m1->v.str.char_len + m2->v.str.char_len;
					tmp_mval.str.len = m1->v.str.len + m2->v.str.len;
					ENSURE_STP_FREE_SPACE(tmp_mval.str.len);
					tmp_mval.str.addr = (char *)stringpool.free;
					memcpy(tmp_mval.str.addr, m1->v.str.addr, m1->v.str.len);
					memcpy(tmp_mval.str.addr + m1->v.str.len, m2->v.str.addr, m2->v.str.len);
					stringpool.free = (unsigned char *)tmp_mval.str.addr + tmp_mval.str.len;
					s2n(&tmp_mval);		/* things rely on the compiler doing literals with complete types */
					ref1->operand[0] = put_lit(&tmp_mval);
					unuse_literal(&m1->v);
					unuse_literal(&m2->v);
					dqdel(optyp_1.oprval.tref, exorder);
					optyp_1 = ref1->operand[0];
					replaced = TRUE;
					op_count--;
				} else
				{
					parm = newtriple(OC_PARAMETER);
					ref1->operand[1] = put_tref(parm);
					ref1 = parm;
					ref1->operand[0] = optyp_1;
				}
				if (se_handling && !replaced)
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
							tripbp->bkptr = ref1;
							dqins(catbp, que, tripbp);
						}
					}
				}
				if (TK_UNDERSCORE != TREF(window_token))
				{
					if (!saw_se)			/* suppressed standard or lucked out on ordering */
						saw_local = FALSE;	/* just clear the backptrs - shut off other processing */
					/* This code checks to see if the only parameter for this OC_CAT is a string literal,
						and if it is, then it simply returns the literal*/
					assert(1 < op_count);
					if ((2 == op_count) && (OC_PARAMETER == ref1->opcode)
						&& (TRIP_REF == ref1->operand[0].oprclass)
						&& (OC_LIT == ref1->operand[0].oprval.tref->opcode))
					{	/* We need to copy some things from the original first */
						ref1->operand[0].oprval.tref->src = ref->src;
						t1 = ref1->operand[0].oprval.tref;
						ref = t1;
						optyp_1 = put_tref(t1);
						break;
					}
					dqloop(catbp, que, tripbp)
					{	/* work chained arguments which are in reverse order */
						argtrip = tripbp->bkptr;
						assert(NULL != argtrip);
						dqdel(tripbp, que);
						tripbp->bkptr = NULL;
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
					assert((catbp == catbp->que.fl) && (catbp == catbp->que.bl) && (NULL == catbp->bkptr));
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
				rh_se = ((TREF(saw_side_effect)) & 0x1);
				coerce(&optyp_1, type);
			}
			if ((MAX_ARGS - 3) < op_count)
			{
				stx_error(ERR_MAXARGCNT, 1, MAX_ARGS - 3);
				return EXPR_FAIL;
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
				CHKTCHAIN(TREF(curtchain), exorder, TRUE); /* defined away in mdq.h except with DEBUG_TRIPLES */
				if (!expratom(&optyp_2))
				{
					stx_error(ERR_RHMISSING);
					return EXPR_FAIL;
				}
			}
			rh_se = ((TREF(saw_side_effect)) & 0x1); /* Same logic as lh, see comment at top of function */
			CHKTCHAIN(TREF(curtchain), exorder, TRUE);	/* defined away in mdq.h except with DEBUG_TRIPLES */
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
			if (rh_se && (GTM_BOOL != TREF(gtm_fullbool))
					&& (oc_tab[ref->opcode].octype & OCT_BOOL) && (ref->opcode != OC_COBOOL))
				CONVERT_TO_SE(ref);
		}
		/* The logic below mirrors the logic at the top and does the following:
		 * The expression will be the lhs of a new operator if this loop continues,
		 * so set lh_se to equal whether a new se was encountered in either branch and reset rh_se.
		 * If there was a side effect in either branch, make sure any caller eval_exprs know it
		 * (by setting the value to TRUE). Since lh_se can never go from false to true, this function will always
		 * correctly report any side effects even in early left-branches to its callers, but will still be able
		 * to detect any novel rh_se for itself so as to adjust boolean handling correctly.
		 */
		lh_se = (lh_se || rh_se);
		rh_se = FALSE;
		TREF(saw_side_effect) = lh_se ? TRUE : start_se;
		optyp_1 = put_tref(ref);
	}
	*a = optyp_1;
	return (OC_INDGLVN == (TREF(curtchain))->exorder.bl->opcode) ? EXPR_INDR : EXPR_GOOD;
}
