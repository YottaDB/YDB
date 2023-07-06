/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
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
#include "stringpool.h"
#include "toktyp.h"

LITREF octabstruct	oc_tab[];

error_def(ERR_NUMOFLOW);

<<<<<<< HEAD
void unary_tail(oprtype *opr, int depth)
=======
void unary_tail(oprtype *opr)
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
{	/* collapse any string of unary operators that can be simplified
	 * opr is a pointer to a operand structure to process
	 */
	int		com, comval, drop, neg, num;
	mval		*mv, *v;
	opctype		c, c1, c2, pending_coerce;
	triple		*t, *t1, *t2, *ta, *ref0;
	boolean_t	get_num;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TRIP_REF == opr->oprclass);
	t = ta = t2 = opr->oprval.tref;
	assert(OCT_UNARY & oc_tab[t->opcode].octype);
<<<<<<< HEAD
	assert((TRIP_REF == t->operand[0].oprclass)
		&& ((NO_REF == t->operand[1].oprclass) || (TRIP_REF == t->operand[1].oprclass)));
=======
	assert((TRIP_REF == t->operand[0].oprclass) && (NO_REF == t->operand[1].oprclass));
	switch (t->opcode)
	{
		case OC_COM:
		case OC_COBOOL:
		case OC_FORCENUM:
		case OC_NEG:
			get_num = TRUE;
			break;
		default:
			get_num = FALSE;
			break;
	}
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
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
			case OC_FORCENUM:
			case OC_NEG:
				get_num = TRUE;
			case OC_COMVAL:
			case OC_LIT:
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
<<<<<<< HEAD
				assert(((NO_REF == t1->operand[1].oprclass) || (TRIP_REF == t1->operand[1].oprclass))
					&& ((NO_REF == t2->operand[1].oprclass)
						|| (TRIP_REF == t2->operand[1].oprclass)
						|| ((INDR_REF == t2->operand[1].oprclass) && (OC_LIT == c2))));
				drop++;			/* use a count of potential "extra" operands */
				if (OC_LIT != c2)
					continue;	/* this keeps the loop going */
				assert(MLIT_REF == t2->operand[0].oprclass);
				v = &t2->operand[0].oprval.mlit->v;
				MV_FORCE_NUMD(v);
				if (!(MV_NM & v->mvtype))
				{	/* "s2n" would not set MV_NM if literal has a NUMOFLOW error.
					 * In that case, do not do any optimization.
					 */
					stx_error(ERR_NUMOFLOW);
					break;
				}
=======
				assert((NO_REF == t1->operand[1].oprclass) && ((NO_REF == t2->operand[1].oprclass)
							|| ((INDR_REF == t2->operand[1].oprclass) && (OC_LIT == c2))));
				drop++;			/* use a count of potential "extra" operands */
				if (OC_LIT != c2)
					continue;	/* this keeps the loop going */
				/* Any successful reduction should only be one-level-deep except for two cases described in the
				 * following assert. It would be possible to greatly reduce the complexity of this module if
				 * these cases could be avoided or dealt with in ex_tail.
				 */
				assert((t1 == t) || (OC_COBOOL == c1 && t1 == t->operand[0].oprval.tref
							&& (OC_COM == t->opcode || OC_COMVAL == t->opcode)));
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
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
<<<<<<< HEAD
				if ((OC_NOOP != pending_coerce) && (OC_COMVAL != pending_coerce))
=======
				assert(MLIT_REF == t1->operand[0].oprclass);
				if (OC_NOOP != pending_coerce && OC_COMVAL != pending_coerce)
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
				{	/* add back a coercion if we need it;
					 * a COMVAL preceding a LIT is unneeded (and will cause an error).
					 * this leaves a COBOOL but removing earlier and readding here is easier to follow & cleaner
					 */
					ref0 = maketriple(pending_coerce);
					dqins(t, exorder, ref0);
					ref0->operand[0].oprclass = TRIP_REF;
					ref0->operand[0].oprval.tref = t;
					opr->oprval.tref = ref0;
				}
				if (get_num)
				{
					v = &t1->operand[0].oprval.mlit->v;
					MV_FORCE_NUMD(v);
					if (!(MV_NM & v->mvtype))
					{
<<<<<<< HEAD
						s2n(mv);
						if (!(MV_NM & mv->mvtype))
						{
							TREF(last_source_column) += (TK_EOL == TREF(director_token)) ? -2 : 2;
							stx_error(ERR_NUMOFLOW);
							break;
						}
=======
						/* The correctness of this section depends on every unary operator along this path
						 * forcing a string-to-num conversion. The OC_NEG and FORCENUM do the conversions
						 * themselves and the mval2bool required by the others includes an s2n conversion
						 * that will cause an error if the converted-value numoverflows at runtime.
						 * Any changes to the kinds of operators which cause numoverflows here will require
						 * changes in appropriate runtime ops to keep this consistent.
						 **/
						TREF(last_source_column) += (TK_EOL == TREF(director_token)) ? -2 : 2;
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NUMOFLOW);
						assert(TREF(rts_error_in_parse));
						return;
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
						if (num)	/* forcenum or negate below replace the literal just added above */
							unuse_literal(v);
					}
					assert(1 >= neg);
					if (num)	/* if an "outer" OC_NEG or OC_FORCENUM, get it over with at compile time */
					{
						mv = (mval *)mcalloc(SIZEOF(mval));
						*mv = *v;
						if (neg)
						{
							if (MV_INT & mv->mvtype)
							{
								if (0 != mv->m[1])
									mv->m[1] = -v->m[1];
								else
									mv->sgn = 0;
							} else if (MV_NM & mv->mvtype)
								mv->sgn = !mv->sgn;
						}
						n2s(mv);
						v = mv;
						put_lit_s(v, t);
>>>>>>> 3c1c09f2 (GT.M V7.1-001)
					}
				}
				break;
			case OC_NOOP:
				if (OC_COMVAL == t->opcode)
				{
					t->opcode = OC_NOOP;
					t->operand[0].oprclass = NO_REF;
					break;
				}
			default:
				break;		/* don't try anything fancy if the leaf is complex */
		}
		return;
	} while (TRUE);		/* the loop is managed by breaks and a continue statement above */
}
