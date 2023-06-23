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
#include "toktyp.h"
#include "opcode.h"
#include "indir_enum.h"
#include "advancewindow.h"
#include "cmd.h"

error_def(ERR_FCHARMAXARGS);

int m_view(void)
{
	oprtype argv[CHARMAXARGS], *argp;
	unsigned short count;
	triple *view, *parm, *parm1;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	argp = &argv[0];
	count = 0;
	switch (expr(argp, MUMPS_EXPR))
	{
	case EXPR_FAIL:
		return FALSE;
	case EXPR_INDR:
		if (TK_COLON != TREF(window_token))
		{
			make_commarg(argp, indir_view);
			return TRUE;
		}
		/* caution: fall through */
	case EXPR_GOOD:
		view = maketriple(OC_VIEW);
		parm = newtriple(OC_PARAMETER);
		view->operand[1] = put_tref(parm);
		parm->operand[0] = *argp;
		count++;
		argp++;
		break;
	}
	for (;;)
	{
		if (TK_COLON != TREF(window_token))
			break;
		advancewindow();
		if (EXPR_FAIL == expr(argp, MUMPS_EXPR))
			return FALSE;
		parm1 = newtriple(OC_PARAMETER);
		parm->operand[1] = put_tref(parm1);
		parm1->operand[0] = *argp;
		parm = parm1;
		count++;
		argp++;
		if (CHARMAXARGS <= count)
		{
			stx_error(ERR_FCHARMAXARGS);
			return FALSE;
		}
	}
	view->operand[0] = put_ilit(count);
	ins_triple(view);
	return TRUE;
}
