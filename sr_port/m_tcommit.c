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
#include "cmd.h"

error_def(ERR_SPOREOL);

int m_tcommit(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TK_EOL != (TREF(window_token)) && (TK_SPACE != TREF(window_token)))
	{
		stx_error(ERR_SPOREOL);
		return FALSE;
	}
	newtriple(OC_TCOMMIT);
	return TRUE;
}
