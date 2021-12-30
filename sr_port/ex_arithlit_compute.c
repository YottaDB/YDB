/****************************************************************
 *								*
 * Copyright (c) 2017-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 * Portions Copyright (c) 2001-2019 Fidelity National		*
 * Information Services, Inc. and/or its subsidiaries.		*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* While FIS did not write this module, it was derived from FIS code in "sr_port/ex_tail.c"
 * that took care of compile-time literal optimization in case of an OCT_ARITH opcode type.
 */

#include "mdef.h"

#include "compiler.h"
#include "error.h"
#include "mmemory.h"
#include "op.h"
#include "flt_mod.h"

/* Given a binary arithmetic operation opcode "c" and its corresponding 2 operands "v0" and "v1" which are both
 * literals, this function does the arithmetic operation and returns a pointer to the result mval. It takes care
 * of any runtime errors by using a condition handler to catch the error and return control to this function.
 */
mval *ex_arithlit_compute(opctype c, mval *v0, mval *v1)
{
	mval	*v;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Establish condition handler "ex_arithlit_compute_ch" with "v" set to NULL. This means if ever the
	 * condition handler is invoked (due to a runtime error inside one of the calls later in this function)
	 * it would do an UNWIND call and return from this function with a value of "v" = NULL. Since we do
	 * a set of "v" to a non-NULL value ("mcalloc" call below) right after the "ESTABLISH_RET" call, we
	 * would return from this function with a non-NULL "v" in case the condition handler did not get called
	 * (i.e. in case there was no runtime error inside any of the calls below).
	 */
	v = NULL;
	ESTABLISH_RET(ex_arithlit_compute_ch, v);
	v = (mval *)mcalloc(SIZEOF(mval));
	switch (c)
	{
	case OC_ADD:
		op_add(v0, v1, v);
		break;
	case OC_SUB:
		op_sub(v0, v1, v);
		break;
	case OC_MUL:
		op_mul(v0, v1, v);
		break;
	case OC_DIV:
		op_div(v0, v1, v);
		break;
	case OC_IDIV:
		op_idiv(v0, v1, v);
		break;
	case OC_MOD:
		flt_mod(v0, v1, v);
		break;
	case OC_EXP:
		op_exp(v0, v1, v);
		break;
	default:
		assertpro(FALSE);
		break;
	}
	REVERT;
	return v;
}
