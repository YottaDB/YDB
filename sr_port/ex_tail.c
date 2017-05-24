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
	triple		pos_in_chain, *bftrip, *bitrip, *t, *t0, *t1, *t2;
	uint		bexprs, j, oct;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TRIP_REF == opr->oprclass);
	UNARY_TAIL(opr);	/* this is first because it can change opr and thus whether we should even process the tail */
	t = opr->oprval.tref;
	c = t->opcode;
	oct = oc_tab[c].octype;
	if (OCT_EXPRLEAF & oct)
		return;
	assert(TRIP_REF == t->operand[0].oprclass);
	assert((TRIP_REF == t->operand[1].oprclass) || (NO_REF == t->operand[1].oprclass));
	if (!(OCT_BOOL & oct))
	{
		for (i = t->operand; ARRAYTOP(t->operand) > i; i++)
			if (TRIP_REF == i->oprclass)
				UNARY_TAIL(i);
                while (OCT_ARITH & oct)                                 /* really a sneaky if that allows us to use breaks */
                {       /* binary arithmetic operations might be on literals, which can be performed at compile time */
			for (i = t->operand, j = 0; ARRAYTOP(t->operand) > i; i++, j++)
			{
				if (((OCT_BOOL | OCT_ARITH | OCT_UNARY) & oc_tab[i->oprval.tref->opcode].octype))
					ex_tail(i);			/* chained Boolean or arithmetic */
				if (OC_LIT != t->operand[j].oprval.tref->opcode)
					break;				/* from for */
			}
			if (ARRAYTOP(t->operand) > i)
				break;					/* from while */
			pos_in_chain = TREF(pos_in_chain);
			TREF(pos_in_chain) = *(TREF(curtchain));
			for (t0 = t; TRIP_REF == t0->operand[0].oprclass; t0 = t0->operand[0].oprval.tref)
				if (t0 != t)
					dqdel(t0, exorder);
			for (t1 = t->operand[1].oprval.tref; TRIP_REF == t1->operand[0].oprclass; t1 = t1->operand[0].oprval.tref)
					dqdel(t1, exorder);
			v0 = &t0->operand[0].oprval.mlit->v;
			assert(MV_NM & v0->mvtype);
			v1 = &t1->operand[0].oprval.mlit->v;
			assert(MV_NM & v1->mvtype);
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
			if (NULL == v)				/* leaving divide by literal 0 to create a run time error */
				break;				/* from while */
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
			TREF(pos_in_chain) = pos_in_chain;
			return;
		}
		for (i = t->operand; ARRAYTOP(t->operand) > i; i++)
			if (TRIP_REF == i->oprclass)
				ex_tail(i);
		if ((OC_COMINT == c) && (OC_BOOLINIT == (t0 = t->operand[0].oprval.tref)->opcode))	/* WARNING assignment */
			opr->oprval.tref = t0;
		return;
	}
	/* the following code deals with Booleans where the expression is not managing flow - those go through bool_expr */
	for (t1 = t; ; t1 = t2)
	{
		assert(TRIP_REF == t1->operand[0].oprclass);
		t2 = t1->operand[0].oprval.tref;
		if (!(oc_tab[t2->opcode].octype & OCT_BOOL))
			break;
	}
	bitrip = maketriple(OC_BOOLINIT);
	dqins(t1->exorder.bl, exorder, bitrip);
	opr->oprval.tref = bitrip;
	t2 = t->exorder.fl;
	assert(&t2->operand[0] == opr);
	assert((OC_COMVAL == t2->opcode) || (OC_COMINT == t2->opcode));
	if (OC_COMINT == t2->opcode)
		dqdel(t2, exorder);
	bftrip = maketriple(OC_BOOLFINI);
	bftrip->operand[0] = put_tref(bitrip);
	dqins(t, exorder, bftrip);
	i = (oprtype *)mcalloc(SIZEOF(oprtype));
	bx_tail(t, FALSE, i);
	*i = put_tnxt(bftrip);
	/* the following logic is analogous to that in bool_expr, but there are material differences */
	if (TREF(curtchain) == bftrip->exorder.fl->exorder.fl)
		newtriple(OC_NOOP);
	for (t0 = t; OC_NOOP == t->opcode; t = t->exorder.bl)
		;
	if (OC_BOOLINIT == t->opcode)
		t = t0;											/* we want our NOOP back */
	for (bexprs = 0; bitrip != t0; t0 = t0->exorder.bl)
	{	/* find out if we are down to a single evaluation, and replace the JMP TRUE / FALSE while at it */
		if (OCT_JUMP & oc_tab[t0->opcode].octype)
		{
			if ((OC_JMPTRUE == t0->opcode) || (OC_JMPFALSE == t0->opcode))
			{	/* JMPTRUE better than NOOP while processing Booleans, but that's done, so replace it & JMPFALSE */
				assert(INDR_REF == t0->operand[0].oprclass);
				t0->opcode = (OC_JMPTRUE == t0->opcode) ? OC_NOOP : OC_JMP;
				t0->operand[0].oprclass = (OC_NOOP ==  t0->opcode) ? NO_REF : INDR_REF;
				if (!bexprs)
					t = t0;
			}
			bexprs++;
		}
	}
	t0 = bftrip->exorder.fl;
	assert(TREF(curtchain) != t0->exorder.fl);
	if (1 < bexprs)
		return;
	switch (t->opcode)
	{	/* this separates cases where we are left with a literal from others (not left with a literal) */
	case OC_LIT:	/* this case deals with the possibility of junk left from a concatenation collapse to a literal */
		if ((OC_JMP != (t1 = bftrip->exorder.bl)->opcode) && (OC_NOOP != t1->opcode))	/* WARNING assignment */
			break;
		for (t1 = t1->exorder.bl; bitrip != t1; t1 = t1->exorder.bl)
			if ((OC_LIT != t1->opcode) && (OC_PARAMETER != t1->opcode) && (OC_PASSTHRU != t1->opcode))
				return;
		t = bftrip->exorder.bl;									/* WARNING fallthrough */
	case OC_JMP:
	case OC_NOOP:
		dqdel(bftrip, exorder);
		bitrip->opcode = OC_NOOP;
		bitrip->operand[0].oprclass = NO_REF;
		tv = (OC_NOOP == t->opcode) ? TRUE : FALSE;
		if (OC_COMVAL == t0->opcode)
		{	/* we need an mval rather than a jmp, so clean up */
			dqdel(t, exorder);
			if (OCT_UNARY & oc_tab[t0->exorder.fl->opcode].octype)
			{	/* a unary has this as an operand, so use that rather than this OC_COMVAL as new value */
				assert(OC_COBOOL != t0->exorder.fl->opcode);
				assert(t0->exorder.fl->operand[0].oprval.tref == t0);
				assert(NO_REF == t->operand[1].oprclass);
				i->oprval.tref = t = t0->exorder.fl;
				dqdel(t0, exorder);
				if (OC_COM == t->opcode)
					tv ^= 1;						/* complement */
				else if (tv && (OC_NEG == t->opcode))
				{	/* better to do the negation now */
					v = (mval *)mcalloc(SIZEOF(mval));
					*v = literal_minusone;
					ENSURE_STP_FREE_SPACE(v->str.len);
					memcpy((char *)stringpool.free, v->str.addr, v->str.len);
					v->str.addr = (char *)stringpool.free;
					stringpool.free += v->str.len;
					t->opcode = OC_LIT;
					put_lit_s(v, t);
					break;
				}
			} else
				i->oprval.tref = t = t0;
		} else if (OC_COMINT == t2->opcode)
		{
			dqins(t, exorder, t2);
			dqdel(t, exorder);
			i->oprval.tref = t = t2;
		}
		PUT_LITERAL_TRUTH(tv, t);							/* turn known result into literal */
		t->opcode = OC_LIT;
		break;
	case OC_JMPEQU:
	case OC_JMPNEQ:
		assert((OC_COBOOL != t->exorder.bl->opcode)					/* only non literals remain */
			|| (OC_LIT != t->exorder.bl->operand[0].oprval.tref->opcode));
		break;
	case OC_JMPTCLR:									/* can't optimize these */
	case OC_JMPTSET:
	default:
		break;
	}
	return;
}
