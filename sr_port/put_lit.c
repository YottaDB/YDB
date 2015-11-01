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
#include "mdq.h"
#include "compiler.h"
#include "opcode.h"
#include "mmemory.h"

GBLREF int mlitmax;
GBLREF mliteral literal_chain;

oprtype put_lit(mval *x)
{
	mliteral *a;
	triple *ref;

	ref = newtriple(OC_LIT);
	ref->operand[0].oprclass = MLIT_REF;
	dqloop(&literal_chain,que,a)
		if (is_equ(x,&(a->v)))
		{
			a->rt_addr--;
			ref->operand[0].oprval.mlit = a;
			return put_tref(ref);
		}
	ref->operand[0].oprval.mlit = a = (mliteral *) mcalloc(sizeof(mliteral));
	dqins(&literal_chain,que,a);
	a->rt_addr = -1;
	a->v = *x;
	mlitmax++;
	return put_tref(ref);
}
