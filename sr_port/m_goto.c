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
#include "mdq.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "cmd.h"

int m_goto(void)
/* compiler module for (ugh!) GOTO */
{
	triple	*obp, *oldchain, tmpchain;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(xecute_literal_parse))
		return FALSE;
	dqinit(&tmpchain, exorder);
	oldchain = setcurtchain(&tmpchain);
	if (!entryref(OC_JMP, OC_EXTJMP, (mint)indir_goto, TRUE, FALSE, FALSE))
	{
		setcurtchain(oldchain);
		return FALSE;
	}
	setcurtchain(oldchain);
	if (TK_COLON == TREF(window_token))
		return m_goto_postcond(oldchain, &tmpchain);		/* post conditional expression */
	obp = oldchain->exorder.bl;
	dqadd(obp, &tmpchain, exorder);					/*this violates info hiding*/
	return TRUE;
}
