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
	oprtype		*p;
	triple		*t, *t1, *t2, *ta, *ref0;
	uint4		w;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TRIP_REF == opr->oprclass);
	t = ta = t2 = opr->oprval.tref;
	pending_coerce = OC_NOOP;
	assert(OCT_UNARY & oc_tab[t->opcode].octype);
	assert((TRIP_REF == t->operand[0].oprclass) && (NO_REF == t->operand[1].oprclass));
	com = comval = drop = neg = num = 0;
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
					dqdel(t2, exorder);
				}
				assert(OC_LIT == c);
				t = ta;
				v = &t1->operand[0].oprval.mlit->v;
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
					PUT_LITERAL_TRUTH((!(1 & com) ? (0 != v->m[1]) : (0 == v->m[1])), t);
					v = &t->operand[0].oprval.mlit->v;
					if (neg)
						unuse_literal(v);	/* need to negate the literal just added above */
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
							rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NUMOFLOW);
							assert(TREF(rts_error_in_parse));
							return;
						}
					}
					n2s(mv);
					v = mv;
					put_lit_s(v, t);
				}
				assert((MV_NM & v->mvtype) && (MV_STR & v->mvtype));
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
					ex_tail(&t1->operand[0]);
					RETURN_IF_RTS_ERROR;
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
				if (TRUE || !drop || (NO_REF != t2->operand[1].oprclass))
					break;		/* don't try anything fancy if the leaf is complex */
				assert(OCT_UNARY & oc_tab[c1].octype);
				drop++;
				if ((OC_NEG == c1) && !com)
					neg = !neg;
				num += (OC_FORCENUM == c1);
				com += (OC_COM == c1);
				comval += (OC_COMVAL == c1);
				if (!com)
				{
					if (!num)
					{	/* nothing going on */
						assert(drop <= 2);
						break;
					}
					/* numeric conversion only - just pick one and throw any others away */
					t1 = t->operand[0].oprval.tref;
					if (OC_COBOOL == t->opcode)
					{	/* keep Boolean processing happy by leaving one of these alone/behind */
						c = OC_COBOOL;
						if (neg)
						{
							t1 = t1->operand[0].oprval.tref;
							drop--;
						}
					} else
						c = neg ? OC_NEG : OC_FORCENUM;
					for (drop--; 0 < drop; t1 = t2, drop--)
					{	/* get rid of any extras we can */
						t2 = t1->operand[0].oprval.tref;
						dqdel(t1, exorder);
					}
					if (neg && (OC_COBOOL == c2))
					{	/* preserving both a COBOOL and a NEG */
						assert(t2 == t->operand[0].oprval.tref);
						t2->opcode = OC_NEG;
						t2->operand[0].oprval.tref = t1;
					} else
					{	/* just keeping one */
						assert(c2 == t2->opcode);
						t->operand[0].oprval.tref = t1;
						t->opcode = c;
					}
					break;
				}
				if ((comval ? 3 : 2) >= drop)
					break;	/* a single complement takes two or three OP codes so no need to waste time */
				if (comval && !(1 & com))
					break;				/* a hack for a case that we're out of time to work out */
				ta = t2 = t;	/* below deals with complement by picking thru & deleting extra triples */
				if (neg)	/* in the common case, most of the loops don't run even a single iteration */
				{	/* a negation outside the complement */
					for (t1 = t; OC_NEG != t1->opcode; t1 = t2, drop--)
					{
						if (!drop)
						{
							assert(0 < drop);
							break;
						}
						t2 = t1->operand[0].oprval.tref;
						if (t1 != t)
							dqdel(t1, exorder);
					}
					assert(drop);	/* there should be something going on within the negated expression */
					if (t2 != ta)
					{	/* we did drop something to get to this negation, so slide it to the "front" */
						t->operand[0].oprval.tref = t1->operand[0].oprval.tref;
						t->opcode = c = OC_NEG;
						dqdel(t1, exorder);
					}
					t1 = ta->operand[0].oprval.tref;
					drop--;
				} else
					t1 = t2 = ta;
				if (comval)
				{	/* with complement & expr (rather than tvexpr) we expect: COBOOL, COM, COMVAL sequence */
					for (; OC_COMVAL != t1->opcode; t1 = t2, drop--)
					{
						if (OC_COM == t1->opcode)
							break;
						if (3 >= drop)
							break;		/* need to deal with "final" COMVAL, COM and COBOOL */
						t2 = t1->operand[0].oprval.tref;
						if (t1 != t)
							dqdel(t1, exorder);
					}
				}
				t2 = t1->operand[0].oprval.tref;
				if (OC_COMVAL == t1->opcode)
				{	/* did find a COMVAL */
					if (t1 != t)
					{	/* if not already in the right place in the revised chain move it */
						if (!neg)
						{	/* no (outer) negation, so OC_COMVAL leads things off */
							assert(drop);
							t->operand[0].oprval.tref = t2;
							t->opcode = c = OC_COMVAL;
							dqdel(t1, exorder);
							t1 = t;
						} else
						{	/* move it up or possibly not - cheaper to do it whether needed or not */
							assert(ta != t1);
							ta->operand[0].oprval.tref = t1;
						}
					}
					ta = t1;
					drop--;
				}
				if (1 & com)
				{	/* need one complement */
					for (t1 = ta->operand[0].oprval.tref; OC_COM != t1->opcode; t1 = t2, drop--)
					{
						if (!drop)
						{
							assert(0 < drop);
							break;
						}
						t2 = t1->operand[0].oprval.tref;
						if (t1 != t)
							dqdel(t1, exorder);
					}
					t2 = t1->operand[0].oprval.tref; /* pick up the operand of what we may have just deleted */
					if (t1 != t)
					{
						if (!neg && (OC_COMVAL != ta->opcode))
						{	/* no (outer) negation or OC_COMVAL, so OC_COM leads things off */
							if (!drop)
							{
								assert(0 < drop);
								break;
							}
							t->operand[0].oprval.tref = t2;
							t->opcode = c = OC_COM;
							t->operand[0] = t1->operand[0];
							dqdel(t1, exorder);
							t1 = t;
						} else
						{	/* move it up or possibly not - cheaper to do it whether needed or not */
							assert(ta != t1);
							ta->operand[0].oprval.tref = t1;
						}
					}
					ta = t1;
					drop--;
				}
				for (t1 = ta->operand[0].oprval.tref; (OCT_UNARY & oc_tab[t1->opcode].octype) ; t1 = t2, drop--)
				{	/* a complement always requires a COBOOL but if the COMs cancelled out, it can go */
					if ((OC_COBOOL == t1->opcode) && (1 & com))
					{
						assert(0 < drop);
						drop--;
						break;
					}
					t2 = t1->operand[0].oprval.tref;
					if (t1 != t)
					{
						assert(ta != t1);
						ta->operand[0] = t1->operand[0];
						dqdel(t1, exorder);
					}
				}
				if (!(1 & com))
				{
					assert(c2 == t1->opcode);
					t->operand[0] = t1->operand[0];
					t->operand[1] = t1->operand[1];
					t->opcode = t1->opcode;
					dqdel(t1, exorder);
					break;
				}
				t2 = t1->operand[0].oprval.tref;
				assert(0 <= drop);
				if (drop && (t1 != t))
				{	/* still working toward the "actual" or "real" operand */
					assert(1 & com);
					if (!neg && (OC_COMVAL != ta->opcode) && (OC_COM != ta->opcode))
					{	/* no (outer) negation, OC_COMVAL or OC_COM - OC_COBOOL leads things off */
						assert(drop);
						t->operand[0].oprval.tref = t2;
						t->opcode = c = OC_COBOOL;
						dqdel(t1, exorder);
						t1 = t;
					} else
					{
						assert(ta != t1);
						ta->operand[0].oprval.tref = t1;
					}
				}
				ta = t1;
				for (t1 = ta->operand[0].oprval.tref; drop; t1 = t2, drop--)
				{	/* anything left is redundant, so get rid of it */
					if (TRIP_REF != t1->operand[0].oprclass)
						break;
					t2 = t1->operand[0].oprval.tref;
					dqdel(t1, exorder);
				}
				assert(c2 == t1->opcode);	/* check that 2nd pass appears to wind up same place as 1st */
				ta->operand[0].oprval.tref = t1;
				break;
		}
		return;
	} while (TRUE);		/* the loop is managed by breaks and a continue statement above */
}
