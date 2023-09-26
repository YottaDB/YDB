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
#include "opcode.h"
#include "toktyp.h"
#include "mdq.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "fullbool.h"
#include "stringpool.h"
#include "gtmdbglvl.h"

GBLREF triple		t_orig;
GBLREF uint4		gtmDebugLevel;

error_def(ERR_COLON);
error_def(ERR_SELECTFALSE);

LITREF octabstruct oc_tab[];

typedef struct _save_for_select
{
	triple		*expr_start;
	triple		*expr_start_orig;
	boolean_t	shift_side_effects;
	boolean_t	saw_side_effect;
	boolean_t	*side_effect_base;
	uint4		expr_depth;
	uint4		side_effect_depth;
} save_for_select;

#define SELECT_CLEANUP							\
MBSTART {								\
	free(TREF(side_effect_base));					\
	TREF(side_effect_base) = save_state->side_effect_base;		\
	TREF(side_effect_depth) = save_state->side_effect_depth;	\
	TREF(expr_depth) = save_state->expr_depth;			\
	TREF(saw_side_effect) = save_state->saw_side_effect;		\
	TREF(shift_side_effects) = save_state->shift_side_effects;	\
	TREF(expr_start_orig) = save_state->expr_start_orig;		\
	TREF(expr_start) = save_state->expr_start;			\
} MBEND


