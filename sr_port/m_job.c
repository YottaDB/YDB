/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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

GBLREF char		window_token;
GBLREF mident		window_ident;
GBLREF boolean_t	run_time;
GBLREF mident		routine_name;
LITREF mident		zero_ident;

int m_job(void)
{
	int	argcnt;
	triple *ref,*next;
	oprtype label, offset, routine, plist, timeout, arglst, *argptr, argval;
	static readonly unsigned char empty_plist[1] = { jp_eol };
	bool is_timeout,dummybool;

	error_def(ERR_MAXACTARG);
	error_def(ERR_RTNNAME);
	error_def(ERR_COMMAORRPAREXP);
	error_def(ERR_JOBACTREF);

	label = put_str(zero_ident.addr, zero_ident.len);
	offset = put_ilit((mint)0);
	if (!lref(&label, &offset, FALSE, indir_job, TRUE, &dummybool))
		return FALSE;
	if ((TRIP_REF == label.oprclass) && (OC_COMMARG == label.oprval.tref->opcode))
		return TRUE;
	if (TK_CIRCUMFLEX != window_token)
	{
		if (!run_time)
			routine = put_str(routine_name.addr, routine_name.len);
		else
			routine = put_tref(newtriple(OC_CURRTN));
	} else
	{
		advancewindow();
		switch(window_token)
		{
		case TK_IDENT:
			routine = put_str(window_ident.addr, window_ident.len);
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
	if (TK_LPAREN == window_token)
	{
		advancewindow();
		argptr = &arglst;
		do
		{
			if (argcnt > MAX_ACTUALS)
			{
				stx_error(ERR_MAXACTARG);
				return FALSE;
			}
			if (TK_PERIOD == window_token)
			{
				stx_error(ERR_JOBACTREF);
				return FALSE;
			}
			if ((TK_COMMA == window_token) || (TK_RPAREN == window_token))
			{
				ref = newtriple(OC_NULLEXP);
				argval = put_tref(ref);
			} else if (!expr(&argval))
				return FALSE;
			ref = newtriple(OC_PARAMETER);
			ref->operand[0] = argval;
			*argptr = put_tref(ref);
			argptr = &ref->operand[1];
			argcnt++;
			if (TK_COMMA == window_token)
				advancewindow();
			else if (TK_RPAREN != window_token)
			{
				stx_error(ERR_COMMAORRPAREXP);
				return FALSE;
			}
		} while (TK_RPAREN != window_token);
		advancewindow();	/* jump over close paren */
	}
	if (TK_COLON == window_token)
	{
		advancewindow();
		if (TK_COLON == window_token)
		{
			is_timeout = TRUE;
			plist = put_str((char *)empty_plist,SIZEOF(empty_plist));
		} else
		{
			if (!jobparameters(&plist))
				return FALSE;
			is_timeout = (TK_COLON == window_token);
		}
		if (is_timeout)
		{
			advancewindow();
			if (!intexpr(&timeout))
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
