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
#include "toktyp.h"
#include "opcode.h"
#include "advancewindow.h"
#include "cmd.h"

GBLREF char window_token;
GBLREF triple *curtchain;

error_def(ERR_SPOREOL);
error_def(ERR_CMD);

int linetail(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for (;;)
	{
		while (TK_SPACE == window_token)
			advancewindow();
		if (TK_EOL == window_token)
			return TRUE;
		if (!cmd())
		{
			if (curtchain->exorder.bl->exorder.bl->exorder.bl->opcode != OC_RTERROR)
			{	/* If rterror is last triple generated (has two args), then error already raised */
				TREF(source_error_found) ? stx_error(TREF(source_error_found)) : stx_error(ERR_CMD);
			}
			assert(curtchain->exorder.bl->exorder.fl == curtchain);
			assert(TREF(source_error_found));
			return FALSE;
		}
		if ((TK_SPACE != window_token) && (TK_EOL != window_token))
		{
			stx_error(ERR_SPOREOL);
			return FALSE;
		}
	}
}
