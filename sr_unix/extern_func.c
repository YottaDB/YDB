/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

/* Maximum size of external routine reference of the form label^routine */
#define MAX_EXTREF (2 * MAX_MIDENT_LEN + STR_LIT_LEN("^"))

GBLREF unsigned char *source_buffer;
GBLREF short int last_source_column;
GBLREF char window_token;
GBLREF char director_token;
GBLREF char *lexical_ptr;

int extern_func(oprtype *a)
{
	char	*extref;
	mstr	package, extentry;
	oprtype *nxtopr;
	triple	*calltrip, *ref;
	bool	have_ident;
	int	cnt, actcnt;
	error_def(ERR_RTNNAME);

	assert (window_token == TK_AMPERSAND);
	advancewindow();
	cnt = 0;
	extref = (char *)&source_buffer[last_source_column - 1];
	package.len = 0;
	package.addr = NULL;
	if (have_ident = (window_token == TK_IDENT))
	{
		if (director_token == TK_PERIOD)	/* if ident is a package reference, then take it off */
		{
			package.len = INTCAST(lexical_ptr - extref - 1);
			package.addr = extref;
			extref = lexical_ptr;
			advancewindow();		/* get to . */
			advancewindow();		/* to next token */
			if (have_ident = (window_token == TK_IDENT))
				advancewindow();
		}
		else
		{
			advancewindow();
		}
	}
	if (window_token == TK_CIRCUMFLEX)
	{
		advancewindow();
		if (window_token == TK_IDENT)
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
	extentry.len = INTCAST((char *)&source_buffer[last_source_column - 1] - extref);
	extentry.len = INTCAST(extentry.len > MAX_EXTREF ? MAX_EXTREF : extentry.len);
	extentry.addr = extref;

	calltrip = maketriple( (a ? OC_FNFGNCAL : OC_FGNCAL));
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

	if (window_token != TK_LPAREN)
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
	}
	else
	{
		if (!(actcnt = actuallist (nxtopr)))
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
