/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "gtm_string.h"
#include "mdef.h"
#include "compiler.h"
#include "mdq.h"
#include "opcode.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "op.h"

LITREF mval literal_null;

#define	MAX_ZREPLACE_ARGS	3

int f_replace(oprtype *a, opctype op)
{
	boolean_t	more_args;
	int4		i;
	triple		*args[MAX_ZREPLACE_ARGS];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	args[0] = maketriple(op);
	if (EXPR_FAIL == expr(&(args[0]->operand[0]), MUMPS_EXPR))
		return FALSE;
	for (i = 1, more_args = TRUE; i < MAX_ZREPLACE_ARGS; i++)
	{
		args[i] = newtriple(OC_PARAMETER);
		if (more_args)
		{
			if (TK_COMMA != TREF(window_token))
				more_args = FALSE;
			else
			{
				advancewindow();
				if (EXPR_FAIL == expr(&(args[i]->operand[0]), MUMPS_EXPR))
					return FALSE;
			}
		}
		if (!more_args)
			args[i]->operand[0] = put_lit((mval *)&literal_null);
		args[i - 1]->operand[1] = put_tref(args[i]);
	}
	assert((TRIP_REF == args[0]->operand[1].oprclass) && (TRIP_REF == args[0]->operand[1].oprval.tref->operand[0].oprclass)
			&& (TRIP_REF == args[0]->operand[1].oprval.tref->operand[1].oprclass));
	ins_triple(args[0]);
	*a = put_tref(args[0]);
	return TRUE;
}
