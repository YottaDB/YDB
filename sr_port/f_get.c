/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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

GBLREF	char		window_token;
GBLREF	triple		*curtchain;

int f_get(oprtype *a, opctype op)
{
	triple		tmpchain, *oldchain, *r, *triptr;
	oprtype		result, *result_ptr;
	error_def(ERR_VAREXPECTED);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	result_ptr = (oprtype *)mcalloc(SIZEOF(oprtype));
	result = put_indr(result_ptr);
	r = maketriple(op);
	switch (window_token)
	{
	case TK_IDENT:
		if (!lvn(&r->operand[0], OC_SRCHINDX, 0))
			return FALSE;
		if (window_token != TK_COMMA)
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
		if (window_token == TK_COMMA)
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
		if (TREF(shift_side_effects))
		{
			dqinit(&tmpchain, exorder);
			oldchain = setcurtchain(&tmpchain);
			if (!indirection(&r->operand[0]))
			{
				setcurtchain(oldchain);
				return FALSE;
			}
			r->operand[1] = result;
			if (window_token == TK_COMMA)
			{
				advancewindow();
				if (!expr(result_ptr))
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
	if (window_token == TK_COMMA)
	{
		advancewindow();
		if (!expr(result_ptr))
			return FALSE;
	} else
		*result_ptr = put_str(0, 0);
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
