/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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

GBLREF int4 source_error_found;
GBLREF char window_token;
GBLREF triple *curtchain;

int linetail(void)
{
	error_def(ERR_SPOREOL);
	error_def(ERR_CMD);

	for (;;)
	{
		while (window_token == TK_SPACE)
			advancewindow();
		if (window_token == TK_EOL)
			return TRUE;
		if (!cmd())
		{
			if (curtchain->exorder.bl->exorder.bl->exorder.bl->opcode != OC_RTERROR)
			{	/* If rterror is last triple generated (has two args), then error already raised */
				source_error_found ? stx_error(source_error_found) : stx_error(ERR_CMD);
			}
			assert(curtchain->exorder.bl->exorder.fl == curtchain);
			assert(source_error_found);
			return FALSE;
		}
		if (window_token != TK_SPACE && window_token != TK_EOL)
		{
			stx_error(ERR_SPOREOL);
			return FALSE;
		}
	}
}
