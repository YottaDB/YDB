/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

error_def(ERR_NUMOFLOW);

void ex_tail(oprtype *opr)
/* work a non-leaf operand toward final form
 * contains code to do arthimetic on literals at compile time
 * and code to bracket Boolean expressions with BOOLINIT and BOOLFINI
 */
{
	boolean_t	stop, tv;
	mval		*v, *v0, *v1;
	opctype		c;
	oprtype		*i;
	triple		*bftrip, *bitrip, *t, *t0, *t1, *t2;
	uint		bexprs, j, oct;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TRIP_REF == opr->oprclass);
	UNARY_TAIL(opr);	/* this is first because it can change opr and thus whether we should even process the tail */
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
		for (i = t->operand, j = 0; ARRAYTOP(t->operand) > i; i++, j++)
		{
			if (TRIP_REF == i->oprclass)
			{
				for (t0 = i->oprval.tref; OCT_UNARY & oc_tab[t0->opcode].octype; t0 = t0->operand[0].oprval.tref)
					;
				if (OCT_BOOL & oc_tab[t0->opcode].octype)
					bx_boollit(t0);
				ex_tail(i);			/* chained Boolean or arithmetic */
				RETURN_IF_RTS_ERROR;
			}
		}
		while (OCT_ARITH & oct)					/* really a sneaky if that allows us to use breaks */
		{	/* Consider moving this to a separate module (say, ex_arithlit) for clarity and modularity */
			/* binary arithmetic operations might be on literals, which can be performed at compile time */
			for (i = t->operand, j = 0; ARRAYTOP(t->operand) > i; i++, j++)
			{
				if (OC_LIT != t->operand[j].oprval.tref->opcode)
					break;				/* from for */
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
			if (!(MV_NM & v1->mvtype) || !(MV_NM & v0->mvtype))
			{	/* if we don't have a useful number we can't do useful math */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NUMOFLOW);
				assert(TREF(rts_error_in_parse));
				return;
			}
			v = (mval *)mcalloc(SIZEOF(mval));
			switch (c)
			{
			case OC_ADD:
				op_add(v0, v1, v);
				break;
			case OC_DIV:
			case OC_IDIV:
			case OC_MOD:
				if (!(MV_NM & v1->mvtype) || (0 != v1->m[1]))
				{
					if (OC_DIV == c)
						op_div(v0, v1, v);
					else if (OC_MOD == c)
						flt_mod(v0, v1, v);
					else
						op_idiv(v0, v1, v);
				} else				/* divide by literal 0 is a technique so let it go to run time*/
					v = NULL;		/* flag value to get out of nested switch */
				break;
			case OC_EXP:
				op_exp(v0, v1, v);
				if ((1 == v->sgn) && (MV_INT & v->mvtype) && (0 == v->m[0]))
					v = NULL;		/* flag value from op_exp indicates DIVZERO, so leave to run time */
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
			unuse_literal(v0);			/* drop original literals only after deciding whether to defer */
			unuse_literal(v1);
			dqdel(t0, exorder);
			dqdel(t1, exorder);
			assert(v->mvtype);
			n2s(v);
			s2n(v);					/* compiler must leave literals with both numeric and string */
			t->opcode = OC_LIT;			/* replace the original operator triple with new literal */
			put_lit_s(v, t);
			t->operand[1].oprclass = NO_REF;
			assert(opr->oprval.tref == t);
			return;
		}
		if ((OC_COMINT == c) && (OC_BOOLINIT == (t0 = t->operand[0].oprval.tref)->opcode)) /* WARNING assignment */
			opr->oprval.tref = t0;
		return;
	}
	/* the following code deals with Booleans where the expression is not directly managing flow - those go through bool_expr */
	for (t1 = t; ; t1 = t2)
	{
		assert(TRIP_REF == t1->operand[0].oprclass);
		t2 = t1->operand[0].oprval.tref;
		if (!(OCT_BOOL & oc_tab[t2->opcode].octype))
			break;
	}
	bitrip = maketriple(OC_BOOLINIT);
	DEBUG_ONLY(bitrip->src = t->src);
	dqrins(t1, exorder, bitrip);
	t2 = t->exorder.fl;
	assert((OC_COMVAL == t2->opcode) || (OC_COMINT == t2->opcode));	/* may need to change COMINT to COMVAL in bx_boolop */
	assert(&t2->operand[0] == opr);				/* check next operation ensures an expression */
	bftrip = maketriple(OC_BOOLFINI);
	DEBUG_ONLY(bftrip->src = t->src);
	bftrip->operand[0] = put_tref(bitrip);
	opr->oprval.tref = bitrip;
	dqins(t, exorder, bftrip);
	i = (oprtype *)mcalloc(SIZEOF(oprtype));
	bx_tail(t, FALSE, i);
	RETURN_IF_RTS_ERROR;
	if (OC_COMINT == (t2 = bftrip->exorder.fl)->opcode)	/* after bx_tail/bx_boolop it's safe to delete any OC_COMINT left */
		dqdel(t2, exorder);
	*i = put_tnxt(bftrip);
	CHKTCHAIN(TREF(curtchain), exorder, TRUE);	/* defined away in mdq except with DEBUG_TRIPLES */
	return;
}
