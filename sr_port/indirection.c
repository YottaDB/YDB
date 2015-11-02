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
#include "gtm_string.h"		/* needed by INCREMENT_EXPR_DEPTH */
#include "compiler.h"
#include "opcode.h"
#include "mdq.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "fullbool.h"
#include "show_source_line.h"
#include "stringpool.h"

GBLREF	boolean_t	run_time;
GBLREF	char		*lexical_ptr;
GBLREF	spdesc		stringpool;
GBLREF	short int	source_column;

error_def(ERR_BOOLSIDEFFECT);
error_def(ERR_LPARENMISSING);
error_def(ERR_MAXNRSUBSCRIPTS);
error_def(ERR_RPARENMISSING);
error_def(ERR_SIDEEFFECTEVAL);

int indirection(oprtype *a)
{
	char		c, source_line_buff[MAX_SRCLINE + SIZEOF(ARROW)];
	oprtype		*sb1, *sb2, subs[MAX_INDSUBSCRIPTS], x;
	triple		*next, *ref;
	int		parens, oldlen, len;
	char		*start, *end, *oldend;
	boolean_t	concat_athashes;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(TK_ATSIGN == TREF(window_token));
	concat_athashes = (2 == source_column);
	INCREMENT_EXPR_DEPTH;
	advancewindow();
	if (!expratom(a))
	{
		DECREMENT_EXPR_DEPTH;
		return FALSE;
	}
	coerce(a, OCT_MVAL);
	ex_tail(a);
	ENCOUNTERED_SIDE_EFFECT;
	DECREMENT_EXPR_DEPTH;
	if ((TK_ATSIGN == TREF(window_token)) || ((TK_ATHASH == TREF(window_token)) && concat_athashes))
	{
		(TREF(indirection_mval)).mvtype = 0;
		(TREF(indirection_mval)).str.len = 0;
		do {
			start = lexical_ptr;
			advancewindow();
			if (TK_LPAREN != TREF(window_token))
			{
				stx_error(ERR_LPARENMISSING);
				return FALSE;
			}
			for (parens = 1; (0 != parens) && (TK_EOL != TREF(window_token)); )
			{
				advancewindow();
				if (TK_LPAREN == TREF(window_token))
					parens++;
				else if (TK_RPAREN == TREF(window_token))
					parens--;
			}
			if (0 != parens)
			{
				stx_error(ERR_RPARENMISSING);
				return FALSE;
			}
			for (end = lexical_ptr - 1; *end != ')'; )
				end--;
			len = INTCAST(end - start) + 1;
			oldlen = (TREF(indirection_mval)).str.len;
			ENSURE_STP_FREE_SPACE(oldlen + len);
			/* Ok to copy from beginning each iteration because we generally expect no more than two iterations,
			 * and that's with nested indirection.
			 */
			memcpy(stringpool.free, (TREF(indirection_mval)).str.addr, oldlen);
			if (oldlen)
			{
				oldend = (char *)stringpool.free + oldlen - 1;
				assert(*oldend == ')');
				*oldend = ',';
			}
			(TREF(indirection_mval)).mvtype = MV_STR;
			(TREF(indirection_mval)).str.addr = (char *)stringpool.free;
			(TREF(indirection_mval)).str.len = oldlen + len;
			stringpool.free += oldlen;
			memcpy(stringpool.free, start, len);
			stringpool.free += len;
			advancewindow();
		} while ((TK_ATHASH == TREF(window_token)) && concat_athashes);
		ref = newtriple(OC_INDNAME);
		ref->operand[0] = *a;
		ref->operand[1] = put_lit(&(TREF(indirection_mval)));
		(TREF(indirection_mval)).mvtype = 0;	/* so stp_gcol (BYPASSOK) - if invoked later - can free up space */
		*a = put_tref(ref);
	}
	return TRUE;
}
