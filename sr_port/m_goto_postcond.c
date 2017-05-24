/****************************************************************
 *								*
 * Copyright (c) 2016 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

int m_goto_postcond(triple *oldchain, triple *tmpchain)
/* process a postconditional for m_goto and m_zgoto */
{
	oprtype	*cr;
	triple	*obp, *ref0, *ref1, *triptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	advancewindow();
	cr = (oprtype *)mcalloc(SIZEOF(oprtype));
	if (!bool_expr(FALSE, cr))
		return FALSE;
	for (triptr = (TREF(curtchain))->exorder.bl; OC_NOOP == triptr->opcode; triptr = triptr->exorder.bl)
		;
	if (OC_LIT == triptr->opcode)
	{	/* it's a literal so optimize it */
		unuse_literal(&triptr->operand[0].oprval.mlit->v);
		dqdel(triptr, exorder);
		if (0 == triptr->operand[0].oprval.mlit->v.m[1])
			setcurtchain(oldchain);			/* it's a FALSE so just discard the argument */
		else
		{	/* it's TRUE so treat as if there was no argument postconditional */
			while (TK_EOL != TREF(window_token))	/* but first discard the rest of the line - it's dead */
				advancewindow();
			obp = oldchain->exorder.bl;
			dqadd(obp, tmpchain, exorder);		/* this is a violation of info hiding */
		}
		return TRUE;
	}
	if ((TREF(expr_start) != TREF(expr_start_orig)) && (OC_NOOP != (TREF(expr_start))->opcode))
	{
		triptr = newtriple(OC_GVRECTARG);
		triptr->operand[0] = put_tref(TREF(expr_start));
	}
	obp = oldchain->exorder.bl;
	dqadd(obp, tmpchain, exorder);				 /* this is a violation of info hiding */
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
