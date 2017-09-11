/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

error_def(ERR_CMD);
error_def(ERR_SPOREOL);

int linetail(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TREF(rts_error_in_parse) = FALSE;
	for (;;)
	{
		while (TK_SPACE == TREF(window_token))
			advancewindow();
		if (TK_EOL == TREF(window_token))
			return TRUE;
		if (!cmd())
		{
			if (!ALREADY_RTERROR)
				TREF(source_error_found) ? stx_error(TREF(source_error_found)) : stx_error(ERR_CMD);
			assert((TREF(curtchain))->exorder.bl->exorder.fl == TREF(curtchain));
			assert(TREF(source_error_found));
			return FALSE;
		}
		if ((TK_SPACE != TREF(window_token)) && (TK_EOL != TREF(window_token)))
		{
			stx_error(ERR_SPOREOL);
			return FALSE;
		}
	}
}
