/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "indir_enum.h"
#include "toktyp.h"
#include "cmd.h"

GBLREF char	window_token;

int m_zsystem(void)
{
	oprtype	x;
	triple	*triptr;

	if (window_token == TK_EOL || window_token == TK_SPACE)
	{
		triptr = newtriple(OC_ZSYSTEM);
		triptr->operand[0] = put_str("",0);
		return TRUE;
	}
	else
	switch (strexpr(&x))
	{
	case EXPR_FAIL:
		return FALSE;
	case EXPR_GOOD:
		triptr = newtriple(OC_ZSYSTEM);
		triptr->operand[0] = x;
		return TRUE;
	case EXPR_INDR:
		make_commarg(&x,indir_zsystem);
		return TRUE;
	}

	return FALSE; /* This will never get executed, added to make compiler happy */
}
