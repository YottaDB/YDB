/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "mmemory.h"

void bx_boolop(triple *t, bool jmp_type_one, bool jmp_to_next, bool sense, oprtype *addr)
{
	oprtype *p;

	if (jmp_to_next)
	{
		p = (oprtype *) mcalloc(SIZEOF(oprtype));
		*p = put_tjmp(t);
	}
	else
	{
		p = addr;
	}
	bx_tail(t->operand[0].oprval.tref, jmp_type_one, p);
	bx_tail(t->operand[1].oprval.tref, sense, addr);
	t->opcode = OC_NOOP;
	t->operand[0].oprclass = t->operand[1].oprclass = 0;
	return;
}
