/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "mdq.h"
#include "mmemory.h"
#include "advancewindow.h"

GBLREF char window_token;
LITREF octabstruct oc_tab[];

int f_select( oprtype *a, opctype op )
{
	triple tmpchain, *oldchain, *ref, *r, *save_start, *save_start_orig, *triptr;
	oprtype *cnd, tmparg, endtrip, target;
	opctype old_op;
	unsigned int save_depth;
	boolean_t first_time, save_shift;
	error_def(ERR_COLON);
	error_def(ERR_SELECTFALSE);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	save_shift = TREF(shift_side_effects);
	save_depth = TREF(expr_depth);
	save_start = TREF(expr_start);
	save_start_orig = TREF(expr_start_orig);
	TREF(shift_side_effects) = FALSE;
	TREF(expr_depth) = 0;
	TREF(expr_start) = TREF(expr_start_orig) = NULL;
	if (save_shift)
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
		if (!bool_expr((bool) FALSE, cnd))
		{
			if (save_shift)
				setcurtchain(oldchain);
			return FALSE;
		}
		if (TK_COLON != window_token)
		{
			if (save_shift)
				setcurtchain(oldchain);
			stx_error(ERR_COLON);
			return FALSE;
		}
		advancewindow();
		if (!expr(&tmparg))
		{
			if (save_shift)
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
		if (TK_COMMA != window_token)
			break;
		advancewindow();
	}
	tmparg = put_ilit(ERR_SELECTFALSE);
	ref = newtriple(OC_RTERROR);
	ref->operand[0] = tmparg;
	ref->operand[1] = put_ilit(FALSE);	/* Not a subroutine reference */
	ins_triple(r);
	assert(!TREF(expr_depth));
	TREF(shift_side_effects) = save_shift;
	TREF(expr_depth) = save_depth;
	TREF(expr_start) = save_start;
	TREF(expr_start_orig) = save_start_orig;
	if (save_shift)
	{
		newtriple(OC_GVSAVTARG);
		setcurtchain(oldchain);
		dqadd(TREF(expr_start), &tmpchain, exorder);
		TREF(expr_start) = tmpchain.exorder.bl;
		triptr = newtriple(OC_GVRECTARG);
		triptr->operand[0] = put_tref(TREF(expr_start));
	}
	*a = put_tref(r);
	return TRUE;
}
