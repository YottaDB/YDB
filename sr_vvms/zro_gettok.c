/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "toktyp.h"
#include "zroutines.h"

void zro_gettok (unsigned char **lp, unsigned char *top, unsigned *toktyp, mstr *tok)
{
	while (*lp < top && **lp == ' ')
		(*lp)++;
	if (*lp >= top)
		*toktyp = TK_EOL;
	else
	switch (**lp)
	{
	case ',':
		*toktyp = TK_COMMA;
		(*lp)++;
		break;
	case '=':
		*toktyp = TK_EQUAL;
		(*lp)++;
		break;
	case '(':
		*toktyp = TK_LPAREN;
		(*lp)++;
		break;
	case ')':
		*toktyp = TK_RPAREN;
		(*lp)++;
		break;
	case '/':
		*toktyp = TK_SLASH;
		(*lp)++;
		break;
	default:
		tok->addr = *lp;
		while (*lp < top && **lp != ',' && **lp != '=' && **lp != '(' && **lp != ')' && **lp != '/' && **lp != ' ')
			(*lp)++;
		*toktyp = TK_IDENT;
		tok->len = *lp - (unsigned char *) tok->addr;
		break;
	}
	return;
}
