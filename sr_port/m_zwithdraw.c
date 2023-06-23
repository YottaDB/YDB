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
#include "indir_enum.h"
#include "toktyp.h"
#include "cmd.h"

error_def(ERR_VAREXPECTED);

int m_zwithdraw(void)
{
	oprtype tmparg;
	triple *ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch (TREF(window_token))
	{
	case TK_IDENT:
		if (!lvn(&tmparg,OC_SRCHINDX,0))
			return FALSE;
		ref = newtriple(OC_LVZWITHDRAW);
		ref->operand[0] = tmparg;
		break;
	case TK_CIRCUMFLEX:
		if (!gvn())
			return FALSE;
		ref = newtriple(OC_GVZWITHDRAW);
		break;
	case TK_ATSIGN:
		if (!indirection(&tmparg))
			return FALSE;
		ref = maketriple(OC_COMMARG);
		ref->operand[0] = tmparg;
		ref->operand[1] = put_ilit((mint) indir_zwithdraw);
		ins_triple(ref);
		return TRUE;
	default:
		stx_error(ERR_VAREXPECTED);
		return FALSE;
	}
	return TRUE;
}
