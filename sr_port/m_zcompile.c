/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "cmd.h"

int m_zcompile(void)
{
	triple	*triptr;
	oprtype	x;

	if(EXPR_FAIL == expr(&x, MUMPS_STR))
		return FALSE;
	triptr = newtriple(OC_ZCOMPILE);
	triptr->operand[0] = x;
	triptr->operand[1] = put_ilit(FALSE);	/* ignore_dollar_zcompile arg */
	return TRUE;
}
