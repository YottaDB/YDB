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
#include "iotimer.h"
#include "indir_enum.h"
#include "advancewindow.h"
#include "cmd.h"

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
	newtriple(OC_LKINIT);
	indirect = FALSE;
	switch (TREF(window_token))
	{
	case TK_MINUS:
		advancewindow();
		ox = OC_LCKDECR;
		break;
	case TK_PLUS:
		advancewindow();
		ox = OC_LCKINCR;
		break;
	case TK_EOL:
	case TK_SPACE:
		ox = OC_UNLOCK;
		restart->opcode = OC_NOOP;
		newtriple(OC_UNLOCK);
		return TRUE;
		break;
	case TK_ATSIGN:
		if (!indirection(&indopr))
			return FALSE;
		ref = maketriple(OC_COMMARG);
		ref->operand[0] = indopr;
		if (TK_COLON != TREF(window_token))
		{
			ref->operand[1] = put_ilit((mint) indir_lock);
			ins_triple(ref);
			return TRUE;
		}
		ref->operand[1] = put_ilit((mint) indir_nref);
		indirect = TRUE;
		/*** CAUTION:  FALL-THROUGH ***/
	default:
		newtriple(OC_UNLOCK);
		ox = OC_LOCK;
	}
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
			if (nref() == EXPR_FAIL)
				return FALSE;
			break;
		}
	}
	ref = maketriple(ox);
	if (TK_COLON != TREF(window_token))
	{	ref->operand[0] = put_ilit(NO_M_TIMEOUT);
		ins_triple(ref);
	}
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(ref->operand[0]), MUMPS_INT))
			return FALSE;
		ins_triple(ref);
		newtriple(OC_TIMTRU);
	}
	return TRUE;
}
