/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mdq.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "cmd.h"

GBLREF char window_token;
GBLREF triple *expr_start, *expr_start_orig;

int m_zgoto(void)
{
	triple tmpchain, *oldchain, *obp, *ref0, *ref1, *triptr;
	oprtype *cr, quits;
	int4 rval;
	error_def(ERR_COLON);

	dqinit(&tmpchain,exorder);
	oldchain = setcurtchain(&tmpchain);
	if (window_token == TK_EOL || window_token == TK_SPACE)
	{
		quits = put_ilit(1);
		rval = EXPR_GOOD;
	}
	else if (!(rval = intexpr(&quits)))
	{
		setcurtchain(oldchain);
		return FALSE;
	}
	if (rval != EXPR_INDR && (window_token == TK_EOL || window_token == TK_SPACE))
	{
		setcurtchain(oldchain);
		obp = oldchain->exorder.bl;
		dqadd(obp,&tmpchain,exorder);   /*this is a violation of info hiding*/
		ref0 = newtriple(OC_ZG1);
		ref0->operand[0] = quits;
		return TRUE;
	}
	if (window_token != TK_COLON)
	{
		setcurtchain(oldchain);
		if (rval != EXPR_INDR)
		{
			stx_error(ERR_COLON);
			return FALSE;
		}
		make_commarg(&quits,indir_zgoto);
		obp = oldchain->exorder.bl;
		dqadd(obp,&tmpchain,exorder);   /*this is a violation of info hiding*/
	 	return TRUE;
	}
	advancewindow();
	if (window_token != TK_COLON)
	{
		if (!entryref(OC_NOOP,OC_PARAMETER,(mint) indir_goto, FALSE, FALSE))
		{
			setcurtchain(oldchain);
			return FALSE;
		}
		ref0 = maketriple(OC_ZGOTO);
		ref0->operand[0] = quits;
		ref0->operand[1] = put_tref(tmpchain.exorder.bl);
		ins_triple(ref0);
		setcurtchain(oldchain);
	}
	else
	{
		ref0 = maketriple(OC_ZG1);
		ref0->operand[0] = quits;
		ins_triple(ref0);
		setcurtchain(oldchain);
	}
	if (window_token == TK_COLON)
	{
		advancewindow();
		cr = (oprtype *) mcalloc(SIZEOF(oprtype));
		if (!bool_expr((bool) FALSE,cr))
			return FALSE;
		if (expr_start != expr_start_orig)
		{
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(expr_start);
		}
		obp = oldchain->exorder.bl;
		dqadd(obp,&tmpchain,exorder);   /*this is a violation of info hiding*/
		if (expr_start != expr_start_orig)
		{
			ref0 = newtriple(OC_JMP);
			ref1 = newtriple(OC_GVRECTARG);
			ref1->operand[0] = put_tref(expr_start);
			*cr = put_tjmp(ref1);
			tnxtarg(&ref0->operand[0]);
		}
		else
			tnxtarg(cr);
		return TRUE;
	}
	obp = oldchain->exorder.bl;
	dqadd(obp,&tmpchain,exorder);   /*this is a violation of info hiding*/
	return TRUE;
}
