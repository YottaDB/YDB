/****************************************************************
 *								*
 * Copyright (c) 2016-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
	oprtype		*cr;
	triple		*obp, *ref0, *ref1, *triptr;
	triple		*boolexprfinish, *boolexprfinish2;
	mval		*v;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	advancewindow();
	cr = (oprtype *)mcalloc(SIZEOF(oprtype));
	if (!bool_expr(FALSE, cr))
		return FALSE;
	triptr = (TREF(curtchain))->exorder.bl;
	boolexprfinish = (OC_BOOLEXPRFINISH == triptr->opcode) ? triptr : NULL;
	if (NULL != boolexprfinish)
		triptr = triptr->exorder.bl;
	for ( ; OC_NOOP == triptr->opcode; triptr = triptr->exorder.bl)
		;
	if (OC_LIT == triptr->opcode)
	{	/* it's a literal so optimize it */
		v = &triptr->operand[0].oprval.mlit->v;
		unuse_literal(v);
		dqdel(triptr, exorder);
		/* Remove OC_BOOLEXPRSTART and OC_BOOLEXPRFINISH opcodes too */
		REMOVE_BOOLEXPRSTART_AND_FINISH(boolexprfinish);	/* Note: Will set "boolexprfinish" to NULL */
		if (0 == MV_FORCE_BOOL(v))
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
		if (NULL != boolexprfinish)
		{
			INSERT_BOOLEXPRFINISH_AFTER_JUMP(boolexprfinish, boolexprfinish2);
			dqdel(boolexprfinish2, exorder);
			dqins(ref0, exorder, boolexprfinish2);
			*cr = put_tjmp(boolexprfinish2);
		} else
		{
			*cr = put_tjmp(ref1);
			boolexprfinish2 = NULL;
		}
		tnxtarg(&ref0->operand[0]);
	} else
	{
		INSERT_BOOLEXPRFINISH_AFTER_JUMP(boolexprfinish, boolexprfinish2);
		*cr = put_tjmp(boolexprfinish2);
	}
	INSERT_OC_JMP_BEFORE_OC_BOOLEXPRFINISH(boolexprfinish2);
	return TRUE;
}
