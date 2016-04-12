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
#include "mmemory.h"
#include "cmd.h"

error_def(ERR_SPOREOL);

int m_else(void)
{
	triple	*jmpref, elsepos_in_chain;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	elsepos_in_chain = TREF(pos_in_chain);
	if (TK_EOL == TREF(window_token))
		return TRUE;
	if (TK_SPACE != TREF(window_token))
	{
		stx_error(ERR_SPOREOL);
		return FALSE;
	}
	jmpref = newtriple(OC_JMPTSET);
	FOR_END_OF_SCOPE(0, jmpref->operand[0]);
	if (!linetail())
	{	tnxtarg(&jmpref->operand[0]);
		TREF(pos_in_chain) = elsepos_in_chain;
		return FALSE;
	} else
		return TRUE;
}
