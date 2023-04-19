/****************************************************************
 *                                                              *
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

int bool_expr(boolean_t sense, oprtype *addr)
/*
 * invoked to resolve expresions that are by definition coerced to Boolean, which include
 * IF arguments, $SELECT() arguments, and postconditionals for both commands and arguments
 * IF is the only one that comes in with the "TRUE" sense
 * *addr winds up as an pointer to a jump operand, which the caller fills in
 */
{
	mval		*v;
	oprtype		x;
	triple		*t1, *t2;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	INCREMENT_EXPR_DEPTH;
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
	coerce(&x, OCT_BOOL);
	/* In this function, x is a convenience opr expected by eval_expr and unary_tail, but unused elsewhere.
	 * It points to the toplevel expression of this bool_expr, COBOOLed if not bool already by definition.
	 * It refers to a triple, call it t0, which may itself refer to triples (call them t1/t2). We call ex_tail
	 * on t1 and t2, but not t0, because this function takes care of the boolean handling for that top-level
	 * triple, but we need to guarantee that everything lower is processed first. This function therefore must
	 * mirror the structure of ex_tail, except insofar as it implements the boolinit/fini-free boolean handling.
	 * As for the flags: the first tells ex_tail that any directly-nested booleans will be processed by a caller
	 * function, so don't construct a boolchain for them yet. The final one says that this isn't a COMVAL-COBOOL
	 * situation, where we'd need to defer making the COBOOL into a boolchain in view of pending simplification by
	 * unary_tail or something similar.
	 */
	if (x.oprval.tref->operand[0].oprclass == TRIP_REF)
		ex_tail(&x.oprval.tref->operand[0], TRUE, FALSE);
	if (x.oprval.tref->operand[1].oprclass == TRIP_REF)
		ex_tail(&x.oprval.tref->operand[1], TRUE, FALSE);
	for(t2 = t1 = x.oprval.tref; OCT_UNARY & oc_tab[t1->opcode].octype; t2 = t1, t1 = t1->operand[0].oprval.tref)
		;
	if (OCT_ARITH & oc_tab[t1->opcode].octype)
		ex_arithlit(t1);
	UNARY_TAIL(&x);
	if (OCT_BOOL & oc_tab[t1->opcode].octype)
		bx_boollit(t1);
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
		DECREMENT_EXPR_DEPTH;
		return TRUE;
	}
	bx_tail(x.oprval.tref, sense, addr);
	CHKTCHAIN(TREF(curtchain), exorder, TRUE);	/* defined away in mdq.h except with DEBUG_TRIPLES */
	DECREMENT_EXPR_DEPTH;
	return TRUE;
}
