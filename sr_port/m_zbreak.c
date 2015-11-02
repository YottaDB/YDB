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

GBLREF boolean_t	run_time;
GBLREF mident		routine_name;
LITREF mident		zero_ident;

error_def(ERR_LABELEXPECTED);
error_def(ERR_RTNNAME);

int m_zbreak(void)
{
	boolean_t	cancel, cancel_all, dummybool, is_count;
	oprtype		action, count, label, offset, routine;
	triple		*next, *ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	label = put_str((char *)zero_ident.addr, zero_ident.len);
	cancel_all = FALSE;
	action = put_str("B", 1);
	if (TK_MINUS == TREF(window_token))
	{
		advancewindow();
		cancel = TRUE;
		count = put_ilit((mint)CANCEL_ONE);
	} else
	{
		cancel = FALSE;
		count = put_ilit((mint)0);
	}
	if (TK_ASTERISK == TREF(window_token))
	{
		if (cancel)
		{
			advancewindow();
			cancel_all = TRUE;
			if (!run_time)
				routine = put_str(routine_name.addr, routine_name.len);
			else
				routine = put_tref(newtriple(OC_CURRTN));
			offset = put_ilit((mint) 0);
			count = put_ilit((mint) CANCEL_ALL);
		} else
		{
			stx_error(ERR_LABELEXPECTED);
			return FALSE;
		}
	} else
	{
		offset = put_ilit((mint) 0);
		if (!lref(&label,&offset, TRUE, indir_zbreak, !cancel, &dummybool))
			return FALSE;
		if (label.oprclass == TRIP_REF && label.oprval.tref->opcode == OC_COMMARG)
			return TRUE;
		if (TK_CIRCUMFLEX != TREF(window_token))
		{	/* Routine not specified, assume current routine */
			if (!run_time)
				routine = put_str(routine_name.addr, routine_name.len);
			else
				routine = put_tref(newtriple(OC_CURRTN));
		} else
		{
			advancewindow();
			switch (TREF(window_token))
			{
			case TK_IDENT:
#				ifdef GTM_TRIGGER
				if (TK_HASH == TREF(director_token))
					/* Coagulate tokens as necessary (and available) to allow '#' in the rtn name */
					advwindw_hash_in_mname_allowed();
#				endif
				routine = put_str((TREF(window_ident)).addr, (TREF(window_ident)).len);
				advancewindow();
				break;
			case TK_ATSIGN:
				if (!indirection(&routine))
					return FALSE;
				break;
			default:
				stx_error(ERR_RTNNAME);
				return FALSE;
			}
		}
		if (!cancel && (TK_COLON == TREF(window_token)))
		{
			advancewindow();
			if (TK_COLON == TREF(window_token))
			{
				is_count = TRUE;
				action = put_str("B",1);
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
			}
		}
	}
	ref = newtriple(OC_SETZBRK);
	ref->operand[0] = label;
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	next->operand[0] = offset;
	ref = newtriple(OC_PARAMETER);
	next->operand[1] = put_tref(ref);
	ref->operand[0] = routine;
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	next->operand[0] = action;
	ref = newtriple(OC_PARAMETER);
	next->operand[1] = put_tref(ref);
	ref->operand[0] = count;
	return TRUE;
}
