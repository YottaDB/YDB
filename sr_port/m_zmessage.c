/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

GBLREF char window_token;

int m_zmessage (void)
{
	triple	*ref0, *ref1;
	oprtype code, arg;
	int	count;

	switch (intexpr (&code))
	{
	case EXPR_FAIL:
		return FALSE;
	case EXPR_INDR:
		if (window_token != TK_COLON)
			make_commarg (&code, indir_zmess);
	}
	ref0 = newtriple (OC_PARAMETER);
	ref0->operand[0] = code;
	ref1 = ref0;
	for (count = 1; window_token == TK_COLON; count++)
	{
		advancewindow();
		if (expr (&arg) == EXPR_FAIL)
			return FALSE;
		ref1->operand[1] = put_tref (newtriple (OC_PARAMETER));
		ref1 = ref1->operand[1].oprval.tref;
		ref1->operand[0] = arg;
	}
	ref1 = newtriple (OC_ZMESS);
	ref1->operand[0] = put_ilit (count);
	ref1->operand[1] = put_tref (ref0);
	return TRUE;
}
