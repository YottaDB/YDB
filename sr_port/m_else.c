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
#include "cmd.h"

GBLREF char window_token;
GBLREF triple pos_in_chain;

int m_else(void)
{
	error_def(ERR_SPOREOL);
	triple	*jmpref, elsepos_in_chain;

	elsepos_in_chain = pos_in_chain;
	if (window_token == TK_EOL)
		return TRUE;
	if (window_token != TK_SPACE)
	{
		stx_error(ERR_SPOREOL);
		return FALSE;
	}
	jmpref = newtriple(OC_JMPTSET);
	jmpref->operand[0] = for_end_of_scope(0);
	if (!linetail())
	{	tnxtarg(&jmpref->operand[0]);
		pos_in_chain = elsepos_in_chain;
		return FALSE;
	}
	else
	{	return TRUE;
	}
}
