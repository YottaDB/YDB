/****************************************************************
 *								*
 * Copyright (c) 2010-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "mdq.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "cmd.h"

error_def(ERR_COLON);

int m_zgoto(void)
{
	int		rval;
	oprtype		*cr, quits;
	triple		*obp, *oldchain, *ref0, *ref1, tmpchain, *triptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	exorder_init(&tmpchain);
	oldchain = setcurtchain(&tmpchain);
	if ((TK_EOL == TREF(window_token)) || (TK_SPACE == TREF(window_token)))
	{	/* Default zgoto level is 1 */
		quits = put_ilit(1);
		rval = EXPR_GOOD;
	} else if (EXPR_FAIL == (rval = expr(&quits, MUMPS_INT)))		/* NOTE assignment */
	{
		setcurtchain(oldchain);
		return FALSE;
	}
	if ((EXPR_INDR != rval) && ((TK_EOL == TREF(window_token)) || (TK_SPACE == TREF(window_token))))
	{	/* Only level parm supplied (no entry ref) - job for op_zg1 */
		setcurtchain(oldchain);
		obp = oldchain->exorder.bl;
		dqadd(obp, &tmpchain, exorder);					/* this is a violation of info hiding */
		ref0 = newtriple(OC_ZG1);
		ref0->operand[0] = quits;
		return TRUE;
	}
	if (TK_COLON != TREF(window_token))
	{	/* First arg parsed, not ending in ":". Better have been indirect */
		setcurtchain(oldchain);
		if (EXPR_INDR != rval)
		{
			stx_error(ERR_COLON);
			return FALSE;
		}
		make_commarg(&quits, indir_zgoto);
		obp = oldchain->exorder.bl;
		dqadd(obp, &tmpchain, exorder);					/* this is a violation of info hiding */
	 	return TRUE;
	}
	advancewindow();
	if (TK_COLON != TREF(window_token))
	{
		if (!entryref(OC_NOOP, OC_PARAMETER, (mint)indir_goto, FALSE, FALSE, TRUE))
		{
			setcurtchain(oldchain);
			return FALSE;
		}
		ref0 = maketriple(OC_ZGOTO);
		ref0->operand[0] = quits;
		ref0->operand[1] = put_tref(tmpchain.exorder.bl);
		ins_triple(ref0);
		setcurtchain(oldchain);
	} else
	{
		ref0 = maketriple(OC_ZG1);
		ref0->operand[0] = quits;
		ins_triple(ref0);
		setcurtchain(oldchain);
	}
	if (TK_COLON == TREF(window_token))
		return m_goto_postcond(oldchain, &tmpchain);			/* post conditional expression */
	obp = oldchain->exorder.bl;
	dqadd(obp, &tmpchain, exorder);						/* this is a violation of info hiding */
	return TRUE;
}
