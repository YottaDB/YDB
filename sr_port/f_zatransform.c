/****************************************************************
 *								*
 * Copyright (c) 2006-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "stringpool.h"
#include "advancewindow.h"

/* $ZATRANSFORM(strexp, int1, int2)
 * Convert an MVAL to it's GDS internal collated form for comparison using either the follows or sorts-after operators
 *
 * Has the following input parameters:
 *
 * 	expr	- REQUIRED - an expression containing valid MVAL or an MVAL in database internal key format (GDS).
 * 	int1	- REQUIRED - collation algorithm index.
 * 	int2    - OPTIONAL - if not specified or 0 translation is from MVAL to GDS. Else, translation is from GDS to
 * 				MVAL.
 * 	int3    - OPTIONAL - if not specified or 0 treat the a canonical numeric strexpr as if it were a numeric
 *				subscript; else treat it only as a string
 *
 * Return value - the converted string
 *
 * Note this function is meant as a proxy to mval2subs to convert a string to/from an internal subscript representation
 * Note that if the collation isn't available, we raise a COLLATIONUNDEF error.
 */
int f_zatransform(oprtype *a, opctype op)
{
	triple 	*gvn, *coll, *optional;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	gvn = maketriple(op);
	if (EXPR_FAIL == expr(&(gvn->operand[0]), MUMPS_STR))
	    return FALSE;
	if (TK_COMMA != TREF(window_token))
	    return FALSE;
	advancewindow();
	/* 2nd parameter (required) */
	coll = newtriple(OC_PARAMETER);
	gvn->operand[1] = put_tref(coll);
	if (EXPR_FAIL == expr(&(coll->operand[0]), MUMPS_INT))
	    return FALSE;
	/* 3rd parameter (optional), defaults to 0 */
	optional = newtriple(OC_PARAMETER);
	coll->operand[1] = put_tref(optional);
	if (TK_COMMA != TREF(window_token))
		optional->operand[0] = put_ilit(0);
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(optional->operand[0]), MUMPS_INT))
			return FALSE;
	}
	/* 4th parameter (optional), defaults to 0 */
	if (TK_COMMA != TREF(window_token))
		optional->operand[1] = put_ilit(0);
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(optional->operand[1]), MUMPS_INT))
			return FALSE;
	}
	ins_triple(gvn);
	*a = put_tref(gvn);
	return TRUE;
}

