/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
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
#include "advancewindow.h"

/* $ZJOBEXAM([strexp1[,strexp2]])
 * Parameters:
 *	strexp1 - (optional) output device/file specification
 *	strexp2 - (optional) zshow information code specification
*/
int f_zjobexam(oprtype *a, opctype op)
{
	triple		*args[2];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	args[0] = maketriple(op);
	if (TK_RPAREN == TREF(window_token))
		args[0]->operand[0] = put_str("",0);	/* No argument specified - default to null */
	else if (EXPR_FAIL == expr(&(args[0]->operand[0]), MUMPS_STR))
		return FALSE;	/* Improper string argument */

	/* Look for optional 2nd argument */
	args[1] = newtriple(OC_PARAMETER);
	if (TK_COMMA == TREF(window_token))
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(args[1]->operand[0]), MUMPS_STR))
			return FALSE;
	}
	else
		args[1]->operand[0] = put_str("",0);

	args[0]->operand[1] = put_tref(args[1]);
	ins_triple(args[0]);
	*a = put_tref(args[0]);
	return TRUE;
}
