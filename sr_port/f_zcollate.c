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

/* $ZCOLLATE(strexp, int1, int2)
 *
 * Parameters:
 *
 * 	strexpr - string experssion containing global variable name (GVN) or a GVN in database internal key format (GDS).
 * 	int1	- collatio algorithm index.
 * 	int2    - if not specified or 0 translation is from GVN to GDS. Else, translation is from GDS to GVN.
 *
 * Return value - the converted name.
 *
 * Note that GDS is the internal database key format and GVN is a formatted global variable name with subscripts.
 * Note that if the collation isn't available, we raise a COLLATIONUNDEF error.
 */
int f_zcollate(oprtype *a, opctype op)
{
	triple 	*gvn, *coll;
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
	if (TK_COMMA != TREF(window_token))
		coll->operand[1] = put_ilit(0);
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(coll->operand[1]), MUMPS_INT))
			return FALSE;
	}
	ins_triple(gvn);
	*a = put_tref(gvn);
	return TRUE;
}
