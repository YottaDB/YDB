/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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

GBLREF	char		window_token;
GBLREF	boolean_t	run_time;
GBLREF	oprtype		*for_stack[], **for_stack_ptr;
GBLREF	triple 		*curtchain;
GBLREF	boolean_t	dollar_zquit_anyway;

LITREF	mval		literal_null;

int m_quit(void)
{
	boolean_t	arg;
	int		rval;
	triple		*triptr;
	triple		*r;
	oprtype		x,*cr;
	error_def(ERR_QUITARGUSE);
	error_def(ERR_QUITARGLST);

	arg = (window_token != TK_EOL && window_token != TK_SPACE);
	if (for_stack_ptr == for_stack)
	{	/* not FOR */
		if (dollar_zquit_anyway && !run_time)
		{	/* turn a plain quit into a set with and without argument conditioned on $QUIT */
			r = newtriple(OC_SVGET);
			r->operand[0] = put_ilit(SV_QUIT);
			x = put_tref(r);
			coerce(&x, OCT_BOOL);
			cr = (oprtype *)mcalloc(sizeof(oprtype));	/* for jump target */
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
				x = put_tref(r);
				return TRUE;
			}
		}
		if (!arg)
		{
			newtriple((run_time) ? OC_HARDRET : OC_RET);
			return TRUE;
		}
		if ((rval = expr(&x)) && (window_token != TK_COMMA))
		{
			if (EXPR_INDR != rval)
			{
				r = newtriple(OC_RETARG);
				r->operand[0] = x;
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
		triptr->operand[0] = for_end_of_scope(1);
		return TRUE;
	}
	stx_error(ERR_QUITARGUSE);
	return FALSE;
}
