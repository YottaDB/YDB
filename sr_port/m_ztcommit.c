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

int m_ztcommit(void)
{
	triple *head;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	head = maketriple(OC_ZTCOMMIT);
	if ((TK_EOL == TREF(window_token)) || (TK_SPACE == TREF(window_token)))
	{
		head->operand[0] = put_ilit(1);
		ins_triple(head);
		return TRUE;
	}
	if (EXPR_FAIL == expr(&head->operand[0], MUMPS_INT))
		return FALSE;
	ins_triple(head);
	return TRUE;
}
