/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright (c) 2001-2020 Fidelity National Information	*
=======
 * Copyright (c) 2001-2023 Fidelity National Information	*
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
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

<<<<<<< HEAD
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
=======
error_def(ERR_NUMOFLOW);

void ex_tail(oprtype *opr, boolean_t is_boolchild, boolean_t parent_comval)
/* Traverse the triple tree in post-order, working non-leaf operands toward final form
 * calls ex_arithlit to do arthimetic on literals at compile time, bx_boollit to optimize
 * certain boolean literals, and bx_tail to replace boolean ops with jump chains.
 * Its callees have certain expectations that they in the past enforced by calling each other and ex_tail recursively;
 * they are now leaves that rely on this function to transport them down the tree in the correct order and maintain certain
 * invariants documented here. Each of the functions expects all of the work of all the other functions to have
 * happened already on all operands of the triple in question. This is guaranteed by the traversal order here. The exception
 * is bx_tail, which is to be performed only on the topmost boolean in a directly-nested boolean expression. In the limiting case
 * of a boolean expression with no nesting, it is processed then and there, and it follows that all boolean operations that
 * descend from the current triple will already have been processed when the recursive ex_tail invocation returns, except for
 * when the current triple is a boolean, and in that case only for those booleans that are part of an uninterrupted chain of
 * boolean children. This is guaranteed by maintaining the is_boolchild flag parameter, true iff the parent triple is a boolean.
 * bx_tail expects not to encounter unsimplified chains of unary operators except for OC_COMs, which is guaranteed by the
 * placement of UNARY_TAIL. The parent_comval parameter allows us to avoid processing COBOOLS as jump chains when they will
 * be simplified by the caller.
 */
{
	opctype		c;
	oprtype		*i;
	triple		*bftrip, *bitrip, *t, *t0, *t1, *t2;
	uint		j, oct;
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TRIP_REF == opr->oprclass);
<<<<<<< HEAD
	UNARY_TAIL(opr, depth); /* this is first because it can change opr and thus whether we should even process the tail */
=======
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
	RETURN_IF_RTS_ERROR;
	CHKTCHAIN(TREF(curtchain), exorder, TRUE);	/* defined away in mdq.h except with DEBUG_TRIPLES */
	t = opr->oprval.tref; /* Refind t since UNARY_TAIL may have shifted it */
	c = t->opcode;
	oct = oc_tab[c].octype;
	if ((OCT_EXPRLEAF & oct) || (OC_NOOP == c))
		return;
	assert(TRIP_REF == t->operand[0].oprclass);
	assert((TRIP_REF == t->operand[1].oprclass) || (NO_REF == t->operand[1].oprclass));
	for (i = t->operand, j = 0; ARRAYTOP(t->operand) > i; i++, j++)
	{
<<<<<<< HEAD
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
=======
		if (TRIP_REF == i->oprclass)
		{
			ex_tail(i, (OCT_BOOL & oct), (OC_COMVAL == c));			/* chained Boolean or arithmetic */
			RETURN_IF_RTS_ERROR;
		}
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
	}
	if (OCT_ARITH & oct)
		ex_arithlit(t);
	/* the following code deals with Booleans where the expression is not directly managing flow - those go through bool_expr */
<<<<<<< HEAD
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
				if ((OC_LT == t->opcode) || (OC_NLT == t->opcode))
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
			case OC_LT:
			case OC_NLT:
				if (num_coms % 2)
					new_opcode = ((OC_LT == t->opcode) ? OC_NLT_RETMVAL : OC_LT_RETMVAL);
				else
					new_opcode = ((OC_LT == t->opcode) ? OC_LT_RETMVAL : OC_NLT_RETMVAL);
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
=======
	RETURN_IF_RTS_ERROR;
	UNARY_TAIL(opr);
	RETURN_IF_RTS_ERROR;
	t = opr->oprval.tref;
	c = t->opcode;
	oct = oc_tab[c].octype;
	if ((OCT_BOOL & oct))
	{
		if (!(OCT_UNARY & oct)) /* Boollit ought eventually to be made to operate on OC_COM */
		{
			bx_boollit(t);
			RETURN_IF_RTS_ERROR;
			c = t->opcode;
			oct = oc_tab[c].octype;
		}
		/* While post-order traversal is the cleanest and most maintainable solution for almost everything this function
		 * does, it doesn't neatly fit our boolean simplification, which turns directly nested booleans, e.g. x&(y&z) into
		 * single jump chains. This saves some instructions, but it requires us not to start jump chains for booleans that
		 * aren't going to have their own boolinit/fini anyway. (Which makes it impossible to handle the operands first in
		 * every case. To deal with this, the wait_for_parent variable tracks whether or not we are called on an operand
		 * of a boolean. If we are, and that operand is itself a genuine boolean operation (as opposed to a cobool or
		 * something similar), then hold off on creating the chain as we will end up inside the one created by the caller
		 * anyway. Finally, we also need to avoid creating boolchains for operands which will be immediately converted
		 * to mvals.
		 **/
		if (!is_boolchild && (OCT_BOOL & oct) && !(OC_COBOOL == c && parent_comval))
		{
			bitrip = bx_startbool(t);
			t0 = t->exorder.fl;
			assert((OC_COMVAL == t0->opcode) || (OC_COMINT == t0->opcode));
			assert(&t0->operand[0] == opr);				/* check next operation ensures an expression */
			bftrip = maketriple(OC_BOOLFINI);
			DEBUG_ONLY(bftrip->src = t->src);
			bftrip->operand[0] = put_tref(bitrip);
			opr->oprval.tref = bitrip;
			dqins(t, exorder, bftrip);
			i = (oprtype *)mcalloc(SIZEOF(oprtype));
			bx_tail(t, FALSE, i);
			RETURN_IF_RTS_ERROR;
			/* after bx_tail/bx_boolop it's safe to delete any OC_COMINT left */
			if (OC_COMINT == (t0 = bftrip->exorder.fl)->opcode)
				dqdel(t0, exorder);
			*i = put_tnxt(bftrip);
			CHKTCHAIN(TREF(curtchain), exorder, TRUE);	/* defined away in mdq except with DEBUG_TRIPLES */
		}
	}
	else if ((OC_COMINT == c) && (OC_BOOLINIT == (t0 = t->operand[0].oprval.tref)->opcode))
		opr->oprval.tref = t0;
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
	return;
}
