/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "compiler.h"
#include "mdq.h"
#include "mmemory.h"
#include "opcode.h"
#include "fullbool.h"
#include "stringpool.h"
#include "toktyp.h"

LITREF mval		literal_minusone, literal_one, literal_zero;
LITREF octabstruct	oc_tab[];

void ex_tail(oprtype *opr, int depth)
/* work a non-leaf operand toward final form
 * contains code to do arthimetic on literals at compile time
 * and code to bracket Boolean expressions with BOOLINIT and BOOLFINI
 */
{
	opctype		c, andor_opcode;
	oprtype		*i;
	triple		*bftrip, *bitrip, *t, *t0, *t1, *t2, *depthtrip;
	enum octype_t	oct;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TRIP_REF == opr->oprclass);
	UNARY_TAIL(opr, depth); /* this is first because it can change opr and thus whether we should even process the tail */
	RETURN_IF_RTS_ERROR;
	CHKTCHAIN(TREF(curtchain), exorder, TRUE);	/* defined away in mdq.h except with DEBUG_TRIPLES */
	t = opr->oprval.tref; /* Refind t since UNARY_TAIL may have shifted it */
	c = t->opcode;
	oct = oc_tab[c].octype;
	if ((OCT_EXPRLEAF & oct) || (OC_NOOP == c))
		return;
	assert(TRIP_REF == t->operand[0].oprclass);
	assert((TRIP_REF == t->operand[1].oprclass) || (NO_REF == t->operand[1].oprclass));
	if (!(OCT_BOOL & oct))
	{
		for (i = t->operand; ARRAYTOP(t->operand) > i; i++)
		{
			if (TRIP_REF == i->oprclass)
			{
				for (t0 = i->oprval.tref; OCT_UNARY & oc_tab[t0->opcode].octype; t0 = t0->operand[0].oprval.tref)
					;
				if (OCT_BOOL & oc_tab[t0->opcode].octype)
				{
					bx_boollit(t0, depth);
					RETURN_IF_RTS_ERROR;
				}
				ex_tail(i, depth);	/* chained Boolean or arithmetic */
				RETURN_IF_RTS_ERROR;
			}
		}
		if (OCT_ARITH & oct)
		{	/* If it is a binary arithmetic operation, try compile-time optimizing it if all operands are literals */
			ex_arithlit_optimize(t);
		}
		return;
	}
	/* the following code deals with Booleans where the expression is not directly managing flow - those go through bool_expr */
	t1 = bool_return_leftmost_triple(t);
	bitrip = maketriple(OC_BOOLINIT);
	DEBUG_ONLY(bitrip->src = t->src);
	dqrins(t1, exorder, bitrip);
	t2 = t->exorder.fl;
	assert((OC_COMVAL == t2->opcode) || (OC_COMINT == t2->opcode));	/* may need to change COMINT to COMVAL in bx_boolop */
	assert(&t2->operand[0] == opr);				/* check next operation ensures an expression */
	/* Overwrite depth (set in coerce.c to INIT_GBL_BOOL_DEPTH) to current bool expr depth */
	assert(TRIP_REF == t2->operand[1].oprclass);
	depthtrip = t2->operand[1].oprval.tref;
	assert(OC_ILIT == depthtrip->opcode);
	assert(ILIT_REF == depthtrip->operand[0].oprclass);
	assert(INIT_GBL_BOOL_DEPTH == depthtrip->operand[0].oprval.ilit);
	depthtrip->operand[0].oprval.ilit = (mint)(depth + 1);
	bftrip = maketriple(OC_BOOLFINI);
	DEBUG_ONLY(bftrip->src = t->src);
	bftrip->operand[0] = put_tref(bitrip);
	opr->oprval.tref = bitrip;
	dqins(t, exorder, bftrip);
	i = (oprtype *)mcalloc(SIZEOF(oprtype));
	andor_opcode = bx_get_andor_opcode(t->opcode, OC_NOOP);
	CHECK_AND_RETURN_IF_BOOLEXPRTOODEEP(depth + 1);
	bx_tail(t, FALSE, i, depth + 1, andor_opcode, CALLER_IS_BOOL_EXPR_FALSE, depth + 1, IS_LAST_BOOL_OPERAND_TRUE);
	RETURN_IF_RTS_ERROR;
	*i = put_tnxt(bftrip);
	CHKTCHAIN(TREF(curtchain), exorder, TRUE);	/* defined away in mdq except with DEBUG_TRIPLES */
	return;
}
