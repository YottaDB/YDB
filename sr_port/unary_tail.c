/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
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
#include "stringpool.h"
#include "toktyp.h"

LITREF octabstruct	oc_tab[];

error_def(ERR_NUMOFLOW);

void unary_tail(oprtype *opr)
{	/* collapse any string of unary operators that cam be simplified
	 * opr is a pointer to a operand structure to process
	 */
	int		com, comval, drop, neg, num;
	mval		*mv, *v;
	opctype		c, c1, c2, pending_coerce;
	triple		*t, *t1, *t2, *ta, *ref0;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TRIP_REF == opr->oprclass);
	t = ta = t2 = opr->oprval.tref;
	assert(OCT_UNARY & oc_tab[t->opcode].octype);
	assert((TRIP_REF == t->operand[0].oprclass) && (NO_REF == t->operand[1].oprclass));
	com = comval = drop = neg = num = 0;
	pending_coerce = OC_NOOP;
	do
	{
		t1 = t2;
		c1 = t1->opcode;
		assert(TRIP_REF == t1->operand[0].oprclass);
		assert((TRIP_REF == t1->operand[1].oprclass) || (NO_REF == t1->operand[1].oprclass));
		t2 = t1->operand[0].oprval.tref;
		c2 = t2->opcode;
		switch (c2)	/* this switch is on the opcode of the operand triple */
		{
			case OC_COM:
			case OC_COBOOL:
			case OC_COMVAL:
			case OC_FORCENUM:
			case OC_LIT:
			case OC_NEG:
			case OC_PARAMETER:/* the next bit of logic works on the opcode of the (current) operator triple */
				comval += (OC_COMVAL == c1);
				com += (OC_COM == c1);	/* use a count to know if there were any and whether an even or odd # */
				num += (OC_FORCENUM == c1);	/* use a count to know if there were any numeric coersions */
				if (OC_NEG == c1)	/* get rid of all negation for literals; all but 1 for other exprs */
				{
					num++;		/* num indicates a coerce if the target is not a literal */
					neg = com ? neg : !neg;		/* a complement makes any right-hand negation irrelevant */
				} else if (OC_NOOP == pending_coerce && ((OC_COMVAL == c1) || (OC_COBOOL == c1)))
				{	/* Delete this coercion; we may add it back later */
					pending_coerce = c1;
				} else if ((OC_NOOP != c1) && !(OCT_UNARY & oc_tab[c1].octype))
					break;
				assert((NO_REF == t1->operand[1].oprclass) && ((NO_REF == t2->operand[1].oprclass)
					|| ((INDR_REF == t2->operand[1].oprclass) && (OC_LIT == c2))));
				drop++;			/* use a count of potential "extra" operands */
				if (OC_LIT != c2)
					continue;	/* this keeps the loop going */
				for (t1 = t, c = t1->opcode; 0 < drop; drop--)	/* reached a literal - time to stop */
				{	/* if it's a literal, delete all but one leading unary op and operate on the literal now */
					assert(OCT_UNARY & oc_tab[c].octype);
					t2 = t1->operand[0].oprval.tref;
					t1->opcode = c = t2->opcode;	/* slide next opcode & operand back before deleting it */
					t1->operand[0] = t2->operand[0];
					if (TREF(expr_start_orig) != t2)
						dqdel(t2, exorder);
					else
					{	/* if it's anchoring the expr_start chain, must NOOP it rather than delete it */
						t2->opcode = OC_NOOP;
						t2->operand[0].oprclass = NO_REF;
					}
				}
				assert(OC_LIT == c);
				t = ta;
				assert(MLIT_REF == t1->operand[0].oprclass);
				v = &t1->operand[0].oprval.mlit->v;
				MV_FORCE_NUMD(v);
				if (OC_NOOP != pending_coerce && OC_COMVAL != pending_coerce)
				{	/* add back a coercion if we need it;
					 * a COMVAL preceding a LIT is unneeded (and will cause an error).
					 * this leaves a COBOOL but removing earier and readding here is easier to follow & cleaner
					 */
					ref0 = maketriple(pending_coerce);
					dqins(t, exorder, ref0);
					ref0->operand[0].oprclass = TRIP_REF;
					ref0->operand[0].oprval.tref = t;
					opr->oprval.tref = ref0;
				}
				if (num || com)
				{	/* num includes both OC_NEG and OC_FORCENUM */
					unuse_literal(v);
					t->opcode = OC_LIT;
				}
				if (com)
				{	/* any complement reduces the literal value to [unsigned] 1 or 0 */
					PUT_LITERAL_TRUTH((!(1 & com) ? MV_FORCE_BOOL(v) : !MV_FORCE_BOOL(v)), t);
					v = &t->operand[0].oprval.mlit->v;
					if (neg)
						unuse_literal(v);	/* negate below replaces the literal just added above */
				}
				assert(1 >= neg);
				if (num)	/* if an "outer" OC_NEG or OC_FORCENUM, get it over with at compile time */
				{
					mv = (mval *)mcalloc(SIZEOF(mval));
					*mv = *v;
					if (neg)
					{	/* logic below is a bit less efficient than run time assembler op_neg, but in C */
						if (MV_INT & mv->mvtype)	/* code, we aren't set up to call assembler code */
						{
							if (0 != mv->m[1])
								mv->m[1] = -v->m[1];
							else
								mv->sgn = 0;
						} else if (MV_NM & mv->mvtype)
							mv->sgn = !mv->sgn;
					} else
					{
						s2n(mv);
						if (!(MV_NM & mv->mvtype))
						{
							TREF(last_source_column) += (TK_EOL == TREF(director_token)) ? -2 : 2;
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NUMOFLOW);
							assert(TREF(rts_error_in_parse));
							return;
						}
					}
					n2s(mv);
					v = mv;
					put_lit_s(v, t);
				}
				break;
			case OC_NOOP:
				if (OC_COMVAL == t->opcode)
				{
					t->opcode = OC_NOOP;
					t->operand[0].oprclass = NO_REF;
					break;
				}
			default:	/* something other than a unary operator or a literal - stop and do what can be done */
				if (OCT_ARITH & oc_tab[c2].octype)
				{	/* some arithmetic */
					if (OC_LIT == t2->opcode)
					{
						c2 = t2->opcode;
						assert(t2 != t);
						t2 = t;						/* start the check over */
						com = drop = neg = num = 0;
						continue;
					} else
						assert(c2 == t2->opcode);
				}
				break;		/* don't try anything fancy if the leaf is complex */
		}
		return;
	} while (TRUE);		/* the loop is managed by breaks and a continue statement above */
}
