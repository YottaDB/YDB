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

#include "opcode.h"
#include "compiler.h"

/* This function is very similar to `put_ilit()` except that it does a `maketriple()` instead of `newtriple()`.
 * That is, it does not insert the newly made triple in the execution chain. This is relied upon by a few callers.
 */
oprtype make_ilit(mint x)
{
	triple *ref;

	ref = maketriple(OC_ILIT);
	ref->operand[0].oprclass = ILIT_REF;
	ref->operand[0].oprval.ilit = x;
	return put_tref(ref);
}
