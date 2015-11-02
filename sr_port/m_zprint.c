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

GBLREF boolean_t	run_time;
GBLREF mident		routine_name;
LITREF mident		zero_ident;

error_def(ERR_LABELEXPECTED);
error_def(ERR_RTNNAME);

int m_zprint(void)
{
	boolean_t	got_some;
	oprtype		lab1, lab2, off1, off2, rtn;
	triple		*next, *ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	got_some = FALSE;
	lab1 = put_str(zero_ident.addr, zero_ident.len);
	off1 = put_ilit(0);
	if ((TK_EOL != TREF(window_token)) && (TK_SPACE != TREF(window_token))
		&& !lref(&lab1, &off1, TRUE, indir_zprint, TRUE, &got_some))
			return FALSE;
	if ((TRIP_REF == lab1.oprclass) && (OC_COMMARG == lab1.oprval.tref->opcode))
		return TRUE;
	if (TK_CIRCUMFLEX != TREF(window_token))
	{	/* Routine not specified, use current routine */
		if (!run_time)
			rtn = put_str(routine_name.addr, routine_name.len);
		else
			rtn = put_tref(newtriple(OC_CURRTN));
	} else
	{
		got_some = TRUE;
		advancewindow();
		switch (TREF(window_token))
		{
		case TK_IDENT:
#			ifdef GTM_TRIGGER
			if (TK_HASH == TREF(director_token))
				/* Coagulate tokens as necessary (and available) to allow '#' in the rtn name */
				advwindw_hash_in_mname_allowed();
#			endif
			rtn = put_str((TREF(window_ident)).addr, (TREF(window_ident)).len);
			advancewindow();
			break;
		case TK_ATSIGN:
			if (!indirection(&rtn))
				return FALSE;
			break;
		default:
			stx_error(ERR_RTNNAME);
			return FALSE;
		}
	}
	if (TK_COLON == TREF(window_token))
	{
		if (!got_some)
		{
			stx_error(ERR_LABELEXPECTED);
			return FALSE;
		}
		lab2 = put_str(zero_ident.addr, zero_ident.len);
		off2 = put_ilit(0);
		advancewindow();
		if (!lref(&lab2, &off2, TRUE, indir_zprint, FALSE, &got_some))
			return FALSE;
		if (!got_some)
		{
			stx_error(ERR_LABELEXPECTED);
			return FALSE;
		}
	} else
	{
		lab2 = lab1;
		off2 = off1;
	}
	ref = newtriple(OC_ZPRINT);
	ref->operand[0] = rtn;
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	next->operand[0] = lab1;
	ref = newtriple(OC_PARAMETER);
	next->operand[1] = put_tref(ref);
	ref->operand[0] = off1;
	next = newtriple(OC_PARAMETER);
	ref->operand[1] = put_tref(next);
	next->operand[0] = lab2;
	ref = newtriple(OC_PARAMETER);
	next->operand[1] = put_tref(ref);
	ref->operand[0] = off2;
	return TRUE;
}
