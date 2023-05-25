/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2023 YottaDB LLC and/or its subsidiaries. *
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
#include "is_equ.h"

LITREF octabstruct	oc_tab[];

error_def(ERR_NUMOFLOW);
error_def(ERR_PATNOTFOUND);

void bx_boollit(triple *t, int depth)
/* search the Boolean in t (recursively) for literal leaves; the logic is similar to bx_tail
 * the rest of the arguments parallel those in bx_boolop and used primarily handling basic Boolean operations (ON, NOR, AND, NAND)
 * to get the jump target and sense right for the left-hand operand of the operation
 * jmp_type_one gives the sense of the jump associated with the first operand
 * jmp_to_next gives whether we need a second jump to complete the operation
 * sense gives the sense of the requested operation
 * *addr points the operand for the jump and is eventually used by logic back in the invocation stack to fill in a target location
 */
{
	boolean_t	tv[ARRAYSIZE(t->operand)];
	int		dummy, j, tvr;
	mval		*mv, *v[ARRAYSIZE(t->operand)];
	opctype		c;
	oprtype		*opr, *p;
	triple		*ref0, *optrip[ARRAYSIZE(t->operand)];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(OCT_BOOL & oc_tab[t->opcode].octype);
	assert((OC_BOOLINIT != t->opcode) || (NO_REF == t->operand[0].oprclass));
	if (OC_BOOLINIT == t->opcode)
		return;
	assert(TRIP_REF == t->operand[0].oprclass);
	assert(((OC_COBOOL != t->opcode) && (OC_COM != t->opcode))
		|| (TRIP_REF == t->operand[1].oprclass));
	for (opr = t->operand, j = 0; opr < ARRAYTOP(t->operand); opr++, j++)
	{	/* checkout an operand to see if we can simplify it */
		p = opr;
		for (optrip[j] = opr->oprval.tref; OCT_UNARY & oc_tab[(c = optrip[j]->opcode)].octype; optrip[j] = p->oprval.tref)
		{	/* find the real object of affection; WARNING assignment above */
			p = &optrip[j]->operand[0];
		}
		if (OCT_ARITH & oc_tab[c].octype)
			ex_tail(p, depth);			/* chained arithmetic */
		else if (OCT_BOOL & oc_tab[c].octype)
			bx_boollit(optrip[j], depth);
		RETURN_IF_RTS_ERROR;
		assert(OC_COMVAL != optrip[j]->opcode);
		UNARY_TAIL(opr, depth);
		for (ref0 = t->operand[j].oprval.tref; OCT_UNARY & oc_tab[ref0->opcode].octype; ref0 = ref0->operand[0].oprval.tref)
			;
		optrip[j] = ref0;
		if (OC_LIT == ref0->opcode)
		{
			int	com, neg, num, comval;

			v[j] = &ref0->operand[0].oprval.mlit->v;
			com = (OC_COM == t->operand[j].oprval.tref->opcode);
			neg = (OC_NEG == t->operand[j].oprval.tref->opcode);
			num = (OC_FORCENUM == t->operand[j].oprval.tref->opcode);
			comval = (OC_COMVAL == t->operand[j].oprval.tref->opcode);
			if (com || neg || num || comval)
			{	/* First check if literal corresponds to a number with a NUMOFLOW error.
				 * In that case, return right away as literal optimization cannot be performed.
				 */
				MV_FORCE_NUMD(v[j]);
				if (!(MV_NM & v[j]->mvtype))
				{
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NUMOFLOW);
					return;
				}
			}
			if (com)
			{       /* any complement reduces the literal value to [unsigned] 1 or 0 */
				unuse_literal(v[j]);
				tv[j] = !MV_FORCE_BOOL(v[j]);
				assert(ref0 == optrip[j]);
				PUT_LITERAL_TRUTH(tv[j], ref0);
				v[j] = &ref0->operand[0].oprval.mlit->v;
				MV_FORCE_NUMD(v[j]);
				num = 0;							/* any complement trumps num */
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
				{
					s2n(mv);
					assert((MV_NM & mv->mvtype)); /* Else we would have issued NUMOFLOW a few lines above */
				}
				n2s(mv);
				assert((MV_NM & mv->mvtype) && (MV_STR & mv->mvtype));
				v[j] = mv;
				assert(ref0 == optrip[j]);
				put_lit_s(v[j], ref0);
			}
			/* In the case of this one optimized but not other, remove all unary's except the first
			 * If the first is a COMVAL, remove it.
			 */
			if (comval)
			{
				dqdel(t->operand[j].oprval.tref, exorder);
				t->operand[j].oprval.tref = t->operand[j].oprval.tref->operand[0].oprval.tref;
			}
		}
	}
	assert(optrip[0] != optrip[1]);
	j = -1;
	if (OC_LIT == optrip[0]->opcode)						/* make j identify what literal if any */
	{       /* operand[0] (left) has been evaluated to a literal */
		j = 0;
		if (OC_LIT == optrip[1]->opcode)
			j = 2;
	} else if (OC_LIT == optrip[1]->opcode)
		j = 1;
	if (2 == j)
	{
		for (j = 0;  j < ARRAYSIZE(v); j++)
		{	/* both arguments are literals, so try the operation at compile time */
			v[j] = &optrip[j]->operand[0].oprval.mlit->v;
			tv[j] = MV_FORCE_BOOL(v[j]);
		}
		switch (t->opcode)
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
			tvr = (0 < numcmp(v[0], v[1]));
			break;
		case OC_NLT:
		case OC_LT:
			tvr = (0 > numcmp(v[0], v[1]));
			break;
		case OC_NPATTERN:
		case OC_PATTERN:
			if (TREF(xecute_literal_parse))
				return;
			tvr = !(*(uint4 *)v[1]->str.addr) ? do_pattern(v[0], v[1]) : do_patfixed(v[0], v[1]);
			if (ERR_PATNOTFOUND == TREF(source_error_found))
				return;
			break;
		case OC_NSORTS_AFTER:
		case OC_SORTS_AFTER:
			tvr = (0 < sorts_after(v[0], v[1]));
			break;
		default:
			assertpro(FALSE);
		}
		for (j = 0;  j < ARRAYSIZE(v); j++)
		{	/* finish cleaning up the original triples */
			/* We don't need any COBOOLs, and all the negatives/coms should be out */
			for (ref0 = t->operand[j].oprval.tref; ref0 != optrip[j]; ref0 = ref0->operand[0].oprval.tref)
			{
				/* Note that all OC_NEG/OC_COM/OC_FORCENUM opcodes should have been eliminated by now.
				 * But it is possible that a string with a NUMOFLOW is used as one of the operands in the
				 * boolean expression. If so, the above opcodes would not have been eliminated.
				 * An example is "write 0!((-(+(-("1E47")))))"
				 *
				 * It is still okay to remove the OC_NEG/OC_COM/OC_FORCENUM opcodes (if any are found) as part
				 * of the literal optimization since a NUMOFLOW error is going to be issued anyways as we are
				 * guaranteed to see an OC_RTERROR triple (added by the "UNARY_TAIL" macro call at the top
				 * of this file) at the end of the "curtchain" triple execution chain in this case (asserted
				 * by the ALREADY_RTERROR macro below).
				 */
				assert(((OC_NEG != ref0->opcode) && (OC_FORCENUM != ref0->opcode) && (OC_COM != ref0->opcode))
					|| ALREADY_RTERROR);
				dqdel(ref0, exorder);
				t->operand[j].oprval.tref = ref0->operand[0].oprval.tref;
			}
			unuse_literal(v[j]);
			optrip[j]->opcode = OC_NOOP;	/* shouldn't dqdel be safe? */
			optrip[j]->operand[0].oprclass = NO_REF;
		}
		t->operand[1].oprclass = NO_REF;
		if (OCT_NEGATED & oc_tab[t->opcode].octype)
			tvr = !tvr;
		ref0 = maketriple(OC_LIT);
		t->opcode = OC_COBOOL;
		t->operand[0] = put_tref(ref0);
		PUT_LITERAL_TRUTH(tvr, ref0);
		dqrins(t, exorder, ref0);
	}
	return;
}
