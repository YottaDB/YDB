/****************************************************************
 *								*
 * Copyright (c) 2011-2017 Fidelity National Information	*
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
#include "indir_enum.h"
#include "opcode.h"
#include "cmd.h"
#include "toktyp.h"

/* Halt the process similar to HALT but ZHALT allows specification of a return code. If the command does not specify a return
 * code, this uses 0 as a default, so that case appears the same to the shell as HALT. However, they are subject to different
 * potential restrictions, so they use a flag that separates the two.
 */
int m_zhalt(void)
{
	triple	*triptr;
	oprtype	ot;
	int	status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TK_SPACE == TREF(window_token)) || (TK_EOL == TREF(window_token)))
	{	/* no argument means return 0 */
		triptr = newtriple(OC_ZHALT);
		triptr->operand[0] = put_ilit(1);
		triptr->operand[1] = put_ilit(0);
		return TRUE;
	}
	switch (status = expr(&ot, MUMPS_INT))		/* NOTE assignment */
	{
		case EXPR_FAIL:
			return FALSE;
		case EXPR_GOOD:
			triptr = newtriple(OC_ZHALT);
			triptr->operand[0] = put_ilit(1);
			triptr->operand[1] = ot;
			return TRUE;
		case EXPR_INDR:
			make_commarg(&ot, indir_zhalt);
			return TRUE;
		default:
			assertpro(FALSE);
	}
	return FALSE; /* This should never get executed, added to make compiler happy */
}
