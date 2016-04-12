/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "mmemory.h"
#include "cmd.h"
#include "indir_enum.h"
#include "advancewindow.h"

GBLREF	boolean_t	run_time;

error_def(ERR_ALIASEXPECTED);
error_def(ERR_QUITARGLST);
error_def(ERR_QUITARGUSE);

int m_quit(void)
{
	boolean_t	arg;
	int		rval;
	mvar		*mvarptr;
	oprtype		tmparg, x;
	triple		*r, *triptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	arg = ((TK_EOL != TREF(window_token)) && (TK_SPACE != TREF(window_token)));
	if (TREF(for_stack_ptr) == TADR(for_stack))
	{	/* not FOR */
		if (!arg)
		{
			newtriple((run_time) ? OC_HARDRET : OC_RET);
			return TRUE;
		}
		/* We now know we have an arg. See if it is an alias indicated arg */
		if (TK_ASTERISK == TREF(window_token))
		{	/* We have QUIT * alias syntax */
			advancewindow();
			if (TK_IDENT == TREF(window_token))
			{	/* Both alias and alias container sources go through here */
				if (!lvn(&tmparg, OC_GETINDX, 0))
					return FALSE;
				r = newtriple(OC_RETARG);
				r->operand[0] = tmparg;
				r->operand[1] = put_ilit(TRUE);
				return TRUE;
			} else
			{	/* Unexpected text after alias indicator */
				stx_error(ERR_ALIASEXPECTED);
				return FALSE;
			}
		} else if (EXPR_FAIL != (rval = expr(&x, MUMPS_EXPR)) && (TK_COMMA != TREF(window_token))) /* NOTE assignment */
		{
			if (EXPR_INDR != rval)
			{
				r = newtriple(OC_RETARG);
				r->operand[0] = x;
				r->operand[1] = put_ilit(FALSE);
			} else	/* Indirect argument */
				make_commarg(&x, indir_quit);
			return TRUE;
		}
		if (TK_COMMA == TREF(window_token))
			stx_error (ERR_QUITARGLST);
		return FALSE;
	} else if (!arg)						/* FOR */
	{
		triptr = newtriple(OC_JMP);
		FOR_END_OF_SCOPE(1, triptr->operand[0]);
		return TRUE;
	}
	stx_error(ERR_QUITARGUSE);
	return FALSE;
}
