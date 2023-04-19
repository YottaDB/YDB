/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
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
#include "opcode.h"
#include "fullbool.h"

LITREF octabstruct	oc_tab[];

void bx_relop(triple *t, opctype cmp, opctype tst, oprtype *addr)
/* Convert boolean relational operation into final cmp-jmp form.
 * *t points to the Boolean operation, cmp and tst give (respectively) the opcode and the associated jump
 * *addr points the operand for the jump and is eventually used by logic back in the invocation stack to fill in a target location
 */
{
	triple	*ref;

	ref = maketriple(tst);
	ref->operand[0] = put_indr(addr);
	dqins(t, exorder, ref);
	t->opcode = cmp;
	return;
}
