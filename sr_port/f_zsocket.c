/****************************************************************
 *								*
 *	Copyright 2014 Fidelity Information Services, Inc	*
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

error_def(ERR_MAXARGCNT);

#define MAX_ZSOCKET_ARGS	6	/* argc, dst, device, keyword, arg1, arg2 */

int f_zsocket(oprtype *a, opctype op)
{
	int	argc;
	char	tok_temp;
	oprtype *argp, argv[MAX_ZSOCKET_ARGS];
	triple	*curr, *last, *root;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	argp = &argv[0];
	argc = 0;
	if (TK_COMMA == TREF(window_token))
	{	/* empty first argument is for socket stringpool */
		curr = newtriple(OC_NULLEXP);
		*argp = put_tref(curr);
	} else
	{
		if (EXPR_FAIL == expr(argp, MUMPS_STR))	/* device name */
			return FALSE;
	}
	assert(TRIP_REF == argp->oprclass);
	argc++;
	argp++;
	advancewindow();
	if (EXPR_FAIL == expr(argp, MUMPS_STR))		/* which item */
		return FALSE;
	assert(TRIP_REF == argp->oprclass);
	argc++;
	argp++;
	for (;;)
	{
		if (TK_COMMA != TREF(window_token))
			break;
		advancewindow();
		tok_temp = TREF(window_token);
		if ((2 == argc) && ((TK_COMMA == tok_temp) || (TK_RPAREN == tok_temp)))
		{	/* missing third argument is for default index */
			curr = newtriple(OC_NULLEXP);
			*argp = put_tref(curr);
		} else if (EXPR_FAIL == expr(argp, MUMPS_EXPR))
			return FALSE;
		assert(TRIP_REF == argp->oprclass);
		argc++;
		argp++;
		if (MAX_ZSOCKET_ARGS < argc)
		{
			stx_error(ERR_MAXARGCNT, 1, MAX_ZSOCKET_ARGS);
			return FALSE;
		}
	}
	root = last = maketriple(op);
	root->operand[0] = put_ilit(argc + 1);
	argp = &argv[0];
	for (; argc > 0 ;argc--, argp++)
	{
		curr = newtriple(OC_PARAMETER);
		curr->operand[0] = *argp;
		last->operand[1] = put_tref(curr);
		last = curr;
	}
	ins_triple(root);
	*a = put_tref(root);
	return TRUE;
}
