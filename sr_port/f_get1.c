/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "toktyp.h"
#include "opcode.h"
#include "indir_enum.h"
#include "mdq.h"
#include "op.h"
#include "fullbool.h"

error_def(ERR_VAREXPECTED);

int	f_get1(oprtype *a, opctype op)
{
	triple		*oldchain, *r;
	save_se		save_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(OC_NOOP);		/* We'll fill in the opcode later, when we figure out what it is */
	switch (TREF(window_token))
	{
		case TK_IDENT:
			r->opcode = OC_FNGET1;
			if (!lvn(&r->operand[0], OC_SRCHINDX, 0))
				return FALSE;
			break;
		case TK_CIRCUMFLEX:
			r->opcode = OC_FNGVGET1;
			if (!gvn())
				return FALSE;
			break;
		case TK_ATSIGN:
			r->opcode = OC_INDFUN;
			r->operand[1] = put_ilit((mint)indir_get);
			if (SHIFT_SIDE_EFFECTS)
			{	/* with short-circuited booleans move indirect processing to expr_start */
				START_GVBIND_CHAIN(&save_state, oldchain);
				if (!indirection(&r->operand[0]))
				{
					setcurtchain(oldchain);
					return FALSE;
				}
				ins_triple(r);
				PLACE_GVBIND_CHAIN(&save_state, oldchain);
				*a = put_tref(r);
				return TRUE;
			}
			if (!indirection(&(r->operand[0])))
				return FALSE;
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			return FALSE;
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
