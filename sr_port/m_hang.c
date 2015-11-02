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
#include "indir_enum.h"
#include "toktyp.h"
#include "cmd.h"

int m_hang(void)
{
	oprtype ot;
	triple *triptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch (expr(&ot, MUMPS_NUM))
	{
	case EXPR_FAIL:
		return FALSE;
	case EXPR_GOOD:
		triptr = newtriple(OC_HANG);
		triptr->operand[0] = ot;
		return TRUE;
	case EXPR_INDR:
		make_commarg(&ot, indir_hang);
		return TRUE;
	default:
		GTMASSERT;
	}
	return FALSE; /* This should never get executed, added to make compiler happy */
}
