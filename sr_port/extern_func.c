/****************************************************************
 *								*
 * Copyright (c) 2011-2017 Fidelity National Information	*
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
#include "advancewindow.h"

error_def(ERR_RTNNAME);

/* Maximum size of external routine reference of the form label^routine */
#define MAX_EXTREF (2 * MAX_MIDENT_LEN + STR_LIT_LEN("^"))

/* compiler parse to AVT module for external functions ($&)  */
int extern_func(oprtype *a)
{
	boolean_t	have_ident;
	char		*extref;
	int		cnt, actcnt;
	mstr		extentry, package;
	oprtype 	*nxtopr;
	triple		*calltrip, *ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TK_AMPERSAND == TREF(window_token));
	advancewindow();
	cnt = 0;
	extref = ((TREF(source_buffer)).addr + TREF(last_source_column) - 1);
	package.len = 0;
	package.addr = NULL;
	if (have_ident = (TK_IDENT == TREF(window_token)))			/* NOTE assignment */
	{
		if (TK_PERIOD == TREF(director_token))
		{	/* if ident is a package reference, then take it off */
			package.addr = extref;
			package.len = INTCAST(TREF(lexical_ptr) - extref - 1);
			extref = TREF(lexical_ptr);
			advancewindow();		/* get to . */
			advancewindow();		/* to next token */
			if (have_ident = (TK_IDENT == TREF(window_token)))	/* NOTE assignment */
				advancewindow();
		} else
			advancewindow();
	}
	if (TK_CIRCUMFLEX == TREF(window_token))
	{
		advancewindow();
		if (TK_IDENT == TREF(window_token))
		{
			have_ident = TRUE;
			advancewindow();
		}
	}
	if (!have_ident)
	{
		stx_error(ERR_RTNNAME);
		return FALSE;
	}
	extentry.len = INTCAST((TREF(source_buffer)).addr + TREF(last_source_column) - 1 - extref);
	extentry.len = INTCAST(extentry.len > MAX_EXTREF ? MAX_EXTREF : extentry.len);
	extentry.addr = extref;
	calltrip = maketriple(a ? OC_FNFGNCAL : OC_FGNCAL);
	nxtopr = &calltrip->operand[1];
	ref = newtriple(OC_PARAMETER);
	ref->operand[0] = put_str(package.addr, package.len);
	*nxtopr = put_tref(ref);
	nxtopr = &ref->operand[1];
	cnt++;
	ref = newtriple(OC_PARAMETER);
	ref->operand[0] = put_str(extentry.addr, extentry.len);
	*nxtopr = put_tref(ref);
	nxtopr = &ref->operand[1];
	cnt++;
	if (TK_LPAREN != TREF(window_token))
	{
		ref = newtriple(OC_PARAMETER);
		ref->operand[0] = put_ilit(0);
		*nxtopr = put_tref(ref);
		nxtopr = &ref->operand[1];
		cnt++;
		ref = newtriple(OC_PARAMETER);
		ref->operand[0] = put_ilit(0);
		*nxtopr = put_tref(ref);
		nxtopr = &ref->operand[1];
		cnt++;
	} else
	{
		if (!(actcnt = actuallist(nxtopr)))
			return FALSE;
		cnt += actcnt;
	}
	cnt++;				/* dst mval, or 0 */
	calltrip->operand[0] = put_ilit(cnt);
	ins_triple(calltrip);
	if (a)
		*a = put_tref(calltrip);
	return TRUE;
}
