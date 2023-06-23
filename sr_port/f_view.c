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

error_def(ERR_FCHARMAXARGS);

int f_view(oprtype *a, opctype op)
{
	int	argc;
	oprtype *argp, argv[CHARMAXARGS];
	triple	*curr, *last, *root;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	argp = &argv[0];
	argc = 0;
	if (EXPR_FAIL == expr(argp, MUMPS_EXPR))
		return FALSE;
	assert(TRIP_REF == argp->oprclass);
	argc++;
	argp++;
	for (;;)
	{
		if (TK_COMMA != TREF(window_token))
			break;
		advancewindow();
		if (EXPR_FAIL == expr(argp, MUMPS_EXPR))
			return FALSE;
		assert(TRIP_REF == argp->oprclass);
		argc++;
		argp++;
		if (argc >= CHARMAXARGS - 1)
		{
			stx_error(ERR_FCHARMAXARGS);
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
