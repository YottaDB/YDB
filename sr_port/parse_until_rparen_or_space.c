/****************************************************************
 *								*
 *	Copyright 2007, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "toktyp.h"
#include "advancewindow.h"

/* This routine parses input until it finds a RIGHT PAREN or SPACE or EOL.
 * It places the parse cursor AT the RIGHT-PAREN or SPACE or EOL before returning.
 * Returns FALSE in case of parse error.
 * Returns TRUE otherwise.
 */
int	parse_until_rparen_or_space()
{
	int		openparen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	for ( openparen = 1; ; )
	{
		if (TK_RPAREN == TREF(window_token))
		{
			openparen--;
			if (0 >= openparen)
				break;
		} else if (TK_LPAREN == TREF(window_token))
			openparen++;
		else if ((TK_EOL == TREF(window_token)) || (TK_SPACE == TREF(window_token)))
			break;
		else if (TK_ERROR == TREF(window_token))
			return FALSE;
		advancewindow();
	}
	return TRUE;
}
