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
#include "indir_enum.h"
#include "toktyp.h"
#include "mdq.h"

GBLREF char window_token;
GBLREF bool shift_gvrefs;
GBLREF triple *expr_start;

int f_data(oprtype *a, opctype op)
{
	triple *oldchain, tmpchain, *r, *triptr;
	error_def(ERR_VAREXPECTED);

	r = maketriple(op);
	switch (window_token)
	{
	case TK_IDENT:
		if (!lvn(&(r->operand[0]),OC_SRCHINDX,0))
			return FALSE;
		ins_triple(r);
		break;
	case TK_CIRCUMFLEX:
		if (!gvn())
			return FALSE;
		r->opcode = OC_GVDATA;
		ins_triple(r);
		break;
	case TK_ATSIGN:
		if (shift_gvrefs)
		{
			dqinit(&tmpchain, exorder);
			oldchain = setcurtchain(&tmpchain);
			if (!indirection(&(r->operand[0])))
			{
				setcurtchain(oldchain);
				return FALSE;
			}
			r->operand[1] = put_ilit((mint) indir_fndata);
			ins_triple(r);
			newtriple(OC_GVSAVTARG);
			setcurtchain(oldchain);
			dqadd(expr_start, &tmpchain, exorder);
			expr_start = tmpchain.exorder.bl;
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(expr_start);
		}
		else
		{
			if (!indirection(&(r->operand[0])))
				return FALSE;
			r->operand[1] = put_ilit((mint) indir_fndata);
			ins_triple(r);
		}
		r->opcode = OC_INDFUN;
		break;
	default:
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
	*a = put_tref(r);
	return TRUE;
}
