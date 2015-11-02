/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
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
#ifdef VMS
#include "vaxsym.h"
#include "mmemory.h"
#endif

GBLREF char *lexical_ptr;
GBLREF unsigned char *source_buffer;

error_def(ERR_RTNNAME);

/* Maximum size of external routine reference of the form label^routine */
#ifdef UNIX
#define MAX_EXTREF (2 * MAX_MIDENT_LEN + STR_LIT_LEN("^"))
#endif

/* compiler parse to AVT module for external functions ($&)  */
int extern_func(oprtype *a)
{
	boolean_t	have_ident;
	char		*extref;
	int		cnt, actcnt;
	mstr		extentry, package;
	oprtype 	*nxtopr;
	triple		*calltrip, *ref;
#	ifdef VMS
	char		*extsym, *extern_symbol;
	oprtype		tabent;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert (TK_AMPERSAND == TREF(window_token));
	advancewindow();
	cnt = 0;
	extref = (char *)&source_buffer[TREF(last_source_column) - 1];
	package.len = 0;
	package.addr = NULL;
	if (have_ident = (TK_IDENT == TREF(window_token)))			/* NOTE assignment */
	{
		if (TK_PERIOD == TREF(director_token))
		{	/* if ident is a package reference, then take it off */
			package.addr = extref;
			package.len = INTCAST(lexical_ptr - extref - 1);
			VMS_ONLY(package.len = ((MAX_EXTREF < package.len) ? MAX_EXTREF : package.len));
			extref = lexical_ptr;
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
	extentry.len = INTCAST((char *)&source_buffer[TREF(last_source_column) - 1] - extref);
	extentry.len = INTCAST(extentry.len > MAX_EXTREF ? MAX_EXTREF : extentry.len);
	extentry.addr = extref;
#	ifdef VMS_CASE_SENSITIVE_MACROS
	if (!run_time)
	{	/* this code is disabled because the
		 * external call table macros are not case sensitive
		 */
		extern_symbol = mcalloc(MAX_SYMREF);
		extsym = extern_symbol;
		MEMCPY_LIT(extsym, ZCSYM_PREFIX);
		extsym += SIZEOF(ZCSYM_PREFIX) - 1;
		memcpy(extsym, package.addr, package.len);
		if ('%' == *extsym)
			*extsym = '_';
		extsym += package.len;
		*extsym++ = '.';
		memcpy(extsym, extentry.addr, extentry.len);
		if ('%' == *extsym)
			*extsym = '_';
		extsym += extentry.len;
		extentry.addr = extern_symbol;
		extentry.len = extsym - extern_symbol;
		tabent = put_cdlt(&extentry);
	} else
	{
#	endif
#	ifdef VMS
		ref = newtriple(OC_FGNLOOKUP);
		ref->operand[0] = put_str(package.addr, package.len);
		ref->operand[1] = put_str(extentry.addr, extentry.len);
		tabent = put_tref(ref);
#	endif
#	ifdef VMS_CASE_SENSITIVE_MACROS
	}
#	endif
	calltrip = maketriple(a ? OC_FNFGNCAL : OC_FGNCAL);
	nxtopr = &calltrip->operand[1];
	ref = newtriple(OC_PARAMETER);
	ref->operand[0] = UNIX_ONLY(put_str(package.addr, package.len)) VMS_ONLY(tabent);
	*nxtopr = put_tref(ref);
	nxtopr = &ref->operand[1];
	cnt++;
#	ifdef UNIX
	ref = newtriple(OC_PARAMETER);
	ref->operand[0] = put_str(extentry.addr, extentry.len);
	*nxtopr = put_tref(ref);
	nxtopr = &ref->operand[1];
	cnt++;
#	endif
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
