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
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "iotimer.h"
#include "job.h"
#include "advancewindow.h"
#include "cmd.h"

GBLREF boolean_t	run_time;
GBLREF mident		routine_name;
LITREF mident		zero_ident;

error_def(ERR_COMMAORRPAREXP);
error_def(ERR_JOBACTREF);
error_def(ERR_MAXACTARG);
error_def(ERR_RTNNAME);

int m_job(void)
{
	boolean_t	is_timeout, dummybool;
	static		readonly unsigned char empty_plist[1] = { jp_eol };
	int		argcnt;
	oprtype		arglst, *argptr, argval, label, offset, routine, plist, timeout;
	triple		*next, *ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	label = put_str(zero_ident.addr, zero_ident.len);
	offset = put_ilit((mint)0);
	if (!lref(&label, &offset, FALSE, indir_job, TRUE, &dummybool))
		return FALSE;
	if ((TRIP_REF == label.oprclass) && (OC_COMMARG == label.oprval.tref->opcode))
		return TRUE;
	if (TK_CIRCUMFLEX != TREF(window_token))
	{
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
	argcnt = 0;
	if (TK_LPAREN == TREF(window_token))
	{
		advancewindow();
		argptr = &arglst;
		while (TK_RPAREN != TREF(window_token))
		{
			if (MAX_ACTUALS < argcnt)
			{
				stx_error(ERR_MAXACTARG);
				return FALSE;
			}
			if (TK_PERIOD == TREF(window_token))
			{
				stx_error(ERR_JOBACTREF);
				return FALSE;
			}
			if (TK_COMMA == TREF(window_token))
			{
				ref = newtriple(OC_NULLEXP);
				argval = put_tref(ref);
			} else if (EXPR_FAIL == expr(&argval, MUMPS_EXPR))
				return FALSE;
			ref = newtriple(OC_PARAMETER);
			ref->operand[0] = argval;
			*argptr = put_tref(ref);
			argptr = &ref->operand[1];
			argcnt++;
			if (TK_COMMA == TREF(window_token))
				advancewindow();
			else if (TK_RPAREN != TREF(window_token))
			{
				stx_error(ERR_COMMAORRPAREXP);
				return FALSE;
			}
		}
		advancewindow();	/* jump over close paren */
	}
	if (TK_COLON == TREF(window_token))
	{
		advancewindow();
		if (TK_COLON == TREF(window_token))
		{
			is_timeout = TRUE;
			plist = put_str((char *)empty_plist,SIZEOF(empty_plist));
		} else
		{
			if (!jobparameters(&plist))
				return FALSE;
			is_timeout = (TK_COLON == TREF(window_token));
		}
		if (is_timeout)
		{
			advancewindow();
			if (EXPR_FAIL == expr(&timeout, MUMPS_INT))
				return FALSE;
		} else
			timeout = put_ilit(NO_M_TIMEOUT);
	} else
	{
		is_timeout = FALSE;
		plist = put_str((char *)empty_plist,SIZEOF(empty_plist));
		timeout = put_ilit(NO_M_TIMEOUT);
	}

	ref = newtriple(OC_JOB);
	ref->operand[0] = put_ilit(argcnt + 5);		/* parameter list + five fixed arguments */
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	next->operand[0] = label;
	ref = newtriple(OC_PARAMETER);
	next->operand[1] = put_tref(ref);
	ref->operand[0] = offset;
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	next->operand[0] = routine;
	ref = newtriple(OC_PARAMETER);
	next->operand[1] = put_tref(ref);
	ref->operand[0] = plist;
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	next->operand[0] = timeout;
	if (argcnt)
		next->operand[1] = arglst;
	if (is_timeout)
		newtriple(OC_TIMTRU);
	return TRUE;
}
