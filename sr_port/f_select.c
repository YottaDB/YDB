/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
GBLREF bool shift_gvrefs;
GBLREF unsigned short int expr_depth;
GBLREF triple *expr_start, *expr_start_orig;
LITREF octabstruct oc_tab[];

int f_select( oprtype *a, opctype op )
{
	triple tmpchain, *oldchain, *ref, *r, *temp_expr_start, *temp_expr_start_orig, *triptr;
	oprtype *cnd, tmparg, endtrip, target;
	opctype old_op;
	unsigned short int temp_expr_depth;
	bool first_time, temp_shift_gvrefs;
	error_def(ERR_COLON);
	error_def(ERR_SELECTFALSE);

	temp_shift_gvrefs = shift_gvrefs;
	temp_expr_depth = expr_depth;
	temp_expr_start = expr_start;
	temp_expr_start_orig = expr_start_orig;
	shift_gvrefs = FALSE;
	expr_depth = 0;
	expr_start = expr_start_orig = 0;
	if (temp_shift_gvrefs)
	{
		dqinit(&tmpchain, exorder);
		oldchain = setcurtchain(&tmpchain);
	}
	r = maketriple(op);
	first_time = TRUE;
	endtrip = put_tjmp(r);
	for (;;)
	{
		cnd = (oprtype *) mcalloc(sizeof(oprtype));
		if (!bool_expr((bool) FALSE, cnd))
		{
			if (temp_shift_gvrefs)
				setcurtchain(oldchain);
			return FALSE;
		}
		if (window_token != TK_COLON)
		{
			stx_error(ERR_COLON);
			if (temp_shift_gvrefs)
				setcurtchain(oldchain);
			return FALSE;
		}
		advancewindow();
		if (!expr(&tmparg))
		{
			if (temp_shift_gvrefs)
				setcurtchain(oldchain);
			return FALSE;
		}
		assert(tmparg.oprclass == TRIP_REF);
		old_op = tmparg.oprval.tref->opcode;
		if (first_time)
		{
			if (old_op == OC_LIT || oc_tab[old_op].octype & OCT_MVADDR)
			{
				ref = newtriple(OC_STOTEMP);
				ref->operand[0] = tmparg;
				tmparg = put_tref(ref);
			}
			r->operand[0] = target = tmparg;
			first_time = FALSE;
		}
		else
		{
			ref = newtriple(OC_STO);
			ref->operand[0] = target;
			ref->operand[1] = tmparg;
			if (tmparg.oprval.tref->opcode == OC_PASSTHRU)
			{
				assert(tmparg.oprval.tref->operand[0].oprclass == TRIP_REF);
				ref = newtriple(OC_STO);
				ref->operand[0] = target;
				ref->operand[1] = put_tref(tmparg.oprval.tref->operand[0].oprval.tref);
			}
		}
		ref = newtriple(OC_JMP);
		ref->operand[0] = endtrip;
		tnxtarg(cnd);
		if (window_token != TK_COMMA)
			break;
		advancewindow();
	}
	tmparg = put_ilit(ERR_SELECTFALSE);
	ref = newtriple(OC_RTERROR);
	ref->operand[0] = tmparg;
	ref->operand[1] = put_ilit(FALSE);	/* Not a subroutine reference */
	ins_triple(r);
	assert(!expr_depth);
	shift_gvrefs = temp_shift_gvrefs;
	expr_depth = temp_expr_depth;
	expr_start = temp_expr_start;
	expr_start_orig = temp_expr_start_orig;
	if (temp_shift_gvrefs)
	{
		newtriple(OC_GVSAVTARG);
		setcurtchain(oldchain);
		dqadd(expr_start, &tmpchain, exorder);
		expr_start = tmpchain.exorder.bl;
		triptr = newtriple(OC_GVRECTARG);
		triptr->operand[0] = put_tref(expr_start);
	}
	*a = put_tref(r);
	return TRUE;
}
