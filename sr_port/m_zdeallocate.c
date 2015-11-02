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
#include "toktyp.h"
#include "iotimer.h"
#include "indir_enum.h"
#include "advancewindow.h"
#include "cmd.h"

error_def(ERR_RPARENMISSING);

int m_zdeallocate(void)
{

	oprtype		indopr;
	triple		*ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	newtriple(OC_LKINIT);
	switch(TREF(window_token))
	{
	case TK_EOL:
	case TK_SPACE:
		break;
	case TK_ATSIGN:
		if (!indirection(&indopr))
			return FALSE;
		ref = newtriple(OC_COMMARG);
		ref->operand[0] = indopr;
		ref->operand[1] = put_ilit((mint)indir_zdeallocate);
		return TRUE;
		break;
	case TK_LPAREN:
		do
		{
			advancewindow();
			if (EXPR_FAIL == nref())
				return FALSE;
		} while (TK_COMMA == TREF(window_token));
		if (TK_RPAREN != TREF(window_token))
		{
			stx_error(ERR_RPARENMISSING);
			return FALSE;
		}
		advancewindow();
		break;
	default:
		if (EXPR_FAIL == nref())
			return FALSE;
		break;
	}
	ref = newtriple(OC_ZDEALLOCATE);
	ref->operand[0] = put_ilit(NO_M_TIMEOUT);
	return EXPR_GOOD;
}
