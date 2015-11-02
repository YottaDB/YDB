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
#include "indir_enum.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "cmd.h"

#define CANCEL_ONE -1
#define CANCEL_ALL -2

LITREF mident 	zero_ident;

error_def(ERR_VAREXPECTED);

int m_zwatch(void)
{
	boolean_t	is_count;
	opctype		op;
	oprtype		count, name,action;
	triple		*next, *ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TK_MINUS == TREF(window_token))
	{
		advancewindow();
		switch (TREF(window_token))
		{
		case TK_ASTERISK:
			name = put_str(zero_ident.addr, zero_ident.len);
			count = put_ilit(CANCEL_ALL);
			advancewindow();
			break;
		case TK_IDENT:
			name = put_str((TREF(window_ident)).addr, (TREF(window_ident)).len);
			count = put_ilit(CANCEL_ONE);
			advancewindow();
			break;
		case TK_ATSIGN:
			if (!indirection(&name))
				return FALSE;
			count = put_ilit(CANCEL_ONE);
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			return FALSE;
		}
		action = put_str("",0);
		op = OC_WATCHREF;
	} else
	{
		if (TK_EQUAL == TREF(window_token))
		{
			advancewindow();
			op = OC_WATCHMOD;
		} else
			op = OC_WATCHREF;
		switch (TREF(window_token))
		{
		case TK_IDENT:
			name = put_str((TREF(window_ident)).addr, (TREF(window_ident)).len);
			advancewindow();
			break;
		case TK_ATSIGN:
			if (!indirection(&name))
				return FALSE;
			if ((OC_WATCHREF == op) && (TK_COLON != TREF(window_token)))
			{
				ref = maketriple(OC_COMMARG);
				ref->operand[0] = name;
				ref->operand[1] = put_ilit((mint) indir_zwatch);
				ins_triple(ref);
				return TRUE;
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			return FALSE;
		}
		if (TK_COLON != TREF(window_token))
		{
			action = put_str("",0);
			count = put_ilit(0);
		} else
		{
			advancewindow();
			if (TK_COLON == TREF(window_token))
			{
				is_count = TRUE;
				action = put_str("", 0);
			} else
			{
				if (EXPR_FAIL == expr(&action, MUMPS_STR))
					return FALSE;
				is_count = (TK_COLON == TREF(window_token));
			}
			if (is_count)
			{
				advancewindow();
				if (EXPR_FAIL == expr(&count, MUMPS_INT))
					return FALSE;
			} else
				count = put_ilit(0);
		}
	}
	ref = newtriple(op);
	ref->operand[0] = name;
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	next->operand[0] = action;
	next->operand[1] = count;
	return TRUE;
}
