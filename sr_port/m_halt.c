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
#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "cmd.h"

int  m_halt(void)
{
	triple	*triptr;

	triptr = newtriple(OC_ZHALT);
	triptr->operand[0] = put_ilit(0);				/* flag as HALT rather than ZHALT */
	triptr->operand[1] = put_ilit(0);				/* return from HALT is always "success" */
	return TRUE;
}
