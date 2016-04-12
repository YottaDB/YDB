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
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mdq.h"
#include "fullbool.h"

error_def(ERR_GVNEXTARG);
error_def(ERR_LVORDERARG);
error_def(ERR_VAREXPECTED);

int f_next(oprtype *a, opctype op)
{
	triple		*oldchain, *ref, *r;
	save_se		save_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	switch (TREF(window_token))
	{
	case TK_IDENT:
		if (TK_LPAREN != TREF(director_token))
		{
			stx_error(ERR_LVORDERARG);
			return FALSE;
		}
		if (!lvn(&(r->operand[0]), OC_SRCHINDX,r))
			return FALSE;
		ins_triple(r);
		break;
	case TK_CIRCUMFLEX:
		ref = TREF(shift_side_effects) ? TREF(expr_start) : (TREF(curtchain))->exorder.bl;
		if (!gvn())
			return FALSE;
		/* the following assumes OC_LIT and OC_GVNAME are all one gets for an unsubscripted global variable reference */
		if ((TREF(shift_side_effects) ? TREF(expr_start) : TREF(curtchain))->exorder.bl->exorder.bl->exorder.bl == ref)
		{
			stx_error(ERR_GVNEXTARG);
			return FALSE;
		}
		r->opcode = OC_GVNEXT;
		ins_triple(r);
		break;
	case TK_ATSIGN:
		if (SHIFT_SIDE_EFFECTS)
		{
			START_GVBIND_CHAIN(&save_state, oldchain);
			if (!indirection(&(r->operand[0])))
			{
				setcurtchain(oldchain);
				return FALSE;
			}
			r->operand[1] = put_ilit((mint)indir_fnnext);
			ins_triple(r);
			PLACE_GVBIND_CHAIN(&save_state, oldchain);
		} else
		{
			if (!indirection(&(r->operand[0])))
				return FALSE;
			r->operand[1] = put_ilit((mint)indir_fnnext);
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
