/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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
#include "mdq.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "compile_pattern.h"
#include "fullbool.h"
#include "show_source_line.h"
#include "stringpool.h"
#include "gtm_string.h"
#include "gtm_utf8.h"

GBLREF	boolean_t	run_time;

error_def(ERR_EXPR);
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
	boolean_t	ind_pat, saw_local, saw_se, se_warn, replaced;
	int		op_count, se_handling;
	opctype		bin_opcode;
	oprtype		optyp_1, optyp_2, *optyp_ptr;
	tbp		*catbp, *tripbp;
	triple		*argtrip, *parm, *ref, *ref1, *t1, *t2;
	mliteral	*m1, *m2;
	mval		tmp_mval;
	int i = 0;
	unsigned short	type;

	DCL_THREADGBL_ACCESS;
	SETUP_THREADGBL_ACCESS;
	if (!expratom(&optyp_1))
	{	/* If didn't already add an error of our own, do so now with catch all expression error */
		if (!ALREADY_RTERROR)
			stx_error(ERR_EXPR);
		return EXPR_FAIL;
	}
	se_handling = TREF(side_effect_handling);
	se_warn = SE_WARN_ON;
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
					optyp_1 = ref1->operand[0];
					replaced = TRUE;
					unuse_literal(&m1->v);
					unuse_literal(&m2->v);
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
							tripbp->bpt = ref1;
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