int f_select(oprtype *a, opctype op)
/* drive parsing for the $select function
 * a is an operand the caller places to access the result
 * op is actuallly an OC_PASSTHRU to anchor the list of STO'd of values which Boolean controlled jumps navigate
 * the return is TRUE for success and FALSE for a failure
 */
{
	boolean_t	first_time, got_true, se_saw_side, shifting, throwing;
	opctype		old_op;
	oprtype		*cnd, endtrip, target, tmparg;
	triple		dmpchain, *loop_expr_start, *oldchain = NULL, *r, *ref, *savechain, tmpchain, *triptr;
	mval		*v;
	save_for_select	*save_state, ss;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef DEBUG
	if (GDL_DebugCompiler & gtmDebugLevel)
		CHKTCHAIN(TREF(curtchain), exorder, (NULL != TREF(expr_start)));
#	endif
	save_state = &ss;
	save_state->expr_start = TREF(expr_start);
	save_state->expr_start_orig = TREF(expr_start_orig);
	save_state->shift_side_effects = TREF(shift_side_effects);
	if ((save_state->shift_side_effects) && (GTM_BOOL != TREF(gtm_fullbool)))
		TREF(saw_side_effect) = TRUE; /* this will stop shifting of side effects in FULL_BOOL mode */
	save_state->saw_side_effect = TREF(saw_side_effect);
	save_state->expr_depth = TREF(expr_depth);
	save_state->side_effect_base = TREF(side_effect_base);
	save_state->side_effect_depth = TREF(side_effect_depth);
	se_saw_side = FALSE;
	TREF(expr_depth) = 0;
	TREF(side_effect_depth) = INITIAL_SIDE_EFFECT_DEPTH;
	TREF(side_effect_base) = malloc(SIZEOF(boolean_t) * TREF(side_effect_depth));
	memset((char *)(TREF(side_effect_base)), 0, SIZEOF(boolean_t) * TREF(side_effect_depth));
	if ((shifting = (save_state->shift_side_effects) && (NULL != save_state->expr_start)
				&& ((!save_state->saw_side_effect) || (GTM_BOOL == TREF(gtm_fullbool)))))
	{	/* shift in progress; WARNING assignment above */
		TREF(expr_depth) = 1;		/* Don't want to hit bottom with each expression, so start at 1 rather than 0 */
		exorder_init(&tmpchain);
		oldchain = setcurtchain(&tmpchain);
	}
	r = maketriple(op);
	endtrip = put_tjmp(r);
	first_time = TRUE;
	got_true = throwing = FALSE;
	savechain = NULL;
	for (;;)
	{
		cnd = (oprtype *)mcalloc(SIZEOF(oprtype));
		if (shifting)
		{	/* aleady preparing to shift everything, so no need for additional juggling */
			TREF(expr_start) = TREF(expr_start_orig) = NULL;
			TREF(shift_side_effects) = TREF(saw_side_effect) = FALSE;
		}
		loop_expr_start = TREF(expr_start);
		if (!bool_expr(FALSE, cnd))				/* process a Boolean */
		{	/* bad Boolean */
			SELECT_CLEANUP;
			if (shifting)
			{
				assert(oldchain);
				setcurtchain(oldchain);
			}
			else if ((NULL != savechain) && (((TREF(curtchain)) != &t_orig) || (!ALREADY_RTERROR)))
				setcurtchain(savechain);		/* error means return to original chain */
			return FALSE;
		}
		if (TK_COLON != TREF(window_token))			/* next comes a colon */
		{	/* syntax problem */
			SELECT_CLEANUP;
			if (shifting)
			{
				assert(oldchain);
				setcurtchain(oldchain);
			}
			else if ((NULL != savechain) && (((TREF(curtchain)) != &t_orig) || (!ALREADY_RTERROR)))
				setcurtchain(savechain);		/* error means return to original chain */
			stx_error(ERR_COLON);
			return FALSE;
		}
		advancewindow();					/* past the colon */
		assert(!got_true || (&dmpchain == TREF(curtchain)));
		for (triptr = (TREF(curtchain))->exorder.bl; !got_true; triptr = triptr->exorder.bl)
		{	/* get back, get back to where we once belonged - to find an indicator of the actual result */
			if (OC_NOOP == triptr->opcode)
				continue;				/* keep looking */
			if (OC_LIT != triptr->opcode)
				break;					/* Boolean was not a literal */
			v = &triptr->operand[0].oprval.mlit->v;		/* Boolean was a literal, so optimize it */
			dqdel(triptr, exorder);
			exorder_init(&dmpchain);			/* both got_true and throwing use dumping */
			unuse_literal(v);
			if (0 == MV_FORCE_BOOL(v))
			{	/* Boolean FALSE: discard the corresponding value */
				assert(NULL == savechain);
				savechain = setcurtchain(&dmpchain);
				throwing = TRUE;
			} else
			{	/* Boolean TRUE: take this argument and disregard any following arguments */
				assert(!throwing && (NULL == savechain));
				got_true = TRUE;
			}
			break;
		}
		TREF(shift_side_effects) = TREF(saw_side_effect) = FALSE;
		TREF(expr_start) = TREF(expr_start_orig) = NULL;	/* FALSE may bypass expr, so, again, discourage shifting */
		if (EXPR_FAIL == expr(&tmparg, MUMPS_EXPR))		/* now a corresponding value */
		{	/* bad expression */
			SELECT_CLEANUP;
			if (shifting)
			{
				assert(oldchain);
				setcurtchain(oldchain);
			}
			else if ((NULL != savechain) && (((TREF(curtchain)) != &t_orig) || (!ALREADY_RTERROR)))
				setcurtchain(savechain);		/* error means return to original chain */
			return FALSE;
		}
		assert(TRIP_REF == tmparg.oprclass);
		if (throwing)
		{	/* finished a useless argument so see what's next */
			TREF(expr_start) = loop_expr_start;
			if (!got_true)
			{	/* discarded arg with a literal FALSE; stop throwing as later arguments may have value */
				setcurtchain(savechain);
				DEBUG_ONLY(savechain = NULL;);
				throwing = FALSE;
			}	/* even if no more arguments (there's usually 1 TRUE), do above to keep uniform state management */
			if (TK_COMMA != TREF(window_token))
				break;
			advancewindow();
			continue;
		}
		assert(!throwing && (&dmpchain != TREF(curtchain)));
		old_op = tmparg.oprval.tref->opcode;
		if (first_time)
		{	/* setup differs */
			if (got_true && (OC_LIT == old_op))
			{	/* if the value is also a literal, turn the OC_PASSTHRU into the return value */
				assert((OC_PASSTHRU == r->opcode) && (NO_REF == r->operand[0].oprclass));
				r = tmparg.oprval.tref;
			} else
			{	/* build a home for the result */
				if ((OC_LIT == old_op) || (OC_FNTEXT == old_op) || (OCT_MVADDR & oc_tab[old_op].octype))
				{	/* need a temp for these - OP_FNTEXT because it may later become an OC_LIT */
					ref = newtriple(OC_STOTEMP);
					ref->operand[0] = tmparg;
					tmparg = put_tref(ref);
				}
				r->operand[0] = target = tmparg;
			}
			first_time = FALSE;
		} else
		{	/* add to the list of possible results */
			assert(OC_PASSTHRU == r->opcode);
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
		if (TK_COMMA != TREF(window_token))
			break;					/* argument list end */
		advancewindow();
		if (got_true)
		{	/* Boolean literal TRUE; now we have the value, start throwing out upcoming useless arguments */
			assert((&dmpchain == dmpchain.exorder.fl) && (&dmpchain == dmpchain.exorder.bl));
			assert(NULL == savechain);
			savechain = setcurtchain(&dmpchain);	/* discard arguments after a compile time TRUE */
			loop_expr_start = TREF(expr_start);
			TREF(expr_start) = TREF(expr_start_orig) = NULL;
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
		if (throwing)
		{	/* if we might have discarded things, return the chains to normal */
			setcurtchain(savechain);
			TREF(expr_start) = loop_expr_start;
		}
	} else
	{	/* if we didn't find a TRUE at compile time, then insert a possible error in case there's no TRUE at run time */
		assert(!throwing);
		if (!first_time)
		{	/* if we ended with a runtime evaluation make sure it has its jump */
			ref = newtriple(OC_JMP);		/* jump to end in case the value turns out to be (the first) TRUE */
			ref->operand[0] = endtrip;
			tnxtarg(cnd);
		} else
		{	/* if all values were literals and FALSE, supply a dummy evaluation so we reach the error gracefully */
			PUT_LITERAL_TRUTH(FALSE, r);
			r->opcode = OC_LIT;
			ins_triple(r);
		}
		tmparg = put_ilit(ERR_SELECTFALSE);
		ref = newtriple(OC_RTERROR);
		ref->operand[0] = tmparg;
		ref->operand[1] = put_ilit(FALSE);		/* flag as not a subroutine reference */
	}
	if (OC_PASSTHRU == r->opcode)
		ins_triple(r);					/* 1st arg was not literal:literal */
	se_saw_side = TREF(saw_side_effect);			/* note this down before it gets reset by DECREMENT_EXPR_DEPTH */
	if (shifting)
		DECREMENT_EXPR_DEPTH;				/* clean up */
	assert(!TREF(expr_depth));
	save_state->saw_side_effect |= se_saw_side;		/* this & next feed state to evaluations containing this $select */
	save_state->side_effect_base[save_state->expr_depth] |= (TREF(side_effect_base))[TREF(expr_depth)];
	if (shifting)
	{	/* get the tmpchain into the shiftchain and add OC_GVSAVTARG / OC_GVRECTARG */
		shifting = (((1 < save_state->expr_depth) && TREF(saw_side_effect))	/*see if something abandoned shift */
			|| ((save_state->expr_start != save_state->expr_start_orig)
			&& (OC_NOOP != save_state->expr_start->opcode)));
		SELECT_CLEANUP;
		assert(&tmpchain == TREF(curtchain));
		newtriple(shifting ? OC_GVSAVTARG : OC_NOOP);	/* must have one of these two at expr_start */
		assert(oldchain);
		setcurtchain(oldchain);
		assert(NULL != save_state->expr_start);
		TREF(expr_start) = save_state->expr_start;
		assert(&t_orig != TREF(expr_start));
		dqadd(TREF(expr_start), &tmpchain, exorder);
		TREF(expr_start) = tmpchain.exorder.bl;
		if (shifting)
		{	/* only play this game if it has not been abandoned */
			assert(OC_GVSAVTARG == (TREF(expr_start))->opcode);
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(TREF(expr_start));
		}
#		ifdef DEBUG
		if (GDL_DebugCompiler & gtmDebugLevel)
		{
			CHKTCHAIN(TREF(curtchain), exorder, TRUE);
			CHKTCHAIN(TREF(expr_start_orig), exorder, FALSE);
		}
#		endif
	} else
		SELECT_CLEANUP;
	CHKTCHAIN(TREF(curtchain), exorder, (NULL != TREF(expr_start)));
	*a = put_tref(r);
	return TRUE;
}
