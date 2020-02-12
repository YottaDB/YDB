/****************************************************************
 *								*
* Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
* All rights reserved.						*
*								*
*	This source code contains the intellectual property	*
*	of its copyright holder(s), and is made available	*
*	under a license.  If you do not know the terms of	*
*	the license, please stop and do not read further.	*
*								*
****************************************************************/

#include "mdef.h"

#include "compiler.h"

int f_zyissqlnull(oprtype *a, opctype op)
{
	triple	*r;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
		return FALSE;
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
