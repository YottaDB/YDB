/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2020 YottaDB LLC and/or its subsidiaries.	*
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
#include "op.h"
#include "opcode.h"
#include "fullbool.h"
#include "stringpool.h"
#include "toktyp.h"
#include "flt_mod.h"

LITREF mval		literal_minusone, literal_one, literal_zero;
LITREF octabstruct	oc_tab[];

void ex_tail(oprtype *opr, int depth)
/* work a non-leaf operand toward final form
 * contains code to do arthimetic on literals at compile time
 * and code to bracket Boolean expressions with BOOLINIT and BOOLFINI
 */
{
	mval		*v, *v0, *v1;
	opctype		c, andor_opcode;
	oprtype		*i;
	triple		*bftrip, *bitrip, *t, *t0, *t1, *t2, *depthtrip;
	enum octype_t	oct;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TRIP_REF == opr->oprclass);
	UNARY_TAIL(opr, depth); /* this is first because it can change opr and thus whether we should even process the tail */
	RETURN_IF_RTS_ERROR;
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
		while (OCT_ARITH & oct)					/* really a sneaky if that allows us to use breaks */
		{	/* Consider moving this to a separate module (say, ex_arithlit) for clarity and modularity */
			/* binary arithmetic operations might be on literals, which can be performed at compile time */
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
				break;					/* from while */
			for (t0 = t->operand[0].oprval.tref; TRIP_REF == t0->operand[0].oprclass; t0 = t0->operand[0].oprval.tref)
				dqdel(t0, exorder);
			for (t1 = t->operand[1].oprval.tref; TRIP_REF == t1->operand[0].oprclass; t1 = t1->operand[0].oprval.tref)
				dqdel(t1, exorder);
			v0 = &t0->operand[0].oprval.mlit->v;
			MV_FORCE_NUMD(v0);
			v1 = &t1->operand[0].oprval.mlit->v;
			MV_FORCE_NUMD(v1);
			v = (mval *)mcalloc(SIZEOF(mval));
			switch (c)
			{
			case OC_ADD:
				op_add(v0, v1, v);
				break;
			case OC_DIV:
			case OC_IDIV:
				if (!(MV_NM & v1->mvtype) || (0 != v1->m[1]))
				{
					if (OC_DIV == c)
						op_div(v0, v1, v);
					else
						op_idiv(v0, v1, v);
				} else				/* divide by literal 0 is a technique so let it go to run time*/
					v = NULL;		/* flag value to get out of nested switch */
				break;
			case OC_EXP:
				op_exp(v0, v1, v);
				break;
			case OC_MOD:
				flt_mod(v0, v1, v);
				break;
			case OC_MUL:
				op_mul(v0, v1, v);
				break;
			case OC_SUB:
				op_sub(v0, v1, v);
				break;
			default:
				assertpro(FALSE && t1->opcode);
				break;
			}
			RETURN_IF_RTS_ERROR;
			if (NULL == v)				/* leaving divide by literal 0 to create a run time error */
				break;				/* from while */
			/* If result is not a numeric (possible in case result had a NUMOFLOW) drop idea of compile optimization.
			 * Instead issue runtime error if this codepath is encountered.
			 */
			if (!(MV_NM & v->mvtype))
				break;
			unuse_literal(v0);			/* drop original literals only after deciding whether to defer */
			unuse_literal(v1);
			dqdel(t0, exorder);
			dqdel(t1, exorder);
			n2s(v);
			s2n(v);					/* compiler must leave literals with both numeric and string */
			t->opcode = OC_LIT;			/* replace the original operator triple with new literal */
			put_lit_s(v, t);
			t->operand[1].oprclass = NO_REF;
			assert(opr->oprval.tref == t);
			return;
		}
		return;
	}
	/* the following code deals with Booleans where the expression is not directly managing flow - those go through bool_expr */
	t1 = bool_return_leftmost_triple(t);
	bitrip = maketriple(OC_BOOLINIT);
	DEBUG_ONLY(bitrip->src = t->src);
	dqins(t1->exorder.bl, exorder, bitrip);
	t2 = t->exorder.fl;
	assert((OC_COMVAL == t2->opcode) || (OC_COMINT == t2->opcode));
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
	return;
}
