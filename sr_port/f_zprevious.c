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
#include "indir_enum.h"
#include "toktyp.h"
#include "mdq.h"
#include "advancewindow.h"

GBLREF char		window_token, director_token;
GBLREF mident		window_ident;

int f_zprevious( oprtype *a, opctype op)
{
	triple *oldchain, tmpchain, *r, *triptr;
	error_def(ERR_VAREXPECTED);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	switch (window_token)
	{
	case TK_IDENT:
		if (director_token != TK_LPAREN)
		{
			r->opcode = OC_FNLVPRVNAME;
			r->operand[0] = put_str(window_ident.addr, window_ident.len);
			ins_triple(r);
			advancewindow();
			break;
		}
		if (!lvn(&(r->operand[0]), OC_SRCHINDX, r))
			return FALSE;
		ins_triple(r);
		break;
	case TK_CIRCUMFLEX:
		if (!gvn())
			return FALSE;
		r->opcode = OC_ZPREVIOUS;
		ins_triple(r);
		break;
	case TK_ATSIGN:
		if (TREF(shift_side_effects))
		{
			dqinit(&tmpchain, exorder);
			oldchain = setcurtchain(&tmpchain);
			if (!indirection(&(r->operand[0])))
			{
				setcurtchain(oldchain);
				return FALSE;
			}
			r->operand[1] = put_ilit((mint)indir_fnzprevious);
			ins_triple(r);
			newtriple(OC_GVSAVTARG);
			setcurtchain(oldchain);
			dqadd(TREF(expr_start), &tmpchain, exorder);
			TREF(expr_start) = tmpchain.exorder.bl;
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(TREF(expr_start));
		} else
		{
			if (!indirection(&(r->operand[0])))
				return FALSE;
			r->operand[1] = put_ilit((mint)indir_fnzprevious);
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
