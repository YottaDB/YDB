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
#include "mdq.h"

void bx_relop(triple *t, opctype cmp, opctype tst, oprtype *addr)
{
	triple *ref;

	ref = maketriple(tst);
	ref->operand[0] = put_indr(addr);
	dqins(t, exorder, ref);
	t->opcode = cmp;
	return;
}
