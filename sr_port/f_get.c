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
#include "indir_enum.h"
#include "mdq.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "fullbool.h"

error_def(ERR_VAREXPECTED);

int f_get(oprtype *a, opctype op)
{
	triple		*oldchain, *r, tmpchain, *triptr;
	oprtype		result, *result_ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	result_ptr = (oprtype *)mcalloc(SIZEOF(oprtype));
	result = put_indr(result_ptr);
	r = maketriple(op);
	switch (TREF(window_token))
	{
	case TK_IDENT:
		if (!lvn(&r->operand[0], OC_SRCHINDX, 0))
			return FALSE;
		if (TK_COMMA != TREF(window_token))
		{
			ins_triple(r);
			*a = put_tref(r);
			return TRUE;
		}
		r->opcode = OC_FNGET2;
		r->operand[1] = result;
		break;
	case TK_CIRCUMFLEX:
		if (!gvn())
			return FALSE;
		if (TK_COMMA == TREF(window_token))
		{	/* 2-argument $GET with global-variable as first argument. In this case generate the following
			 * sequence of opcodes. OC_FNGVGET1, opcodes-to-evaluate-second-argument-expression, OC_FNGVGET2
			 */
			r->opcode = OC_FNGVGET1;
			ins_triple(r);
			triptr = r;
			/* Prepare triple for OC_FNGVGET2 */
			r = maketriple(op);
			r->opcode = OC_FNGVGET2;
			r->operand[0] = put_tref(triptr);
			r->operand[1] = result;
		} else
		{
			r->opcode = OC_FNGVGET;
			r->operand[0] = result;
		}
		break;
	case TK_ATSIGN:
		r->opcode = OC_INDGET;
		TREF(saw_side_effect) = TREF(shift_side_effects);
		if (TREF(shift_side_effects) && (GTM_BOOL == TREF(gtm_fullbool)))
		{
			dqinit(&tmpchain, exorder);
			oldchain = setcurtchain(&tmpchain);
			if (!indirection(&r->operand[0]))
			{
				setcurtchain(oldchain);
				return FALSE;
			}
			r->operand[1] = result;
			if (TK_COMMA == TREF(window_token))
			{
				advancewindow();
				if (EXPR_FAIL == expr(result_ptr, MUMPS_EXPR))
					return FALSE;
			} else
				*result_ptr = put_str(0, 0);
			ins_triple(r);
			newtriple(OC_GVSAVTARG);
			setcurtchain(oldchain);
			dqadd(TREF(expr_start), &tmpchain, exorder);
			TREF(expr_start) = tmpchain.exorder.bl;
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(TREF(expr_start));
			*a = put_tref(r);
			return TRUE;
		}
		if (!indirection(&r->operand[0]))
			return FALSE;
		r->operand[1] = result;
		break;
	default:
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
	if (TK_COMMA == TREF(window_token))
	{
		advancewindow();
		if (EXPR_FAIL == expr(result_ptr, MUMPS_EXPR))
			return FALSE;
	} else
		*result_ptr = put_str(0, 0);
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
