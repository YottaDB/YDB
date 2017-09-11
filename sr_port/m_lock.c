/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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
#include "toktyp.h"
#include "iotimer.h"
#include "indir_enum.h"
#include "advancewindow.h"
#include "cmd.h"

LITREF	mval		literal_notimeout;

error_def(ERR_RPARENMISSING);

int m_lock(void)
{
	boolean_t	indirect;
	opctype		ox;
	oprtype		indopr;
	triple		*ref, *restart;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	restart = newtriple(OC_RESTARTPC);
	indirect = FALSE;
	ox = OC_LOCK;
	switch (TREF(window_token))
	{
	case TK_EOL:
	case TK_SPACE:
		ox = OC_UNLOCK;
		restart->opcode = OC_NOOP;
		newtriple(OC_UNLOCK);
		return TRUE;
		break;
	case TK_MINUS:
	case TK_PLUS:
		ox = (TK_MINUS == TREF(window_token)) ? OC_LCKDECR : OC_LCKINCR;
		advancewindow();
		if (TK_ATSIGN != TREF(window_token))
			break;
		/* WARNING possible fall-through */
	case TK_ATSIGN:
		if (!indirection(&indopr))
			return FALSE;
		ref = maketriple(OC_COMMARG);
		ref->operand[0] = indopr;
		if ((TK_COLON != TREF(window_token)) && (OC_LOCK == ox))
		{
			ref->operand[1] = put_ilit((mint)indir_lock);
			ins_triple(ref);
			return TRUE;
		}
		ref->operand[1] = put_ilit((mint)indir_nref);
		indirect = TRUE;			/* WARNING  fall-through */
	default:
		if (OC_LOCK == ox)
			newtriple(OC_UNLOCK);
	}
	newtriple(OC_LKINIT);
	if (indirect)
		ins_triple(ref);
	else
	{
		switch (TREF(window_token))
		{
		case TK_LPAREN:
			do
			{
				advancewindow();
				if (nref() == EXPR_FAIL)
					return FALSE;
			} while (TK_COMMA == TREF(window_token));
			if (TK_RPAREN != TREF(window_token))
			{
				stx_error(ERR_RPARENMISSING);
				return FALSE;
			}
			advancewindow();
			break;
		default:
			if (EXPR_FAIL == nref())
				return FALSE;
			break;
		}
	}
	ref = maketriple(ox);
	if (TK_COLON != TREF(window_token))
	{
		ref->operand[0] = put_lit((mval *)&literal_notimeout);
		ins_triple(ref);
	}
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(ref->operand[0]), MUMPS_EXPR))
			return FALSE;
		ins_triple(ref);
		newtriple(OC_TIMTRU);
	}
	return TRUE;
}
