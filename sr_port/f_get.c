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
#include "indir_enum.h"
#include "mdq.h"
#include "mmemory.h"
#include "advancewindow.h"


GBLREF	char	window_token;
GBLREF	bool	shift_gvrefs;
GBLREF	triple	*expr_start;

int f_get(oprtype *a, opctype op)
{
	triple		tmpchain, *oldchain, *r, *triptr;
	triple		*jmp_to_get, *ret_get_val;
	oprtype		result, *result_ptr;
	error_def(ERR_VAREXPECTED);


	result_ptr = (oprtype *)mcalloc(sizeof(oprtype));
	result = put_indr(result_ptr);

	jmp_to_get = maketriple(op);
	ret_get_val = maketriple(op);

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

		r->opcode = OC_FNGVGET1;

		if (!gvn())
			return FALSE;

		ins_triple(r);

		jmp_to_get->opcode = OC_JMPNEQ;
		jmp_to_get->operand[0] = put_tjmp(ret_get_val);

		ins_triple(jmp_to_get);

		ret_get_val->opcode = OC_FNGVGET2;
		ret_get_val->operand[0] = put_tref(r);
		ret_get_val->operand[1] = result;

		if (window_token != TK_COMMA)
			*result_ptr = put_str(0,0);
		else
		{
			advancewindow();
			if (!expr(result_ptr))
				return FALSE;
		}

		ins_triple(ret_get_val);
		*a = put_tref(ret_get_val);

		return TRUE;
		break;

	case TK_ATSIGN:
		r->opcode = OC_INDGET;

		if (shift_gvrefs)
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
			}
			else
				*result_ptr = put_str(0, 0);
			ins_triple(r);

			newtriple(OC_GVSAVTARG);
			setcurtchain(oldchain);
			dqadd(expr_start, &tmpchain, exorder);
			expr_start = tmpchain.exorder.bl;
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(expr_start);
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
	}
	else
		*result_ptr = put_str(0, 0);

	ins_triple(r);
	*a = put_tref(r);

	return TRUE;
}
