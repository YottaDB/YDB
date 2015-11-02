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

int f_mstr(oprtype *a, opctype op)
{
	triple *r;

	r = maketriple(op);
	if (EXPR_FAIL == expr(&r->operand[0], MUMPS_STR))
		return FALSE;
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
