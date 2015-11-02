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

error_def(ERR_VAREXPECTED);

int f_data(oprtype *a, opctype op)
{
	triple		*oldchain, *r;
	save_se		save_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(OC_FNDATA == op || OC_FNZDATA == op);
	r = maketriple(op);
	switch (TREF(window_token))
	{
	case TK_IDENT:
		if (!lvn(&(r->operand[0]), OC_SRCHINDX, 0))
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
		if (SHIFT_SIDE_EFFECTS)
		{
			START_GVBIND_CHAIN(&save_state, oldchain);
			if (!indirection(&(r->operand[0])))
			{
				setcurtchain(oldchain);
				return FALSE;
			}
			r->operand[1] = put_ilit((mint)(OC_FNDATA == op ? indir_fndata : indir_fnzdata));
			ins_triple(r);
			PLACE_GVBIND_CHAIN(&save_state, oldchain);
		} else
		{
			if (!indirection(&(r->operand[0])))
				return FALSE;
			r->operand[1] = put_ilit((mint)(OC_FNDATA == op ? indir_fndata : indir_fnzdata));
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
