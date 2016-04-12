/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "indir_enum.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "cmd.h"

error_def(ERR_FCHARMAXARGS);

int m_zmessage (void)
{
	int		count;
	oprtype		arg;
	triple		*ref0, *ref1;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch (expr(&arg, MUMPS_EXPR))
	{
	case EXPR_FAIL:
		return FALSE;
	case EXPR_INDR:
		if (TK_COLON != TREF(window_token))
		{
			coerce(&arg, OCT_MVAL);
			make_commarg(&arg, indir_zmess);
			return TRUE;
		}
		/* caution: fall through */
	case EXPR_GOOD:
		coerce(&arg, OCT_MINT);
		ref0 = maketriple(OC_ZMESS);
		ref0->operand[1] = put_tref(newtriple(OC_PARAMETER));
		ref1 = ref0->operand[1].oprval.tref;
		ref1->operand[0] = arg;
		break;
	}
	for (count = 1; TK_COLON == TREF(window_token); count++)
	{
		advancewindow();
		if (EXPR_FAIL == expr(&arg, MUMPS_EXPR))
			return FALSE;
		ref1->operand[1] = put_tref(newtriple(OC_PARAMETER));
		ref1 = ref1->operand[1].oprval.tref;
		ref1->operand[0] = arg;
	}
	ref0->operand[0] = put_ilit(count);
	ins_triple(ref0);
	return TRUE;
}
