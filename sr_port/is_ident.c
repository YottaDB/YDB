/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "toktyp.h"

LITREF	char	ctypetab[NUM_CHARS];

char	is_ident(mstr *v)
{
	int4		y;
	char		*c, *c_top, ret;
	signed char	ch;
	bool		dig_start;

	ret = TRUE;
	c = v->addr;
	c_top = c + v->len;
	if (!v->len || (ch = *c++) < 0)
		ret = 0;
	else
	{
		switch (y = ctypetab[ch])
		{
		case TK_LOWER:
		case TK_PERCENT:
		case TK_UPPER:
		case TK_DIGIT:
			dig_start = y == TK_DIGIT;
			for ( ; c < c_top; c++)
			{
				ch = *c;
				if (ch < 0)
					break;
				y = ctypetab[ch];
				if (y != TK_DIGIT && dig_start)
					break;
				if (y != TK_DIGIT && y != TK_UPPER && y != TK_LOWER)
					break;
			}
			if (c == c_top)
			{	/* we have an ident */
				ret = 1 + dig_start;
			}
			else
			{	ret = 0;
			}
			break;
		default:
			ret = 0;
		}
	}
	return ret;
}
