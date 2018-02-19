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
#include "toktyp.h"
#include "mdq.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "fullbool.h"
#include "stringpool.h"

GBLREF	triple		t_orig;

error_def(ERR_COLON);
error_def(ERR_SELECTFALSE);

LITREF octabstruct oc_tab[];

#define SELECT_CLEANUP					\
{							\
	free(TREF(side_effect_base));			\
	TREF(side_effect_base) = save_se_base;		\
	TREF(side_effect_depth) = save_se_depth;	\
	TREF(expr_depth) = save_expr_depth;		\
}

int f_select(oprtype *a, opctype op)
/* drive parsing for the $select function
 * a is an operand the caller places to access the result
 * op is actuallly an OC_PASSTHRU to anchor the list of Boolean controlled jumps around STOs of associated values
 * the return is TRUE for success and FALSE for a failure
 */
{
	boolean_t	first_time, got_true, save_saw_side, saw_se_in_select, *save_se_base, save_shift, shifting, throwing;
	opctype		old_op;
	oprtype		*cnd, endtrip, target, tmparg;
	triple		dmpchain, *loop_save_start, *loop_save_start_orig, *oldchain, *r, *ref, *savechain, *save_start,
			*save_start_orig, tmpchain, *triptr;
	uint4		save_expr_depth, save_se_depth;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	save_expr_depth = TREF(expr_depth);
	save_saw_side = TREF(saw_side_effect);
	save_shift = TREF(shift_side_effects);
	save_se_depth = TREF(side_effect_depth);
	save_se_base = TREF(side_effect_base);
	save_start = TREF(expr_start);
#	ifdef DEBUG
	if (NULL != save_start)
		CHKTCHAIN(save_start, exorder, FALSE);
#	endif
	save_start_orig = TREF(expr_start_orig);
	TREF(expr_start) = TREF(expr_start_orig) = NULL;
	TREF(expr_depth) = 0;
	TREF(saw_side_effect) = FALSE;
	TREF(shift_side_effects) = FALSE;
	TREF(side_effect_depth) = INITIAL_SIDE_EFFECT_DEPTH;
	TREF(side_effect_base) = malloc(SIZEOF(boolean_t) * TREF(side_effect_depth));
	memset((char *)(TREF(side_effect_base)), 0, SIZEOF(boolean_t) * TREF(side_effect_depth));
	if (shifting = (save_shift && (!save_saw_side || (GTM_BOOL == TREF(gtm_fullbool)))))	/* WARNING assignment */
	{
		dqinit(&tmpchain, exorder);
		oldchain = setcurtchain(&tmpchain);
		INCREMENT_EXPR_DEPTH;	/* Don't want to hit bottom with each expression, so start at 1 rather than 0 */
		TREF(expr_start) = TREF(expr_start_orig) = &tmpchain;
	}
	r = maketriple(op);
	first_time = TRUE;
	got_true = throwing = FALSE;
	endtrip = put_tjmp(r);
	cnd = NULL;
	savechain = NULL;
	for (;;)
	{
		if (NULL == cnd)
			cnd = (oprtype *)mcalloc(SIZEOF(oprtype));
		if (shifting && !got_true)
		{
			loop_save_start = TREF(expr_start);
			loop_save_start_orig = TREF(expr_start_orig);
		}
		if (!bool_expr(FALSE, cnd))
		{
			SELECT_CLEANUP;
			if (shifting)
				setcurtchain(oldchain);
			else if ((NULL != savechain)
					/* the below guards against returning to a chain that should
					 * be abandoned because of an error */
					&& (((TREF(curtchain)) != &t_orig) || (!ALREADY_RTERROR)))
				setcurtchain(savechain);
			return FALSE;
		}
		if (TK_COLON != TREF(window_token))
		{
			SELECT_CLEANUP;
			if (shifting)
				setcurtchain(oldchain);
			else if ((NULL != savechain)
					/* the below guards against returning to a chain that should
					 * be abandoned because of an error */
					&& (((TREF(curtchain)) != &t_orig) || (!ALREADY_RTERROR)))
				setcurtchain(savechain);
			stx_error(ERR_COLON);
			return FALSE;
		}
		advancewindow();
		for (triptr = (TREF(curtchain))->exorder.bl; OC_NOOP == triptr->opcode; triptr = triptr->exorder.bl)
			;	/* get back, get back to where we once belonged - to find an indicator of the actual result */
		if (!got_true && OC_LIT == triptr->opcode)
		{	/* it is a literal not following an already optimizing TRUE, so optimize it */
			dqdel(triptr, exorder);
			unuse_literal(&triptr->operand[0].oprval.mlit->v);
			dqinit(&dmpchain, exorder);
			if (0 == triptr->operand[0].oprval.mlit->v.m[1])
			{	/* it's FALSE: discard the corresponding value */
				throwing = TRUE;
				savechain = setcurtchain(&dmpchain);
			} else
			{	/* it's TRUE: take this argument and disregard any following */
				assert(!throwing && (NULL == savechain));
				got_true = TRUE;
			}
		}
		if (EXPR_FAIL == expr(&tmparg, MUMPS_EXPR))
		{
			SELECT_CLEANUP;
			if (shifting)
				setcurtchain(oldchain);
			else if ((NULL != savechain)
					/* the below guards against returning to a chain that should
					 * be abandoned because of an error */
					&& (((TREF(curtchain)) != &t_orig) || (!ALREADY_RTERROR)))
				setcurtchain(savechain);
			return FALSE;
		}
		assert(TRIP_REF == tmparg.oprclass);
		if (throwing)
		{	/* finished with the false argument so put things back to normal */
			assert(savechain);
			setcurtchain(savechain);
			if (shifting)
			{
				TREF(expr_start) = loop_save_start;
				TREF(expr_start_orig) = loop_save_start_orig;
			}
			savechain = NULL;
			throwing = FALSE;
			if (TK_COMMA != TREF(window_token))
				break;
			advancewindow();
			continue;
		}
		old_op = tmparg.oprval.tref->opcode;
		if (first_time)
		{
			first_time = FALSE;
			if (got_true && (OC_LIT == old_op))
			{	/* if this is the only possible result, NOOP the OC_PASSTHRU so tmparg becomes the return value */
				assert((OC_PASSTHRU == r->opcode) && (NO_REF == r->operand[0].oprclass));
				r = tmparg.oprval.tref;
			} else
			{
				if ((OC_LIT == old_op) || (oc_tab[old_op].octype & OCT_MVADDR))
				{
					ref = newtriple(OC_STOTEMP);
					ref->operand[0] = tmparg;
					tmparg = put_tref(ref);
				}
				r->operand[0] = target = tmparg;
			}
		} else if (OC_PASSTHRU == r->opcode)
		{
			ref = newtriple(OC_STO);
			ref->operand[0] = target;
			ref->operand[1] = tmparg;
			if (OC_PASSTHRU == old_op)
			{
				assert(TRIP_REF == tmparg.oprval.tref->operand[0].oprclass);
				ref = newtriple(OC_STO);
				ref->operand[0] = target;
				ref->operand[1] = put_tref(tmparg.oprval.tref->operand[0].oprval.tref);
			}
		}
		if (!got_true)
		{	/* jump to the end in case the run time value should turn out to be (the first) TRUE */
			ref = newtriple(OC_JMP);
			ref->operand[0] = endtrip;
			tnxtarg(cnd);
		}
		if (TK_COMMA != TREF(window_token))
			break;
		advancewindow();
		if (got_true && (NULL == savechain))
		{
			if (shifting)
			{
				loop_save_start = TREF(expr_start);
				loop_save_start_orig = TREF(expr_start_orig);
			}
			savechain = setcurtchain(&dmpchain);	/* discard arguments after a compile time TRUE */
		}
		cnd = NULL;
	}
	if (got_true)
	{
		if (shifting)
		{
			TREF(expr_start) = loop_save_start;
			TREF(expr_start_orig) = loop_save_start_orig;
		}
		if (NULL != savechain)
			setcurtchain(savechain);		/* if we discarded things, return the chain to normal */
	} else
	{	/* if we didn't find a TRUE at compile time, then insert a possible error in case there's no TRUE at run time */
		tmparg = put_ilit(ERR_SELECTFALSE);
		if (first_time)
		{	/* if all values were literals and FALSE, supply a dummy evaluation so we reach the error gracefully */
			PUT_LITERAL_TRUTH(FALSE, r);
			r->opcode = OC_LIT;
		}
		ref = newtriple(OC_RTERROR);
		ref->operand[0] = tmparg;
		ref->operand[1] = put_ilit(FALSE);		/* Not a subroutine reference */
	}
	if (OC_PASSTHRU == r->opcode)
		ins_triple(r);
	saw_se_in_select = TREF(saw_side_effect);		/* note this down before it gets reset by DECREMENT_EXPR_DEPTH */
	if (shifting)
		DECREMENT_EXPR_DEPTH;				/* clean up */
	assert(!TREF(expr_depth));
	TREF(expr_start) = save_start;
	TREF(expr_start_orig) = save_start_orig;
	TREF(shift_side_effects) = save_shift;
	save_se_base[save_expr_depth] |= (TREF(side_effect_base))[TREF(expr_depth)];
	TREF(saw_side_effect) = saw_se_in_select | save_saw_side;
	SELECT_CLEANUP;	/* restores TREF(expr_depth), TREF(side_effect_base) and TREF(side_effect_depth) */
	if (shifting)
	{
		shifting = ((1 < save_expr_depth) || ((save_start != save_start_orig) && (OC_NOOP != save_start->opcode)));
		newtriple(shifting ? OC_GVSAVTARG : OC_NOOP);	/* must have one of these two at expr_start */
		setcurtchain(oldchain);
		assert(NULL != save_start);
		dqadd(save_start, &tmpchain, exorder);
		save_start = tmpchain.exorder.bl;
		if (shifting)
		{	/* only play this game if something else started it */
			assert(OC_GVSAVTARG == save_start->opcode);
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(save_start);
		}
		TREF(expr_start) = save_start;
	}
	*a = put_tref(r);
	return TRUE;
}
