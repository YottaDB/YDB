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

int m_ztcommit(void)
{
	triple *head;

	head = maketriple(OC_ZTCOMMIT);
	if (window_token == TK_EOL || window_token == TK_SPACE)
	{
		head->operand[0] = put_ilit(1);
		ins_triple(head);
		return TRUE;
	}

	if (!intexpr(&head->operand[0]))
		return FALSE;

	ins_triple(head);
	return TRUE;
}
