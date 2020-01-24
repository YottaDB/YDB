/****************************************************************
 *								*
* Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	*
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
#include "opcode.h"
#include "toktyp.h"
#include "advancewindow.h"

/* This is the compiler routine for $ZYHASH which exposes the MurmurHash3 routine to M code. It takes 2 parameters:
 * string: the string to hash
 * salt: an optional salt for the hash that defaults to 0 if not present
 */
int f_zyhash(oprtype *a, opctype op)
{
	triple	*r;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
		return FALSE;
	if (TK_COMMA != TREF(window_token))
		r->operand[1] = put_ilit(0);
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(r->operand[1]), MUMPS_INT))
			return FALSE;
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
