
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
#include "opcode.h"
#include "mdq.h"
#include "mmemory.h"
#include <emit_code.h>
#include "fullbool.h"
#include "matchc.h"
#include "numcmp.h"
#include "patcode.h"
#include "sorts_after.h"
#include "stringpool.h"

LITREF octabstruct	oc_tab[];

STATICFNDEF void bx_boollit_tail(triple *t, boolean_t jmp_type_one, boolean_t jmp_to_next, boolean_t sense, oprtype *addr);

void bx_boollit(triple *t, boolean_t sense, oprtype *addr)
{	/* an interface hybrid of bx_tail and bx_boolop that allows calls in and recursive calls */
	opctype		c;

	if (OCT_NEGATED & oc_tab[(c = t->opcode)].octype)				/* WARNING assignment */
		sense ^= 1;
	if ((OC_OR == c) || (OC_NOR == c))
		bx_boollit_tail(t, TRUE, !sense, sense, addr);
	else
		bx_boollit_tail(t, FALSE, sense, sense, addr);
}

void bx_boollit_tail(triple *t, boolean_t jmp_type_one, boolean_t jmp_to_next, boolean_t sense, oprtype *addr)
/* search the Boolean in t (recursively) for literal leaves; the logic is similar to bx_tail
 * the rest of the arguments parallel those in bx_boolop and used primarily handling basic Boolean operations (ON, NOR, AND, NAND)
 * to get the jump target and sense right for the left-hand operand of the operation
 * jmp_type_one gives the sense of the jump associated with the first operand
 * jmp_to_next gives whether we need a second jump to complete the operation
 * sense gives the sense of the requested operation
 * *addr points the operand for the jump and is eventually used by logic back in the invocation stack to fill in a target location
 */
{
	boolean_t	sin[ARRAYSIZE(t->operand)], tv[ARRAYSIZE(t->operand)];
	int		com, comval, dummy, j, neg, num, tvr;
	mval		*mv, *v[ARRAYSIZE(t->operand)];
	opctype		c;
	oprtype		*i, *p;
	triple		*cob[ARRAYSIZE(t->operand)], *ref0, *tl[ARRAYSIZE(t->operand)];

	assert(OCT_BOOL & oc_tab[t->opcode].octype);
	assert(TRIP_REF == t->operand[0].oprclass);
	assert((OC_COBOOL != t->opcode) && (OC_COM != t->opcode) || (TRIP_REF == t->operand[1].oprclass));
	for (i = t->operand, j = 0; i < ARRAYTOP(t->operand); i++, j++)
	{	/* checkout an operand to see if we can simplify it */
		p = i;
		com = 0;
		for (tl[j] = i->oprval.tref; OCT_UNARY & oc_tab[(c = tl[j]->opcode)].octype; tl[j] = p->oprval.tref)
		{	/* find the real object of affection; WARNING assignment above */
			assert((TRIP_REF == tl[j]->operand[0].oprclass) && (NO_REF == tl[j]->operand[1].oprclass));
			com ^= (OC_COM == c);	/* if we make a recursive call below, COM matters, but NEG and FORCENUM don't */
			p = &tl[j]->operand[0];
		}
		if (OCT_ARITH & oc_tab[c].octype)
			ex_tail(p);								/* chained arithmetic */
		else if (OCT_BOOL & oc_tab[c].octype)
		{	/* recursively check an operand */
			sin[j] = sense;
			p = addr;
			if (!j && !(OCT_REL & oc_tab[t->opcode].octype))
			{	/* left hand operand of parent */
				sin[j] = jmp_type_one;
				if (jmp_to_next)
				{	/* left operands need extra attention to decide between jump next or to the end */
					p = (oprtype *)mcalloc(SIZEOF(oprtype));
					*p = put_tjmp(t);
				}
			}
			bx_boollit(tl[j], sin[j] ^ com, p);
		}
		if ((OC_JMPTRUE != tl[j]->opcode) && (OC_JMPFALSE != tl[j]->opcode) && (OC_LIT != tl[j]->opcode))
			return;									/* this operation doesn't qualify */
		com = comval = neg = num = 0;
		cob[j] = NULL;
		for (ref0 = i->oprval.tref; OCT_UNARY & oc_tab[(c = ref0->opcode)].octype; ref0 = ref0->operand[0].oprval.tref)
		{       /* we may be able to clean up this operand; WARNING assignment above */
			assert((TRIP_REF == ref0->operand[0].oprclass) && (NO_REF == ref0->operand[1].oprclass));
			num += (OC_FORCENUM == c);
			com += (OC_COM == c);
			if (!com)								/* "outside" com renders neg mute */
				neg ^= (OC_NEG == c);
			if (!comval && (NULL == cob[j]))
			{
				if (comval = (OC_COMVAL == c))					/* WARNING assignment */
				{
					if (ref0 != t->operand[j].oprval.tref)
						dqdel(t->operand[j].oprval.tref, exorder);
					t->operand[j].oprval.tref = tl[j];			/* need mval: no COBOOL needed */
				}
				else if (OC_COBOOL == c)
				{	/* the operand needs a COBOOL in case its operator remains unresolved */
					cob[j] = t->operand[j].oprval.tref;
					if (ref0 == cob[j])
						continue;					/* already where it belongs */
					cob[j]->opcode = OC_COBOOL;
					cob[j]->operand[0].oprval.tref = tl[j];
				} else if (ref0 == t->operand[j].oprval.tref)
					continue;
			}
			dqdel(ref0, exorder);
		}
		assert(ref0 == tl[j]);
		if (!comval && (NULL == cob[j]) && (tl[j] != t->operand[j].oprval.tref))
		{	/* left room for a COBOOL, but there's no need */
			dqdel(t->operand[j].oprval.tref, exorder);
			t->operand[j].oprval.tref = tl[j];
		}
		if ((OC_JMPTRUE == ref0->opcode) || (OC_JMPFALSE == ref0->opcode))
		{	/* switch to a literal representation of TRUE / FALSE */
			assert(INDR_REF == ref0->operand[0].oprclass);
			ref0->operand[1] = ref0->operand[0];					/* track info as we switch opcode */
			PUT_LITERAL_TRUTH((sin[j] ? OC_JMPFALSE : OC_JMPTRUE) == ref0->opcode, ref0);
			ref0->opcode = OC_LIT;
			com = 0;								/* already accounted for by sin */
		}
		assert((OC_LIT == ref0->opcode) && (MLIT_REF == ref0->operand[0].oprclass));
		v[j] = &ref0->operand[0].oprval.mlit->v;
		if (com)
		{       /* any complement reduces the literal value to [unsigned] 1 or 0 */
			unuse_literal(v[j]);
			tv[j] = (0 == v[j]->m[1]);
			assert(ref0 == tl[j]);
			PUT_LITERAL_TRUTH(tv[j], ref0);
			v[j] = &ref0->operand[0].oprval.mlit->v;
			num = 0;								/* any complement trumps num */
		}
		if (neg || num)
		{	/* get literal into uniform state */
			unuse_literal(v[j]);
			mv = (mval *)mcalloc(SIZEOF(mval));
			*mv = *v[j];
			if (neg)
			{
				if (MV_INT & mv->mvtype)
				{
					if (0 != mv->m[1])
						mv->m[1] = -mv->m[1];
					else
						mv->sgn = 0;
				} else if (MV_NM & mv->mvtype)
					mv->sgn = !mv->sgn;
			} else
				s2n(mv);
			n2s(mv);
			v[j] = mv;
			assert(ref0 == tl[j]);
			put_lit_s(v[j], ref0);
		}
	}
	assert(tl[0] != tl[1]);									/* start processing a live one */
	for (tvr = j, j = 0;  j < tvr; j++)
	{	/* both arguments are literals, so do the operation at compile time */
		if (NULL != cob[j])
			dqdel(cob[j], exorder);
		v[j] = &tl[j]->operand[0].oprval.mlit->v;
		tv[j] = (0 != v[j]->m[1]);
		unuse_literal(v[j]);
		tl[j]->opcode = OC_NOOP;
		tl[j]->operand[0].oprclass = NO_REF;
	}
	t->operand[1].oprclass = NO_REF;
	switch (c = t->opcode)									/* WARNING assignment */
	{	/* optimize the Boolean operations here */
		case OC_NAND:
		case OC_AND:
			tvr = (tv[0] && tv[1]);
			break;
		case OC_NOR:
		case OC_OR:
			tvr = (tv[0] || tv[1]);
			break;
		case OC_NCONTAIN:
		case OC_CONTAIN:
			tvr = 1;
			(void)matchc(v[1]->str.len, (unsigned char *)v[1]->str.addr, v[0]->str.len,
				(unsigned char *)v[0]->str.addr, &dummy, &tvr);
			tvr ^= 1;
			break;
		case OC_NEQU:
		case OC_EQU:
			tvr = is_equ(v[0], v[1]);
			break;
		case OC_NFOLLOW:
		case OC_FOLLOW:
			tvr = 0 < memvcmp(v[0]->str.addr, v[0]->str.len, v[1]->str.addr, v[1]->str.len);
			break;
		case OC_NGT:
		case OC_GT:
			tvr = 0 < numcmp(v[0], v[1]);
			break;
		case OC_NLT:
		case OC_LT:
			tvr = 0 > numcmp(v[0], v[1]);
			break;
		case OC_NPATTERN:
		case OC_PATTERN:
			tvr = !(*(uint4 *)v[1]->str.addr) ? do_pattern(v[0], v[1]) : do_patfixed(v[0], v[1]);
			break;
		case OC_NSORTS_AFTER:
		case OC_SORTS_AFTER:
			tvr = 0 < sorts_after(v[0], v[1]);
			break;
		default:
			assertpro(FALSE);
	}
	tvr ^= !sense;
	t->operand[0] = put_indr(addr);
	t->opcode = tvr ? OC_JMPFALSE : OC_JMPTRUE;
	return;
}
