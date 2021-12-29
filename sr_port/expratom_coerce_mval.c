/****************************************************************
 *								*
 * Copyright (c) 2021 YottaDB LLC and/or its subsidiaries.	*
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

LITREF octabstruct     oc_tab[];

/* This function is very similar to "expratom()" except that in this case, we know the result operand (stored in "*a")
 * needs to be coerced into an mval type and unary NOT operation handling needs to be done by doing appropriate
 * boolean opcode insertions (all done by "ex_tail()"). This function is currently invoked in 2 places.
 * 1) For an extended reference where we allow for an alternate non-standard syntax using "^[...]x" syntax.
 * 2) For indirection processing in "sr_port/indirection.c".
 *
 * Output parameter
 * ----------------
 * "a" : Is a pointer to a structure that gets filled in and holds a pointer to the triple corresponding to the
 * currently processed expression atom (referenced by "TREF(window_token)").
 */
int expratom_coerce_mval(oprtype *a)
{
	int	retval;

	retval = expratom(a);
	if (!retval)
		return retval;
	coerce(a, OCT_MVAL);
	ex_tail(a, 0);
	return retval;
}
