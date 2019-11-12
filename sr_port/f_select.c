/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2020 YottaDB LLC and/or its subsidiaries.	*
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
MBSTART {						\
	free(TREF(side_effect_base));			\
	TREF(side_effect_base) = save_se_base;		\
	TREF(side_effect_depth) = save_se_depth;	\
	TREF(expr_depth) = save_expr_depth;		\
	TREF(shift_side_effects) = save_shift;		\
	TREF(expr_start) = save_start;			\
	TREF(expr_start_orig) = save_start_orig;	\
} MBEND

int f_select(oprtype *a, opctype op)
/* drive parsing for the $select function
 * a is an operand the caller places to access the result
 * op is actuallly an OC_PASSTHRU to anchor the list of Boolean controlled jumps around STOs of associated values
 * the return is TRUE for success and FALSE for a failure
 */
{
	boolean_t	first_time, got_true, save_saw_side, *save_se_base, save_shift, saw_se_in_select, shifting, throwing;
	opctype		old_op;
	oprtype		*cnd, endtrip, target, tmparg;
	triple		dmpchain, *loop_save_start, *loop_save_start_orig, *oldchain, *r, *ref, *savechain, *save_start,
			*save_start_orig, tmpchain, *triptr, *noop;
	triple		*boolexprfinish, *boolexprfinish2;
	uint4		save_expr_depth, save_se_depth;
	mval		*v;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	save_saw_side = TREF(saw_side_effect);
	save_start_orig = TREF(expr_start_orig);
	save_start = TREF(expr_start);
	save_shift = TREF(shift_side_effects);
	save_expr_depth = TREF(expr_depth);
	save_se_depth = TREF(side_effect_depth);
	save_se_base = TREF(side_effect_base);
#	ifdef DEBUG
	if (NULL != save_start)
		CHKTCHAIN(save_start, exorder, FALSE);
	oldchain = TREF(curtchain);
#	endif
	TREF(expr_start) = TREF(expr_start_orig) = NULL;
	TREF(expr_depth) = 0;
	TREF(saw_side_effect) = FALSE;
	TREF(shift_side_effects) = FALSE;
	TREF(side_effect_depth) = INITIAL_SIDE_EFFECT_DEPTH;
	TREF(side_effect_base) = malloc(SIZEOF(boolean_t) * TREF(side_effect_depth));
	memset((char *)(TREF(side_effect_base)), 0, SIZEOF(boolean_t) * TREF(side_effect_depth));
<<<<<<< HEAD
	if (shifting = (save_shift && (!save_saw_side || (YDB_BOOL == TREF(ydb_fullbool)))))	/* WARNING assignment */
	{
=======
	if (shifting = (save_shift && (!save_saw_side || (GTM_BOOL == TREF(gtm_fullbool)))))	/* WARNING assignment */
	{	/* shift in progress or needed so use a temporary chain */
>>>>>>> 3d3cd0dd... GT.M V6.3-010
		dqinit(&tmpchain, exorder);
		oldchain = setcurtchain(&tmpchain);
		INCREMENT_EXPR_DEPTH;	/* Don't want to hit bottom with each expression, so start at 1 rather than 0 */
		TREF(expr_start) = TREF(expr_start_orig) = &tmpchain;
	}
	r = maketriple(op);
	noop = maketriple(OC_NOOP);	/* This is the jump target. Need this to be separate from `r`. Finally before inserting
					 * `r` in the triple chain, we first insert `noop`. This lets the caller `bool_expr()`
					 * insert OC_BOOLEXPRSTART triples BEFORE `r` without unbalanced OC_BOOLEXPRFINISH issues.
					 */
	first_time = TRUE;
	got_true = throwing = FALSE;
<<<<<<< HEAD
	endtrip = put_tjmp(noop);
	cnd = NULL;
=======
	endtrip = put_tjmp(r);
>>>>>>> 3d3cd0dd... GT.M V6.3-010
	savechain = NULL;
	for (;;)
	{
		cnd = (oprtype *)mcalloc(SIZEOF(oprtype));
		if (shifting)
		{	/* be ready to back out discarded stuff; use pre-shifting characteristics just during Boolean evaluation */
			loop_save_start = TREF(expr_start);
			loop_save_start_orig = TREF(expr_start_orig);
			TREF(shift_side_effects) = save_shift;
			TREF(saw_side_effect) = save_saw_side;
		}
		if (!bool_expr(FALSE, cnd))				/* process a Boolean */
		{
			SELECT_CLEANUP;
			if (shifting)
				setcurtchain(oldchain);
			else if ((NULL != savechain) && (((TREF(curtchain)) != &t_orig) || (!ALREADY_RTERROR)))
				setcurtchain(savechain);		/* error means return to original chain */
			return FALSE;
		}
		if (TK_COLON != TREF(window_token))			/* next comes a colon */
		{
			SELECT_CLEANUP;
			if (shifting)
				setcurtchain(oldchain);
			else if ((NULL != savechain) && (((TREF(curtchain)) != &t_orig) || (!ALREADY_RTERROR)))
				setcurtchain(savechain);		/* error means return to original chain */
			stx_error(ERR_COLON);
			return FALSE;
		}
		advancewindow();
<<<<<<< HEAD
		triptr = (TREF(curtchain))->exorder.bl;
		boolexprfinish = (OC_BOOLEXPRFINISH == triptr->opcode) ? triptr : NULL;
		if (NULL != boolexprfinish)
			triptr = triptr->exorder.bl;
		for ( ; OC_NOOP == triptr->opcode; triptr = triptr->exorder.bl)
			;	/* get back, get back to where we once belonged - to find an indicator of the actual result */
		if (!got_true && OC_LIT == triptr->opcode)
		{	/* it is a literal not following an already optimizing TRUE, so optimize it */
			v = &triptr->operand[0].oprval.mlit->v;
=======
		assert((got_true ? &dmpchain : (shifting ? &tmpchain : oldchain)) == TREF(curtchain));
		if (shifting)	/* if shifting return to supression of side effect games for the expression */
			TREF(shift_side_effects) = TREF(saw_side_effect) = FALSE;
		for (triptr = (TREF(curtchain))->exorder.bl; !got_true; triptr = triptr->exorder.bl)
		{	/* get back, get back to where we once belonged - to find an indicator of the actual result */
			if (OC_NOOP == triptr->opcode)
				continue;				/* keep looking */
			if (OC_LIT != triptr->opcode)
				break;					/* Boolean was not a literal */
			v = &triptr->operand[0].oprval.mlit->v;		/* Boolean was a literal, so optimize it */
>>>>>>> 3d3cd0dd... GT.M V6.3-010
			dqdel(triptr, exorder);
			/* Remove OC_BOOLEXPRSTART and OC_BOOLEXPRFINISH opcodes too */
			REMOVE_BOOLEXPRSTART_AND_FINISH(boolexprfinish);	/* Note: Will set "boolexprfinish" to NULL */
			unuse_literal(v);
			dqinit(&dmpchain, exorder);
			if (0 == MV_FORCE_BOOL(v))
			{	/* Boolean FALSE: discard the corresponding value */
				assert(NULL == savechain);
				savechain = setcurtchain(&dmpchain);
				assert(shifting ? (&tmpchain == savechain) : (oldchain == savechain));
				throwing = TRUE;
			} else
			{	/* Boolean TRUE: take this argument and disregard any following arguments */
				assert(!throwing && (NULL == savechain));
				if (first_time && shifting)
					setcurtchain(oldchain);
				got_true = TRUE;
			}
			break;
		}
		if (EXPR_FAIL == expr(&tmparg, MUMPS_EXPR))		/* now a corresponding value */
		{
			SELECT_CLEANUP;
			if (shifting)
				setcurtchain(oldchain);
			else if ((NULL != savechain) && (((TREF(curtchain)) != &t_orig) || (!ALREADY_RTERROR)))
				setcurtchain(savechain);		/* error means return to original chain */
			return FALSE;
		}
		assert(TRIP_REF == tmparg.oprclass);
		if (throwing)
		{	/* finished a useless argument so see what's next */
			assert(shifting ? (&tmpchain == savechain) : (savechain == oldchain));
			if (!got_true)
			{	/* discarded arg with a literal FALSE; stop throwing as later arguments may have value */
				if (shifting)
				{
					TREF(expr_start) = loop_save_start;
					TREF(expr_start_orig) = loop_save_start_orig;
				}
				TREF(shift_side_effects) = save_shift;
				setcurtchain(savechain);
				DEBUG_ONLY(savechain = NULL);
				dqinit(&dmpchain, exorder);
				throwing = FALSE;
			}	/* even if no more arguments, do the above to keep state management consistent */
			if (TK_COMMA != TREF(window_token))
				break;
			advancewindow();
			continue;
		}
		assert(!throwing && (&dmpchain != TREF(curtchain)));
		old_op = tmparg.oprval.tref->opcode;
		if (first_time)
		{
			if (got_true && (OC_LIT == old_op))
			{	/* if the value is also a literal, turn the OC_PASSTHRU into the return value */
				assert((OC_PASSTHRU == r->opcode) && (NO_REF == r->operand[0].oprclass));
				r = tmparg.oprval.tref;
			} else
			{	/* build a home for the result */
				if ((OC_LIT == old_op) || (OCT_MVADDR & oc_tab[old_op].octype))
				{
					ref = newtriple(OC_STOTEMP);
					ref->operand[0] = tmparg;
					tmparg = put_tref(ref);
				}
				r->operand[0] = target = tmparg;
			}
			first_time = FALSE;
		} else if (OC_PASSTHRU == r->opcode)
		{	/* add to the list of possible results */
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
<<<<<<< HEAD
		if (!got_true)
		{	/* jump to the end in case the run time value should turn out to be (the first) TRUE */
			ref = newtriple(OC_JMP);
			ref->operand[0] = endtrip;
			INSERT_BOOLEXPRFINISH_AFTER_JUMP(boolexprfinish, boolexprfinish2);
			*cnd = put_tjmp(boolexprfinish2);
			/* No need for INSERT_OC_JMP_BEFORE_OC_BOOLEXPRFINISH since OC_JMP has been inserted already above */
		}
=======
>>>>>>> 3d3cd0dd... GT.M V6.3-010
		if (TK_COMMA != TREF(window_token))
			break;
		advancewindow();
		if (got_true)
		{	/* Boolean literal TRUE; now we have the value, start throwing out upcoming useless arguments */
			assert(NULL == savechain);
			assert((dmpchain.exorder.fl == dmpchain.exorder.bl) && (dmpchain.exorder.fl == &dmpchain));
			savechain = setcurtchain(&dmpchain);	/* discard arguments after a compile time TRUE */
			if (shifting)
				savechain = &tmpchain;
			throwing = TRUE;
			continue;
		}
		if (OC_PASSTHRU == r->opcode)
		{	/* Not the case where the 1st argument has both a literal Boolean and a literal result */
			ref = newtriple(OC_JMP);		/* jump to end in case the value turns out to be (the first) TRUE */
			ref->operand[0] = endtrip;
			tnxtarg(cnd);
		}
	}
	if (got_true)
	{	/* FALSE throwing cleans up after itself */
		assert(!throwing ? (NULL == savechain) : (shifting ? (&tmpchain == savechain) : (oldchain == savechain)));
		if (shifting)
		{	/* clean up any discards and ensure we're working on the intended chain */
			TREF(expr_start) = loop_save_start;
			TREF(expr_start_orig) = loop_save_start_orig;
			setcurtchain(&tmpchain);
		}
		if (throwing)
		{	/* if we might have discarded things, return the chain to normal */
			assert(savechain);
			setcurtchain(savechain);
		}
	} else
	{	/* if we didn't find a TRUE at compile time, then insert a possible error in case there's no TRUE at run time */
		assert(!throwing);
		assert(shifting ? (&tmpchain == TREF(curtchain)) : (oldchain == TREF(curtchain)));
		if (!first_time)
		{	/* if we ended with a runtime evaluation make sure it has its jump */
			ref = newtriple(OC_JMP);		/* jump to end in case the value turns out to be (the first) TRUE */
			ref->operand[0] = endtrip;
			tnxtarg(cnd);
		} else
		{	/* if all values were literals and FALSE, supply a dummy evaluation so we reach the error gracefully */
			PUT_LITERAL_TRUTH(FALSE, r);
			r->opcode = OC_LIT;
			ins_triple(noop);
			ins_triple(r);
		}
		tmparg = put_ilit(ERR_SELECTFALSE);
		ref = newtriple(OC_RTERROR);
		ref->operand[0] = tmparg;
		ref->operand[1] = put_ilit(FALSE);		/* flag as not a subroutine reference */
	}
	assert(shifting ? (&tmpchain == TREF(curtchain)) : (oldchain == TREF(curtchain)));
	if (OC_PASSTHRU == r->opcode)
<<<<<<< HEAD
	{
		ins_triple(noop);
		ins_triple(r);
	}
=======
		ins_triple(r);					/* 1st arg was not literal:literal */
>>>>>>> 3d3cd0dd... GT.M V6.3-010
	saw_se_in_select = TREF(saw_side_effect);		/* note this down before it gets reset by DECREMENT_EXPR_DEPTH */
	if (shifting)
		DECREMENT_EXPR_DEPTH;				/* clean up */
	assert(!TREF(expr_depth));
	save_se_base[save_expr_depth] |= (TREF(side_effect_base))[TREF(expr_depth)];
	TREF(saw_side_effect) = saw_se_in_select | save_saw_side;
	SELECT_CLEANUP;						/* restores TREFs - see macro definition at top of this module */
	if (shifting)
	{	/* get the tmpchain into the shiftchain and add OC_GVSAVTARG / OC_GVRECTARG as needed */
		assert(&tmpchain == TREF(curtchain) && (TREF(expr_start) == save_start) && (NULL != save_start));
		if (tmpchain.exorder.fl == tmpchain.exorder.bl)
			setcurtchain(oldchain);
		else
		{
			shifting = (((1 < save_expr_depth) && TREF(saw_side_effect))
				|| ((save_start != save_start_orig) && (OC_NOOP != save_start->opcode)));
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
	}
	assert(oldchain == TREF(curtchain));
	*a = put_tref(r);
	return TRUE;
}
