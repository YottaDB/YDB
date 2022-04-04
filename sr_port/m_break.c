/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
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
#include "opcode.h"
#include "toktyp.h"
#include "cmd.h"
#include "start_fetches.h"

int m_break(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TK_SPACE != TREF(window_token)) && (TK_EOL != TREF(window_token)))
		if (!m_xecute())
			return FALSE;
	newtriple(OC_BREAK);
	/* Because the activity in direct mode brought on by the BREAK can alter the environment, start a new fetch */
	MID_LINE_REFETCH;
	return TRUE;
}
