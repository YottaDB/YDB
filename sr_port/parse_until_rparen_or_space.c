/****************************************************************
 *								*
 *	Copyright 2007 Fidelity Information Services, Inc	*
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

GBLREF	char	window_token;

/* This routine parses input until it finds a RIGHT PAREN or SPACE or EOL.
 * It places the parse cursor AT the RIGHT-PAREN or SPACE or EOL before returning.
 */
void	parse_until_rparen_or_space()
{
	int		lparencnt, rparencnt;

	lparencnt = 1;
	rparencnt = 0;
	for ( ; ; )
	{
		if (TK_RPAREN == window_token)
		{
			rparencnt++;
			if (rparencnt >= lparencnt)
				break;
		} else if (TK_LPAREN == window_token)
			lparencnt++;
		else if ((TK_EOL == window_token) || (TK_SPACE == window_token))
			break;
		advancewindow();
	}
}
