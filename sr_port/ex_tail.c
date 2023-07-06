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
#include "fullbool.h"
#include "stringpool.h"
#include "toktyp.h"
#include "flt_mod.h"

LITREF mval		literal_minusone, literal_one, literal_zero;
LITREF octabstruct	oc_tab[];

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
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TRIP_REF == opr->oprclass);
	RETURN_IF_RTS_ERROR;
	CHKTCHAIN(TREF(curtchain), exorder, TRUE);	/* defined away in mdq.h except with DEBUG_TRIPLES */
	t = opr->oprval.tref;
	c = t->opcode;
	oct = oc_tab[c].octype;
	if ((OCT_EXPRLEAF & oct) || (OC_NOOP == c))
		return;
	assert(TRIP_REF == t->operand[0].oprclass);
	assert((TRIP_REF == t->operand[1].oprclass) || (NO_REF == t->operand[1].oprclass));
	for (i = t->operand, j = 0; ARRAYTOP(t->operand) > i; i++, j++)
	{
		if (TRIP_REF == i->oprclass)
		{
			ex_tail(i, (OCT_BOOL & oct), (OC_COMVAL == c));			/* chained Boolean or arithmetic */
			RETURN_IF_RTS_ERROR;
		}
	}
	if (OCT_ARITH & oct)
		ex_arithlit(t);
	RETURN_IF_RTS_ERROR;
	/* the following code deals with Booleans where the expression is not directly managing flow - those go through bool_expr */
	UNARY_TAIL(opr);
	RETURN_IF_RTS_ERROR;
	t = opr->oprval.tref;
	c = t->opcode;
	oct = oc_tab[c].octype;
	if ((OCT_BOOL & oct))
	{
		if (!(OCT_UNARY & oct)) /* Boollit ought eventually to be made to operate on OC_COM */
		{
			if (EXT_BOOL == TREF(gtm_fullbool))
				CONVERT_TO_SE(t);
			bx_boollit(t);
			RETURN_IF_RTS_ERROR;
			c = t->opcode;
			oct = oc_tab[c].octype;
		}
		/* While post-order traversal is the cleanest and most maintainable solution for almost everything this function
		 * does, it doesn't neatly fit our boolean simplification, which turns directly nested booleans, e.g. x&(y&z) into
		 * single jump chains. This saves some instructions, but it requires us not to start jump chains for booleans that
		 * aren't going to have their own boolinit/fini anyway. (Which makes it impossible to handle the operands first in
		 * every case). To deal with this, the is_boolchild variable tracks whether or not we are called on an operand
		 * of a boolean. If we are, and that operand is itself a genuine boolean operation (as opposed to a cobool or
		 * something similar), then hold off on creating the chain as we will end up inside the one created by the caller
		 * anyway. Finally, we also need to avoid creating boolchains for operands which will be immediately converted
		 * to mvals.
		 **/
		assert(!(OC_COBOOL == c && parent_comval && OC_LIT != t->operand[0].oprval.tref->opcode));
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
	return;
}
