/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
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
{
	boolean_t	first_time, save_saw_side, saw_se_in_select, *save_se_base, save_shift, shifting, gvn_or_indir_in_select;
	opctype		old_op;
	oprtype		*cnd, endtrip, target, tmparg;
	triple		*oldchain, *r, *ref, *save_start, *save_start_orig, tmpchain, *triptr;
	uint4		save_expr_depth, save_se_depth;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	save_expr_depth = TREF(expr_depth);
	save_start = TREF(expr_start);
	save_start_orig = TREF(expr_start_orig);
	save_saw_side = TREF(saw_side_effect);
	save_shift = TREF(shift_side_effects);
	save_se_base = TREF(side_effect_base);
	save_se_depth = TREF(side_effect_depth);
	TREF(expr_depth) = 0;
	TREF(expr_start) = TREF(expr_start_orig) = NULL;
	TREF(saw_side_effect) = FALSE;
	TREF(side_effect_depth) = INITIAL_SIDE_EFFECT_DEPTH;
	TREF(side_effect_base) = malloc(SIZEOF(boolean_t) * TREF(side_effect_depth));
	memset((char *)(TREF(side_effect_base)), 0, SIZEOF(boolean_t) * TREF(side_effect_depth));
	if (shifting = (save_shift && (!save_saw_side || (GTM_BOOL == TREF(gtm_fullbool)))))	/* NOTE assignment */
	{
		dqinit(&tmpchain, exorder);
		oldchain = setcurtchain(&tmpchain);
		INCREMENT_EXPR_DEPTH;	/* Don't want to hit bottom with each expression, so start at 1 rather than 0 */
		TREF(expr_start) = TREF(expr_start_orig) = &tmpchain;
		TREF(shift_side_effects) = TRUE;
	} else
		TREF(shift_side_effects) = FALSE;
	r = maketriple(op);
	first_time = TRUE;
	endtrip = put_tjmp(r);
	for (;;)
	{
		cnd = (oprtype *)mcalloc(SIZEOF(oprtype));
		if (!bool_expr(FALSE, cnd))
		{
			SELECT_CLEANUP;
			if (shifting)
				setcurtchain(oldchain);
			return FALSE;
		}
		if (TK_COLON != TREF(window_token))
		{
			SELECT_CLEANUP;
			if (shifting)
				setcurtchain(oldchain);
			stx_error(ERR_COLON);
			return FALSE;
		}
		advancewindow();
		if (EXPR_FAIL == expr(&tmparg, MUMPS_EXPR))
		{
			SELECT_CLEANUP;
			if (shifting)
				setcurtchain(oldchain);
			return FALSE;
		}
		assert(TRIP_REF == tmparg.oprclass);
		old_op = tmparg.oprval.tref->opcode;
		if (first_time)
		{
			if ((OC_LIT == old_op) || (oc_tab[old_op].octype & OCT_MVADDR))
			{
				ref = newtriple(OC_STOTEMP);
				ref->operand[0] = tmparg;
				tmparg = put_tref(ref);
			}
			r->operand[0] = target = tmparg;
			first_time = FALSE;
		} else
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
		ref = newtriple(OC_JMP);
		ref->operand[0] = endtrip;
		tnxtarg(cnd);
		if (TK_COMMA != TREF(window_token))
			break;
		advancewindow();
	}
	tmparg = put_ilit(ERR_SELECTFALSE);
	ref = newtriple(OC_RTERROR);
	ref->operand[0] = tmparg;
	ref->operand[1] = put_ilit(FALSE);	/* Not a subroutine reference */
	ins_triple(r);
	saw_se_in_select = TREF(saw_side_effect);	/* note this down before it gets reset by DECREMENT_EXPR_DEPTH */
	if (shifting)
		DECREMENT_EXPR_DEPTH;		/* Clean up */
	assert(!TREF(expr_depth));
	gvn_or_indir_in_select = (TREF(expr_start) != TREF(expr_start_orig));
	TREF(expr_start) = save_start;
	TREF(expr_start_orig) = save_start_orig;
	TREF(shift_side_effects) = save_shift;
	save_se_base[save_expr_depth] |= (TREF(side_effect_base))[TREF(expr_depth)];
	TREF(saw_side_effect) = saw_se_in_select | save_saw_side;
	SELECT_CLEANUP;	/* restores TREF(expr_depth), TREF(side_effect_base) and TREF(side_effect_depth) */
	if (shifting)
	{
		if (!gvn_or_indir_in_select && ((GTM_BOOL == TREF(gtm_fullbool)) || !saw_se_in_select))
		{
			setcurtchain(oldchain);
			triptr = (TREF(curtchain))->exorder.bl;
			dqadd(triptr, &tmpchain, exorder);	/* this is a violation of info hiding */
		} else
		{
			shifting = ((TREF(expr_start) != TREF(expr_start_orig)) && (OC_NOOP != (TREF(expr_start))->opcode));
			newtriple(shifting ? OC_GVSAVTARG : OC_NOOP);	/* must have one of these two at expr_start */
			setcurtchain(oldchain);
			assert(NULL != TREF(expr_start));
			dqadd(TREF(expr_start), &tmpchain, exorder);
			TREF(expr_start) = tmpchain.exorder.bl;
			if (shifting)
			{	/* only play this game if something else started it */
				assert(OC_GVSAVTARG == (TREF(expr_start))->opcode);
				triptr = newtriple(OC_GVRECTARG);
				triptr->operand[0] = put_tref(TREF(expr_start));
			}
		}
	}
	*a = put_tref(r);
	return TRUE;
}
