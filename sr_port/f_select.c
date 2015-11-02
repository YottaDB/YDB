/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
	boolean_t	first_time, save_saw_side, *save_se_base, save_shift, shifting;
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
	TREF(shift_side_effects) = FALSE;
	TREF(side_effect_depth) = INITIAL_SIDE_EFFECT_DEPTH;
	TREF(side_effect_base) = malloc(SIZEOF(boolean_t) * TREF(side_effect_depth));
	memset((char *)(TREF(side_effect_base)), 0, SIZEOF(boolean_t) * TREF(side_effect_depth));
	if (shifting = (save_shift && (!save_saw_side || (GTM_BOOL == TREF(gtm_fullbool)))))	/* NOTE assignment */
	{
		dqinit(&tmpchain, exorder);
		oldchain = setcurtchain(&tmpchain);
	}
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
			if (OC_PASSTHRU == tmparg.oprval.tref->opcode)
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
	assert(!TREF(expr_depth));
	TREF(expr_start) = save_start;
	TREF(expr_start_orig) = save_start_orig;
	TREF(saw_side_effect) = save_saw_side;
	TREF(shift_side_effects) = save_shift;
	SELECT_CLEANUP;
	TREF(expr_depth) = save_expr_depth;
	if (shifting)
	{
		shifting = ((TREF(expr_start) != TREF(expr_start_orig)) && (OC_NOOP != (TREF(expr_start))->opcode));
		newtriple(shifting ? OC_GVSAVTARG : OC_NOOP);	/* must have one of these two at expr_start */
		setcurtchain(oldchain);
		dqadd(TREF(expr_start), &tmpchain, exorder);
		TREF(expr_start) = tmpchain.exorder.bl;
		if (shifting)
		{	/* only play this game if something else started it */
			assert(OC_GVSAVTARG == (TREF(expr_start))->opcode);
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(TREF(expr_start));
		}
	}
	*a = put_tref(r);
	return TRUE;
}
