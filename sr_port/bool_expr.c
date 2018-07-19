/****************************************************************
 *                                                              *
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
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

LITREF octabstruct	oc_tab[];

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
	UNARY_TAIL(&x);
	for(t2 = t1 = x.oprval.tref; OCT_UNARY & oc_tab[t1->opcode].octype; t2 = t1, t1 = t1->operand[0].oprval.tref)
		;
	if (OCT_ARITH & oc_tab[t1->opcode].octype)
		ex_tail(&t2->operand[0]);
	else if (OCT_BOOL & oc_tab[t1->opcode].octype)
		bx_boollit(t1);
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
		DECREMENT_EXPR_DEPTH;
		return TRUE;
	}
	bx_tail(x.oprval.tref, sense, addr);
	DECREMENT_EXPR_DEPTH;
	return TRUE;
}
