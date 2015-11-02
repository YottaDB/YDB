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

	if (for_stack_ptr == for_stack)
	{
		arg = (window_token != TK_EOL && window_token != TK_SPACE);
		cr = NULL;
		if (dollar_zquit_anyway && !run_time)
		{	/* turn a plain quit into a set with and without argument conditioned on $QUIT */
			r = newtriple(OC_SVGET);
			r->operand[0] = put_ilit(SV_QUIT);
			x = put_tref(r);
			coerce(&x, OCT_BOOL);
			cr = (oprtype *)mcalloc(sizeof(oprtype));	/* jump target to be placed later */
			bx_tail(x.oprval.tref, arg ? (bool)TRUE : (bool)FALSE, cr);
			r = newtriple(arg ? OC_RET : OC_RETARG);
			if (!arg)
				r->operand[0] = put_lit((mval *)&literal_null);
			x = put_tref(r);
		}
		if (!arg)
		{
			if (NULL != cr)
				*cr = put_tjmp(curtchain->exorder.bl);
			newtriple((run_time) ? OC_HARDRET : OC_RET);
		} else
		{
			if (!(rval = expr(&x)))
				return FALSE;
			if (window_token == TK_COMMA)
			{
				stx_error (ERR_QUITARGLST);
				return FALSE;
			}
			if (EXPR_INDR == rval)
			{	/* Indirect argument */
				if (NULL != cr)
					*cr = put_tjmp(curtchain->exorder.bl);
				make_commarg(&x, indir_quit);
			} else
			{
				r = newtriple(OC_RETARG);
				r->operand[0] = x;
				if (NULL != cr)
					*cr = put_tjmp(curtchain->exorder.bl);
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
