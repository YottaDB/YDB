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
#include "mdq.h"

LITREF octabstruct	oc_tab[];

void bx_relop(triple *t, opctype cmp, opctype tst, oprtype *addr)
/* work Boolean relational arguments
 * *t points to the Boolean operation
 * cmp and tst give (respectively) the opcode and the associated jump
 * *addr points the operand for the jump and is eventually used by logic back in the invocation stack to fill in a target location
 */
{
	oprtype	*p;
	triple	*ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ref = maketriple(tst);
	ref->operand[0] = put_indr(addr);
	dqins(t, exorder, ref);
	t->opcode = cmp;
	for (p = t->operand ; p < ARRAYTOP(t->operand); p++)
	{	/* Some day investigate whether this is still needed */
		if (TRIP_REF == p->oprclass)
		{
			ex_tail(p);
			RETURN_IF_RTS_ERROR;
		}
	}
	return;
}
