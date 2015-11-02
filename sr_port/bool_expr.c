/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"		/* needed by INCREMENT_EXPR_DEPTH */
#include "compiler.h"
#include "opcode.h"

int bool_expr(boolean_t op, oprtype *addr)
{
	oprtype x;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	INCREMENT_EXPR_DEPTH;
	if (!eval_expr(&x))
	{
		DECREMENT_EXPR_DEPTH;
		return FALSE;
	}
	assert(TRIP_REF == x.oprclass);
	coerce(&x, OCT_BOOL);
	bx_tail(x.oprval.tref, op, addr);
	DECREMENT_EXPR_DEPTH;
	return TRUE;
}
