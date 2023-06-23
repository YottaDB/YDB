/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

LITREF mval literal_null ;

int f_zdate(oprtype *a, opctype op) /* op is not used */
{
	boolean_t	more_args;
	int		i;
	triple		*args[4];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	args[0] = maketriple(OC_FNZDATE);
	if (EXPR_FAIL == expr(&(args[0]->operand[0]), MUMPS_EXPR))
		return FALSE;
	for (i = 1 , more_args = TRUE ; i < 4 ; i++)
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
	ins_triple(args[0]);
	*a = put_tref(args[0]);
	return TRUE;
}
