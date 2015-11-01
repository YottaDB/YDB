/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#include "cmd.h"
#include "indir_enum.h"

GBLREF	char		window_token;
GBLREF	bool		run_time;
GBLREF	oprtype		*for_stack[], **for_stack_ptr;

int m_quit(void)
{
	int	rval;
	triple	*triptr;
	triple	*r;
	oprtype	x;
	error_def(ERR_QUITARGUSE);
	error_def(ERR_QUITARGLST);

	if (for_stack_ptr == for_stack)
	{
		if (window_token == TK_EOL || window_token == TK_SPACE)
			newtriple((run_time) ? OC_HARDRET : OC_RET);
		else
		{
			if (!(rval = expr(&x)))
				return FALSE;
			if (EXPR_INDR == rval)
			{	/* Indirect argument */
				make_commarg(&x, indir_quit);
				return TRUE;
			}
			r = newtriple(OC_RETARG);
			r->operand[0] = x;
			if (window_token == TK_COMMA)
			{
				stx_error (ERR_QUITARGLST);
				return FALSE;
			}
		}
	} else
	{
		if (window_token == TK_EOL || window_token == TK_SPACE)
		{
			triptr = newtriple(OC_JMP);
			triptr->operand[0] = for_end_of_scope(1);
		} else
		{
			stx_error(ERR_QUITARGUSE);
			return FALSE;
		}
	}
	return TRUE;
}
