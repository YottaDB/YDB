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
#include "mdq.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mmemory.h"
#include "advancewindow.h"
#include "cmd.h"

int m_xecute(void)
{
	oprtype *cr, x;
	triple *obp, *oldchain, *ref0, *ref1, tmpchain, *triptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	dqinit(&tmpchain,exorder);
	oldchain = setcurtchain(&tmpchain);
	switch (expr(&x, MUMPS_STR))
	{
	case EXPR_FAIL:
		setcurtchain(oldchain);
		return FALSE;
	case EXPR_INDR:
		if (TK_COLON != TREF(window_token))
		{
			make_commarg(&x,indir_xecute);
			break;
		}
		/* caution: fall through */
	case EXPR_GOOD:
		ref0 = maketriple(OC_COMMARG);
		ref0->operand[0] = x;
		ref0->operand[1] = put_ilit(indir_linetail);
		ins_triple(ref0);
	}
	setcurtchain(oldchain);
	if (TK_COLON == TREF(window_token))
	{
		advancewindow();
		cr = (oprtype *)mcalloc(SIZEOF(oprtype));
		if (!bool_expr(FALSE, cr))
			return FALSE;
		if ((TREF(expr_start) != TREF(expr_start_orig)) && (OC_NOOP != (TREF(expr_start))->opcode))
		{
			triptr = newtriple(OC_GVRECTARG);
			triptr->operand[0] = put_tref(TREF(expr_start));
		}
		obp = oldchain->exorder.bl;
		dqadd(obp,&tmpchain,exorder);		/* violates info hiding */
		if ((TREF(expr_start) != TREF(expr_start_orig)) && (OC_NOOP != (TREF(expr_start))->opcode))
		{
			ref0 = newtriple(OC_JMP);
			ref1 = newtriple(OC_GVRECTARG);
			ref1->operand[0] = put_tref(TREF(expr_start));
			*cr = put_tjmp(ref1);
			tnxtarg(&ref0->operand[0]);
		} else
			tnxtarg(cr);
		return TRUE;
	}
	obp = oldchain->exorder.bl;
	dqadd(obp,&tmpchain,exorder);		/* violates info hiding */
	return TRUE;
}
