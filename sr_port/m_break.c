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
GBLREF oprtype	*for_stack[], **for_stack_ptr;

int m_break(void)
{
	if (window_token != TK_SPACE && window_token != TK_EOL)
		if (!m_xecute())
			return FALSE;
	newtriple(OC_BREAK);
	if (for_stack_ptr == for_stack)
		start_fetches (OC_FETCH);
	else
		start_for_fetches ();
	return TRUE;
}
