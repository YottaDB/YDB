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
#include "gtm_string.h"		/* needed by INCREMENT_EXPR_DEPTH */
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
	boolean_t	is_com, tv;
	uint4		bexprs;
	opctype		c;
	oprtype		x;
	triple		*bitrip, *t, *t0, *t1, *t2;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	INCREMENT_EXPR_DEPTH;
	if (!eval_expr(&x))
	{
		DECREMENT_EXPR_DEPTH;
		return FALSE;
	}
	UNARY_TAIL(&x);
	if (OC_LIT == (x.oprval.tref)->opcode)
	{	/* if its just a literal don't waste time */
		DECREMENT_EXPR_DEPTH;
		return TRUE;
	}
	assert(TRIP_REF == x.oprclass);
	coerce(&x, OCT_BOOL);
	t = x.oprval.tref;
	for (t1 = t; ; t1 = t2)
	{
		assert(TRIP_REF == t1->operand[0].oprclass);
		t2 = t1->operand[0].oprval.tref;
		if (!(oc_tab[t2->opcode].octype & OCT_BOOL))
			break;
	}
	if (OC_INDGLVN == t2->opcode)
		t1 = t2;	/* because of how we process indirection, can't insert a NOOP between COBOOL and INDGLGN */
	bitrip = maketriple(OC_BOOLINIT);						/* a marker we'll delete later */
	dqins(t1->exorder.bl, exorder, bitrip);
	assert(TREF(curtchain) ==  t->exorder.fl);
	(TREF(curtchain))->operand[0] = put_tref(bitrip);
	bx_tail(t, sense, addr);
	(TREF(curtchain))->operand[0].oprclass = NO_REF;
	assert(t == x.oprval.tref);
	DECREMENT_EXPR_DEPTH;
	for (bexprs = 0, t0 = t; bitrip != t0; t0 = t0->exorder.bl)
	{
		if (OCT_JUMP & oc_tab[c = t0->opcode].octype)				/* WARNING assignment */
		{
			switch (t0->opcode)
			{
				case OC_JMPFALSE:
				case OC_JMPTRUE:
					assert(INDR_REF == t0->operand[0].oprclass);
					t0->opcode = (OC_JMPTRUE == t0->opcode) ? OC_NOOP : OC_JMP;
					t0->operand[0].oprclass = (OC_NOOP ==  t0->opcode) ? NO_REF : INDR_REF;
					if (!bexprs++)
						t = t0;
					break;
				default:
					bexprs += 2;
			}
		}
	}
	bitrip->opcode = OC_NOOP;							/* ditch it after it served us */
	if (1 == bexprs)
	{	/* if there is just a one JMP TRUE / FALSE turn it into a literal */
		assert((OC_NOOP ==  t->opcode) || (OC_JMP ==  t->opcode));
		PUT_LITERAL_TRUTH((OC_NOOP == t->opcode) ^ sense, t);
		t->opcode = OC_LIT;
	} else if (!bexprs && (OC_COBOOL == t->opcode) && (OC_LIT == (t0 = t->operand[0].oprval.tref)->opcode)
		&& ((OC_JMPEQU == t->exorder.fl->opcode) || (OC_JMPNEQ == t->exorder.fl->opcode)))
	{	/* just one jump based on a literal, so resolve it */
		t->opcode = OC_NOOP;
		t->operand[0].oprclass = NO_REF;
		t = t->exorder.fl;
		dqdel(t, exorder);
		unuse_literal(&t0->operand[0].oprval.mlit->v);
		tv = (((0 == t0->operand[0].oprval.mlit->v.m[1]) ? OC_JMPNEQ : OC_JMPEQU) == t->opcode) ^ sense;
		PUT_LITERAL_TRUTH(tv, t0);
	}
	return TRUE;
}
