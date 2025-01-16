/****************************************************************
 *                                                              *
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"         /* needed by INCREMENT_EXPR_DEPTH */
#include "compiler.h"
#include "mdq.h"
#include "mmemory.h"
#include "opcode.h"
#include "stringpool.h"

LITREF	octabstruct	oc_tab[];

/*
 * Invoked to resolve expresions that are by definition coerced to Boolean, which include
 * IF arguments, $SELECT() arguments, and postconditionals for both commands and arguments
 * IF is the only one that comes in with the "TRUE" sense
 * *addr winds up as an pointer to a jump operand, which the caller fills in.
 *
 * On function return:
 * -------------------
 * *boolexprfinish_ptr is set to point to the OC_BOOLEXPRFINISH triple if one exists and NULL otherwise.
 */
int bool_expr(boolean_t sense, oprtype *addr, triple **boolexprfinish_ptr)
{
	mval		*v;
	opctype		andor_opcode;
	oprtype		x;
	triple		*t1, *t2;
	triple		*boolexprstart, *boolexprfinish;
	boolean_t	optimizable;
	int		num_coms;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	INCREMENT_EXPR_DEPTH;
	*boolexprfinish_ptr = NULL;
	if (!eval_expr(&x))
	{
		DECREMENT_EXPR_DEPTH;
		return FALSE;
	}
	assert(TRIP_REF == x.oprclass);
	if (OC_LIT == (x.oprval.tref)->opcode)
	{	/* if its just a literal don't waste time */
		DECREMENT_EXPR_DEPTH;
		return TRUE;
	}
	t1 = x.oprval.tref;
	t1 = (OCT_BOOL & oc_tab[t1->opcode].octype) ? bool_return_leftmost_triple(x.oprval.tref) : t1;
	boolexprstart = maketriple(OC_BOOLEXPRSTART);
	dqins(t1->exorder.bl, exorder, boolexprstart);
	coerce(&x, OCT_BOOL);
	boolexprfinish = newtriple(OC_BOOLEXPRFINISH);
	boolexprfinish->operand[0] = put_tref(boolexprstart);	/* This helps locate the corresponding OC_BOOLEXPRSTART
								 * given a OC_BOOLEXPRFINISH.
								 */
	*boolexprfinish_ptr = boolexprfinish;
	UNARY_TAIL(&x, 0);
	for (t2 = t1 = x.oprval.tref; OCT_UNARY & oc_tab[t1->opcode].octype; t2 = t1, t1 = t1->operand[0].oprval.tref)
		;
	if (OCT_ARITH & oc_tab[t1->opcode].octype)
		ex_tail(&t2->operand[0], 0);
	else if (OCT_BOOL & oc_tab[t1->opcode].octype)
		bx_boollit(t1, 0);
	/* It is possible "ex_tail" or "bx_boollit" is invoked above and has a compile-time error. In that case,
	 * "ins_errtriple" would be invoked which does a "dqdelchain" that could remove "t1" from the "t_orig"
	 * triple execution chain and so it is no longer safe to do "dqdel" etc. on "t1".
	 * Hence the RETURN_EXPR_IF_RTS_ERROR check below.
	 */
	RETURN_EXPR_IF_RTS_ERROR;
	for (t1 = x.oprval.tref; OC_NOOP == t1->opcode; t1 = t1->exorder.bl)
		;
	if ((OC_COBOOL == t1->opcode) && (OC_LIT == (t2 = t1->operand[0].oprval.tref)->opcode)
		&& (TREF(curtchain) != t2->exorder.bl) && !(OCT_JUMP & oc_tab[t2->exorder.bl->opcode].octype))
	{	/* returning literals directly simplifies things for all callers, so swap the sense */
		v = &t2->operand[0].oprval.mlit->v;
		unuse_literal(v);
		MV_FORCE_NUMD(v);
		PUT_LITERAL_TRUTH(MV_FORCE_BOOL(v), t2);
		dqdel(t1, exorder);
		REMOVE_BOOLEXPRSTART_AND_FINISH(boolexprfinish);
		*boolexprfinish_ptr = NULL;
		DECREMENT_EXPR_DEPTH;
		return TRUE;
	}
	/* Check if OC_EQU or OC_NEQU can be optimized (YDB#777).
	 * Also note that set:'(x'="") can be optimized as set:(x=""). Hence the additional check for OC_COM below.
	 * Similar checks are done for OC_EQU/OC_NEQU even when `""` is not one of the arguments and for other
	 * opcodes like OC_CONTAIN, OC_NCONTAIN etc.
	 */
	t1 = x.oprval.tref;
	optimizable = TRUE;
	num_coms = 0;
	if (OC_COM == t1->opcode)
	{
		t2 = t1;
		for ( ; ; )
		{
			assert(OC_COM == t1->opcode);
			num_coms++;
			t1 = t1->operand[0].oprval.tref;
			if ((OC_EQU == t1->opcode) || (OC_NEQU == t1->opcode))
				break;
			if ((OC_CONTAIN == t1->opcode) || (OC_NCONTAIN == t1->opcode))
				break;
			if ((OC_FOLLOW == t1->opcode) || (OC_NFOLLOW == t1->opcode))
				break;
			if ((OC_PATTERN == t1->opcode) || (OC_NPATTERN == t1->opcode))
				break;
			if ((OC_SORTSAFTER == t1->opcode) || (OC_NSORTSAFTER == t1->opcode))
				break;
			if ((OC_GT == t1->opcode) || (OC_NGT == t1->opcode))
				break;
			if ((OC_LT == t1->opcode) || (OC_NLT == t1->opcode))
				break;
			if (OC_COM != t1->opcode)
			{
				optimizable = FALSE;
				break;
			}
		}
	}
	if (optimizable)
	{
		opctype		new_opcode;
		boolean_t	is_equnul;

		is_equnul = FALSE;
		switch(t1->opcode)
		{
		case OC_EQU:
		case OC_NEQU:;
			oprtype		*t_opr;
			int		j;

			if (num_coms % 2)
				new_opcode = ((OC_EQU == t1->opcode) ? OC_NEQU_RETBOOL : OC_EQU_RETBOOL);
			else
				new_opcode = ((OC_EQU == t1->opcode) ? OC_EQU_RETBOOL : OC_NEQU_RETBOOL);
			for (t_opr = t1->operand, j = 0; t_opr < ARRAYTOP(t1->operand); t_opr++, j++)
			{
				if ((TRIP_REF == t_opr->oprclass) && (OC_LIT == t_opr->oprval.tref->opcode)
					&& (0 == t_opr->oprval.tref->operand[0].oprval.mlit->v.str.len))
				{
					/* OC_EQU for say [set:x="" ...] or [set:""=x ...] OR
					 * OC_NEQU for [set:x'="" ...] or [set:""'=x ...] can be optimized
					 * to one OC_EQUNUL_RETBOOL/OC_NEQUNUL_RETBOOL opcode. [YDB#777]
					 *
					 * Note that we do not remove the OC_BOOLEXPRSTART/OC_BOOLEXPRFINISH triples
					 * (part of this optimization) here as more OC_BOOLEXPRFINISH triples can be
					 * generated by the caller after we return and we cannot communicate to the
					 * caller to not generate those triples. Therefore, we let the caller add the
					 * triples as usual and postpone the removal of the OC_BOOLEXPRSTART and more
					 * than one OC_BOOLEXPRFINISH triples corresponding to this OC_BOOLEXPRSTART
					 * at a later stage in "sr_port/alloc_reg.c".
					 */
					if (0 == j)
						t1->operand[0] = t1->operand[1];
					is_equnul = TRUE;
					if (num_coms % 2)
						new_opcode = ((OC_EQU == t1->opcode) ? OC_NEQUNUL_RETBOOL : OC_EQUNUL_RETBOOL);
					else
						new_opcode = ((OC_EQU == t1->opcode) ? OC_EQUNUL_RETBOOL : OC_NEQUNUL_RETBOOL);
					break;
				}
			}
			break;
		case OC_CONTAIN:
		case OC_NCONTAIN:
			if (num_coms % 2)
				new_opcode = ((OC_CONTAIN == t1->opcode) ? OC_NCONTAIN_RETBOOL : OC_CONTAIN_RETBOOL);
			else
				new_opcode = ((OC_CONTAIN == t1->opcode) ? OC_CONTAIN_RETBOOL : OC_NCONTAIN_RETBOOL);
			break;
		case OC_FOLLOW:
		case OC_NFOLLOW:
			if (num_coms % 2)
				new_opcode = ((OC_FOLLOW == t1->opcode) ? OC_NFOLLOW_RETBOOL : OC_FOLLOW_RETBOOL);
			else
				new_opcode = ((OC_FOLLOW == t1->opcode) ? OC_FOLLOW_RETBOOL : OC_NFOLLOW_RETBOOL);
			break;
		case OC_PATTERN:
		case OC_NPATTERN:
			if (num_coms % 2)
				new_opcode = ((OC_PATTERN == t1->opcode) ? OC_NPATTERN_RETBOOL : OC_PATTERN_RETBOOL);
			else
				new_opcode = ((OC_PATTERN == t1->opcode) ? OC_PATTERN_RETBOOL : OC_NPATTERN_RETBOOL);
			break;
		case OC_SORTSAFTER:
		case OC_NSORTSAFTER:
			if (num_coms % 2)
				new_opcode = ((OC_SORTSAFTER == t1->opcode) ? OC_NSORTSAFTER_RETBOOL : OC_SORTSAFTER_RETBOOL);
			else
				new_opcode = ((OC_SORTSAFTER == t1->opcode) ? OC_SORTSAFTER_RETBOOL : OC_NSORTSAFTER_RETBOOL);
			break;
		case OC_GT:
		case OC_NGT:
			if (num_coms % 2)
				new_opcode = ((OC_GT == t1->opcode) ? OC_NGT_RETBOOL : OC_GT_RETBOOL);
			else
				new_opcode = ((OC_GT == t1->opcode) ? OC_GT_RETBOOL : OC_NGT_RETBOOL);
			break;
		case OC_LT:
		case OC_NLT:
			if (num_coms % 2)
				new_opcode = ((OC_LT == t1->opcode) ? OC_NLT_RETBOOL : OC_LT_RETBOOL);
			else
				new_opcode = ((OC_LT == t1->opcode) ? OC_LT_RETBOOL : OC_NLT_RETBOOL);
			break;
		default:
			optimizable = FALSE;
			break;
		}
		if (optimizable)
		{
			triple		*ref;

			ref = maketriple(sense ? OC_JMPNEQ : OC_JMPEQU);
			ref->operand[0] = put_indr(addr);
			dqins(t1, exorder, ref);
			/* Normally a OC_BOOLEXPRSTART triple has no operands. But in this case where we are postponing
			 * an optimization, we signal "alloc_reg.c" of this pending todo by setting operand[0] to the
			 * OC_EQUNUL_RETBOOL, OC_NEQUNUL_RETBOOL etc. triple. This is checked in "alloc_reg.c" and if
			 * found to be non-NULL it removes the OC_BOOLEXPRSTART/OC_BOOLEXPRFINISH triples (by replacing
			 * them with OC_NOOP triples).
			 */
			boolexprstart->operand[0] = put_tref(t1);	/* signal alloc_reg() a later optimization */
			t1->opcode = new_opcode;
			if (is_equnul)
				ex_tail(&t1->operand[0], 0);
			else
			{
				ex_tail(&t1->operand[0], 0);
				ex_tail(&t1->operand[1], 0);
			}
			/* Finally check if we came to OC_EQU/OC_NEQU/OC_CONTAIN/OC_NCONTAIN etc. through a OC_COM sequence.
			 * If so, remove those triples by replacing them with a OC_NOOP opcode.
			 */
			if (num_coms)
			{
				t1 = t2;	/* set t1 to point to outermost OC_COM triple */
				for ( ; ; )
				{
					assert(OC_COM == t1->opcode);
					t1->opcode = OC_NOOP;
					t1 = t1->operand[0].oprval.tref;
					if (OC_COM != t1->opcode)
						break;
				}
			}
			DECREMENT_EXPR_DEPTH;
			return TRUE;
		}
	}
	/* Pass CALLER_IS_BOOL_EXPR_FALSE if IF is caller of `bool_expr()`.
	 * Pass CALLER_IS_BOOL_EXPR_TRUE in all other cases.
	 * Since IF is the only one that comes in with the TRUE sense, we do a `!sense` to achieve this.
	 */
	andor_opcode = bx_get_andor_opcode(x.oprval.tref->opcode, OC_NOOP);
	bx_tail(x.oprval.tref, sense, addr, 0, andor_opcode, !sense, 0, IS_LAST_BOOL_OPERAND_TRUE);
	CHKTCHAIN(TREF(curtchain), exorder, TRUE);	/* defined away in mdq.h except with DEBUG_TRIPLES */
	DECREMENT_EXPR_DEPTH;
	return TRUE;
}
