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
#include "advancewindow.h"
#include "cmd.h"

GBLREF char		window_token, director_token;
GBLREF mident		window_ident;
GBLREF boolean_t	run_time;
GBLREF mident		routine_name;
LITREF mident		zero_ident;

int m_zprint(void)
{
	oprtype	lab1, lab2, off1, off2, rtn;
	triple	*ref, *next;
	bool	got_some;

	error_def(ERR_LABELEXPECTED);
	error_def(ERR_RTNNAME);

	got_some = FALSE;
	lab1 = put_str(zero_ident.addr, zero_ident.len);
	off1 = put_ilit(0);
	if (window_token != TK_EOL && window_token != TK_SPACE && !lref(&lab1,&off1,TRUE,indir_zprint,TRUE,&got_some))
		return FALSE;
	if (lab1.oprclass == TRIP_REF && lab1.oprval.tref->opcode == OC_COMMARG)
		return TRUE;
	if (window_token != TK_CIRCUMFLEX)
	{	/* Routine not specified, use current routine */
		if (!run_time)
			rtn = put_str(routine_name.addr, routine_name.len);
		else
			rtn = put_tref(newtriple(OC_CURRTN));
	} else
	{
		got_some = TRUE;
		advancewindow();
		switch(window_token)
		{
			case TK_IDENT:
#				ifdef GTM_TRIGGER
				if (TK_HASH == director_token)
					/* Coagulate tokens as necessary (and available) to allow '#' in the rtn name */
					advwindw_hash_in_mname_allowed();
#				endif
				rtn = put_str(window_ident.addr, window_ident.len);
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
	if (window_token == TK_COLON)
	{
		if (!got_some)
		{
			stx_error(ERR_LABELEXPECTED);
			return FALSE;
		}
		lab2 = put_str(zero_ident.addr, zero_ident.len);
		off2 = put_ilit(0);
		advancewindow();
		if (!lref(&lab2,&off2,TRUE,indir_zprint,FALSE,&got_some))
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
