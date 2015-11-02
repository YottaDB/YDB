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
#include "mmemory.h"
#include "cmd.h"
#include "indir_enum.h"
#include "svnames.h"
#include "advancewindow.h"

GBLREF	char		window_token;
GBLREF	boolean_t	run_time;
GBLREF	triple 		*curtchain;
GBLREF	boolean_t	dollar_zquit_anyway;

error_def(ERR_ALIASEXPECTED);
error_def(ERR_QUITARGUSE);
error_def(ERR_QUITARGLST);

LITREF	mval		literal_null;

int m_quit(void)
{
	boolean_t	arg;
	int		rval;
	triple		*triptr;
	triple		*r;
	oprtype		x, *cr, tmparg;
	mvar		*mvarptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	arg = ((TK_EOL != window_token) && (TK_SPACE != window_token));
	if (TREF(for_stack_ptr) == TADR(for_stack))
	{	/* not FOR */
		if (dollar_zquit_anyway && !run_time)
		{	/* turn a plain quit into a set with and without argument conditioned on $QUIT */
			r = newtriple(OC_SVGET);
			r->operand[0] = put_ilit(SV_QUIT);
			x = put_tref(r);
			coerce(&x, OCT_BOOL);
			cr = (oprtype *)mcalloc(SIZEOF(oprtype));	/* for jump target */
			bx_tail(x.oprval.tref, (bool)TRUE, cr);
			r = newtriple(OC_RET);
			x = put_tref(r);
			r = newtriple(OC_NOOP);				/* need a jump target */
			x = put_tref(r);
			*cr = put_tjmp(curtchain->exorder.bl);
			if (!arg)
			{
				r = newtriple(OC_RETARG);
				r->operand[0] = put_lit((mval *)&literal_null);
				r->operand[1] = put_ilit(FALSE);
				x = put_tref(r);
				return TRUE;
			}
		}
		if (!arg)
		{
			newtriple((run_time) ? OC_HARDRET : OC_RET);
			return TRUE;
		}
		/* We now know we have an arg. See if it is an alias indicated arg */
		if (TK_ASTERISK == window_token)
		{	/* We have QUIT * alias syntax */
			advancewindow();
			if (TK_IDENT == window_token)
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
		} else if ((rval = expr(&x)) && (TK_COMMA != window_token))
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
		if (window_token == TK_COMMA)
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
