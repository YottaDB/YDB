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
#include "iotimer.h"
#include "indir_enum.h"
#include "advancewindow.h"
#include "cmd.h"

GBLREF char window_token;

int m_zallocate(void)
{

	triple *ref;
	oprtype indopr;
	bool indirect;

	error_def(ERR_RPARENMISSING);

	newtriple(OC_RESTARTPC);
	indirect = FALSE;
	newtriple(OC_LKINIT);
	switch(window_token)
	{
		case TK_ATSIGN:
			if (!indirection(&indopr))
				return FALSE;
			ref = newtriple(OC_COMMARG);
			ref->operand[0] = indopr;
			if (TK_COLON != window_token)
			{
				ref->operand[1] = put_ilit((mint)indir_zallocate);
				return TRUE;
			}
			ref->operand[1] = put_ilit((mint)indir_nref);
			indirect = TRUE;
			break;
		case TK_LPAREN:
			do
			{
				advancewindow();
				if (EXPR_FAIL == nref())
					return FALSE;
			} while (TK_COMMA == window_token);
			if (TK_RPAREN != window_token)
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
	ref = maketriple(OC_ZALLOCATE);
	if (TK_COLON != window_token)
	{
		ref->operand[0] = put_ilit(NO_M_TIMEOUT);
		ins_triple(ref);
	} else
	{
		advancewindow();
		if (!intexpr(&(ref->operand[0])))
			return EXPR_FAIL;
		ins_triple(ref);
		newtriple(OC_TIMTRU);
	}
	return EXPR_GOOD;
}
