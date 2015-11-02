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
#include "indir_enum.h"
#include "opcode.h"
#include "toktyp.h"
#include "cmd.h"

int m_trollback(void)
{
	oprtype	x;
	triple	*ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TK_SPACE == TREF(window_token)) || (TK_EOL == TREF(window_token)))
	{
		ref = newtriple(OC_TROLLBACK);
		ref->operand[0] = put_ilit(0);
		return TRUE;
	} else
	{
		switch (expr(&x, MUMPS_INT))
		{
		case EXPR_FAIL:
			return FALSE;
		case EXPR_GOOD:
			ref = newtriple(OC_TROLLBACK);
			ref->operand[0] = x;
			return TRUE;
		case EXPR_INDR:
			make_commarg(&x,indir_trollback);
			return TRUE;
		}
	}
	return FALSE; /* This should never get executed, added to make compiler happy */
}
