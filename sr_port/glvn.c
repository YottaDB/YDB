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
#include "toktyp.h"
#include "mdq.h"
#include "fullbool.h"

error_def(ERR_VAREXPECTED);

int glvn(oprtype *a)
{
	triple		*oldchain, *ref;
	oprtype		x1;
	save_se		save_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch (TREF(window_token))
	{
	case TK_IDENT:
		if (!lvn(a,OC_GETINDX,0))
			return FALSE;
		return TRUE;
	case TK_CIRCUMFLEX:
		if (!gvn())
			return FALSE;
		*a = put_tref(newtriple(OC_GVGET));
		return TRUE;
	case TK_ATSIGN:
		if (SHIFT_SIDE_EFFECTS)
		{
			START_GVBIND_CHAIN(&save_state, oldchain);
			if (!indirection(&x1))
			{
				setcurtchain(oldchain);
				return FALSE;
			}
			ref = newtriple(OC_INDGLVN);
			PLACE_GVBIND_CHAIN(&save_state, oldchain);
		} else
		{
			if (!indirection(&x1))
				return FALSE;
			ref = newtriple(OC_INDGLVN);
		}
		if (TREF(expr_depth))
			(TREF(side_effect_base))[TREF(expr_depth)] = (OLD_SE != TREF(side_effect_handling));
		ref->operand[0] = x1;
		*a = put_tref(ref);
		return TRUE;
	default:
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
}
