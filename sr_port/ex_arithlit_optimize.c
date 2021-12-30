/****************************************************************
 *								*
 * Copyright (c) 2017-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 * Portions Copyright (c) 2001-2019 Fidelity National		*
 * Information Services, Inc. and/or its subsidiaries.		*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* While FIS did not write this module, it was derived from FIS code in "sr_port/ex_tail.c"
 * that took care of compile-time literal optimization in case of an OCT_ARITH opcode type.
 */

#include "mdef.h"

#include "compiler.h"
#include "mdq.h"

/* This file was created from code that used to be in "sr_port/ex_tail.c" and which took care of
 * compile-time literal optimization in case of an OCT_ARITH opcode type. Hence inherited the
 * FIS and YottaDB copyrights from that file.
 */

GBLREF boolean_t             run_time;

#ifdef DEBUG
LITREF octabstruct	oc_tab[];
#endif

/* This function takes in an input parameter "t" which is a pointer to a triple structure corresponding to
 * a binary arithmetic operation.
 *
 * It checks if all operands of the operation are literals and if so tries to compute the result and replace the
 * arithmetic operation triple with the result literal triple in the current triple chain.
 *
 * If it is not able to optimize (e.g. all operands are not literals OR all operands are literals but computation
 * encountered errors like DIVZERO etc.), it leaves the the triple chain unchanged.
 */
void ex_arithlit_optimize(triple *t)
{
	boolean_t	save_run_time;
	mval		*v, *v0, *v1;
	oprtype		*i;
	triple		*t0, *t1;
#	ifdef DEBUG
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	assert(OCT_ARITH & oc_tab[t->opcode].octype);	/* caller should have ensured this */
	for (i = t->operand; ARRAYTOP(t->operand) > i; i++)
	{
		if (OC_LIT != i->oprval.tref->opcode)
			break;				/* from for */
		/* Go down to the mlit and ensure it has a numeric type */
		for (t0 = i->oprval.tref; TRIP_REF == t0->operand[0].oprclass; t0 = t0->operand[0].oprval.tref)
			;
		assert(MLIT_REF == t0->operand[0].oprclass);
		v0 = &t0->operand[0].oprval.mlit->v;
		MV_FORCE_NUM(v0);
		if (!(MV_NM & v0->mvtype))
			break;
	}
	if (ARRAYTOP(t->operand) > i)
	{	/* At least one of the operands is not a literal or a number. Cannot optimize at compile-time. */
		return;
	}
	for (t0 = t->operand[0].oprval.tref; TRIP_REF == t0->operand[0].oprclass; t0 = t0->operand[0].oprval.tref)
		dqdel(t0, exorder);
	for (t1 = t->operand[1].oprval.tref; TRIP_REF == t1->operand[0].oprclass; t1 = t1->operand[0].oprval.tref)
		dqdel(t1, exorder);
	v0 = &t0->operand[0].oprval.mlit->v;
	MV_FORCE_NUMD(v0);
	v1 = &t1->operand[0].oprval.mlit->v;
	MV_FORCE_NUMD(v1);
	/* Take a copy of "run_time" variable and set it to TRUE for the below computation.
	 * This will ensure that any runtime errors encountered inside "ex_arithlit_compute" do not invoke
	 * "stx_error_va_fptr" as part of a "rts_error_csa" call and return prematurely. We want such errors
	 * to go through the "ex_arithlit_compute_ch" condition handler and return back to us instead of returning
	 * from the "rts_error" call and proceeding inside (say op_div.c) as if no error had occurred.
	 */
	save_run_time = run_time;
	run_time = TRUE;
	v = ex_arithlit_compute(t->opcode, v0, v1);
	run_time = save_run_time;	/* Reset "run_time" back to original value */
	/* The above call does not invoke the parser. It invokes runtime functions. So we do not expect any parse
	 * time errors. Hence the assert below and hence no need for a RETURN_IF_RTS_ERROR macro call here.
	 */
	assert(!TREF(rts_error_in_parse));
	if (NULL == v)
	{	/* This is a case of an error (e.g. DIVZERO, NUMOFLOW, NEGFRACPWR etc.).
		 * Return without optimizing.
		 */
		return;
	}
	if (!(MV_NM & v->mvtype))
	{	/* This is a case of result not being numeric (possible in case result had a NUMOFLOW).
		 * Drop idea of compile optimization.
		 */
		return;
	}
	/* Optimization succeeded. Proceed to replace triples in execution chain with the computed literal triple */
	unuse_literal(v0);			/* drop original literals only after deciding whether to defer */
	unuse_literal(v1);
	dqdel(t0, exorder);
	dqdel(t1, exorder);
	n2s(v);
	s2n(v);					/* compiler must leave literals with both numeric and string */
	t->opcode = OC_LIT;			/* replace the original operator triple with new literal */
	put_lit_s(v, t);
	t->operand[1].oprclass = NO_REF;
	return;
}

