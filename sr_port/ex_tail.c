/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
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
	t2 = t->exorder.fl;
	assert((OC_COMVAL == t2->opcode) || (OC_COMINT == t2->opcode));	/* may need to change COMINT to COMVAL in bx_boolop */
	assert(&t2->operand[0] == opr);				/* check next operation ensures an expression */
	assert(TRIP_REF == t2->operand[1].oprclass);
	/* Check if OC_EQU/OC_NEQU/OC_CONTAIN/OC_NCONTAIN etc. can be optimized (YDB#777 and YDB#1091).
	 *
	 * Don't do this in case any side effects were seen and boolean short circuiting is not in effect (hence the
	 * OK_TO_SHORT_CIRCUIT usage below). This is because "bx_boolop()" (which is recursively called from the "ex_tail()"
	 * call below) plays with the triple chains (replacing OC_COMVAL with OC_PASSTHRU etc.) in case OK_TO_SHORT_CIRCUIT
	 * is FALSE and is not aware of the triple manipulations happening here (dqdel(), t->opcode changes etc.) as part
	 * of the optimization. See https://gitlab.com/YottaDB/DB/YDB/-/merge_requests/1544#note_2010673163 for an example
	 * that fails otherwise.
	 *
	 * One can try and make "bx_boolop()" aware of this YDB#777 or YDB#1091 optimization and make the two work together
	 * but it is more work and not considered worth it as the use cases that would benefit from it don't happen in
	 * practice in my understanding. Therefore it is left as an exercise for the future --- nars -- 2024/07/23.
	 */
	if (OK_TO_SHORT_CIRCUIT)
	{
		boolean_t	optimizable;
		int		num_coms;

		optimizable = TRUE;
		num_coms = 0;
		if (OC_COM == t->opcode)
		{
			t1 = t;
			for ( ; ; )
			{
				assert(OC_COM == t->opcode);
				num_coms++;
				t = t->operand[0].oprval.tref;
				if ((OC_EQU == t->opcode) || (OC_NEQU == t->opcode))
					break;
				if ((OC_CONTAIN == t->opcode) || (OC_NCONTAIN == t->opcode))
					break;
				if ((OC_FOLLOW == t->opcode) || (OC_NFOLLOW == t->opcode))
					break;
				if ((OC_PATTERN == t->opcode) || (OC_NPATTERN == t->opcode))
					break;
				if ((OC_SORTSAFTER == t->opcode) || (OC_NSORTSAFTER == t->opcode))
					break;
				if ((OC_GT == t->opcode) || (OC_NGT == t->opcode))
					break;
				if (OC_COM != t->opcode)
				{
					optimizable = FALSE;
					t = t1;	/* Reset "t" since we now know YDB#777 optimization is not possible */
					break;
				}
			}
		}
		if (optimizable)
		{
			opctype		new_opcode;
			boolean_t	is_equnul;

			is_equnul = FALSE;
			switch(t->opcode)
			{
			case OC_EQU:
			case OC_NEQU:;
				oprtype	*t_opr;
				int	j;

				if (num_coms % 2)
					new_opcode = ((OC_EQU == t->opcode) ? OC_NEQU_RETMVAL : OC_EQU_RETMVAL);
				else
					new_opcode = ((OC_EQU == t->opcode) ? OC_EQU_RETMVAL : OC_NEQU_RETMVAL);
				for (t_opr = t->operand, j = 0; t_opr < ARRAYTOP(t->operand); t_opr++, j++)
				{
					if ((TRIP_REF == t_opr->oprclass) && (OC_LIT == t_opr->oprval.tref->opcode)
						&& (0 == t_opr->oprval.tref->operand[0].oprval.mlit->v.str.len))
					{
						/* OC_EQU for [x=""] or [""=x] OR OC_NEQU for [x'=""] or [""'=x] can be optimized
						 * to OC_EQUNUL_RETMVAL/OC_NEQUNUL_RETMVAL opcode (i.e. without any surrounding
						 * OC_BOOLINIT/OC_BOOLFINI opcodes). [YDB#777]
						 */
						t2->operand[1].oprclass = NO_REF;
						t2->operand[0] = t->operand[1-j];
						is_equnul = TRUE;
						if (num_coms % 2)
							new_opcode =
								((OC_EQU == t->opcode) ? OC_NEQUNUL_RETMVAL : OC_EQUNUL_RETMVAL);
						else
							new_opcode =
								((OC_EQU == t->opcode) ? OC_EQUNUL_RETMVAL : OC_NEQUNUL_RETMVAL);
						break;
					}
				}
				break;
			case OC_CONTAIN:
			case OC_NCONTAIN:
				if (num_coms % 2)
					new_opcode = ((OC_CONTAIN == t->opcode) ? OC_NCONTAIN_RETMVAL : OC_CONTAIN_RETMVAL);
				else
					new_opcode = ((OC_CONTAIN == t->opcode) ? OC_CONTAIN_RETMVAL : OC_NCONTAIN_RETMVAL);
				break;
			case OC_FOLLOW:
			case OC_NFOLLOW:
				if (num_coms % 2)
					new_opcode = ((OC_FOLLOW == t->opcode) ? OC_NFOLLOW_RETMVAL : OC_FOLLOW_RETMVAL);
				else
					new_opcode = ((OC_FOLLOW == t->opcode) ? OC_FOLLOW_RETMVAL : OC_NFOLLOW_RETMVAL);
				break;
			case OC_PATTERN:
			case OC_NPATTERN:
				if (num_coms % 2)
					new_opcode = ((OC_PATTERN == t->opcode) ? OC_NPATTERN_RETMVAL : OC_PATTERN_RETMVAL);
				else
					new_opcode = ((OC_PATTERN == t->opcode) ? OC_PATTERN_RETMVAL : OC_NPATTERN_RETMVAL);
				break;
			case OC_SORTSAFTER:
			case OC_NSORTSAFTER:
				if (num_coms % 2)
					new_opcode = ((OC_SORTSAFTER == t->opcode)
							? OC_NSORTSAFTER_RETMVAL : OC_SORTSAFTER_RETMVAL);
				else
					new_opcode = ((OC_SORTSAFTER == t->opcode)
							? OC_SORTSAFTER_RETMVAL : OC_NSORTSAFTER_RETMVAL);
				break;
			case OC_GT:
			case OC_NGT:
				if (num_coms % 2)
					new_opcode = ((OC_GT == t->opcode) ? OC_NGT_RETMVAL : OC_GT_RETMVAL);
				else
					new_opcode = ((OC_GT == t->opcode) ? OC_GT_RETMVAL : OC_NGT_RETMVAL);
				break;
			default:
				optimizable = FALSE;
				break;
			}
			if (optimizable)
			{
				t2->opcode = new_opcode;
				CHECK_AND_RETURN_IF_BOOLEXPRTOODEEP(depth + 1);
				if (is_equnul)
					ex_tail(&t2->operand[0], depth + 1);
				else
				{
					t2->operand[0] = t->operand[0];
					t2->operand[1] = t->operand[1];
					ex_tail(&t2->operand[0], depth + 1);
					ex_tail(&t2->operand[1], depth + 1);
				}
				dqdel(t, exorder);
				/* Finally check if we came to OC_EQU/OC_NEQU through a OC_COM sequence.
				 * If so, remove those triples by replacing them with a OC_NOOP opcode.
				 */
				if (num_coms)
				{
					for ( ; ; )
					{
						assert(OC_COM == t1->opcode);
						t1->opcode = OC_NOOP;
						t1 = t1->operand[0].oprval.tref;
						if (OC_COM != t1->opcode)
							break;
					}
				}
				CHKTCHAIN(TREF(curtchain), exorder, TRUE);
				return;
			}
		}
	}
	bitrip = maketriple(OC_BOOLINIT);
	DEBUG_ONLY(bitrip->src = t->src);
	t1 = bool_return_leftmost_triple(t);
	dqrins(t1, exorder, bitrip);
	/* Overwrite depth (set in coerce.c to INIT_GBL_BOOL_DEPTH) to current bool expr depth */
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
