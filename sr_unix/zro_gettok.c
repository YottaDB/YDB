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
#include "zroutines.h"

int zro_gettok (char **lp, char *top, mstr *tok)
{
	int	toktyp;

	if (*lp >= top)
		toktyp = ZRO_EOL;
	else
	switch (**lp)
	{
	case ZRO_DEL:

		toktyp = ZRO_DEL;
		while (*lp < top && **lp == ZRO_DEL)
			(*lp)++;
		break;
	case ZRO_LBR:
		toktyp = ZRO_LBR;
		(*lp)++;
		break;
	case ZRO_RBR:
		toktyp = ZRO_RBR;
		(*lp)++;
		break;
	default:
		tok->addr = *lp;
		while (*lp < top && **lp != ZRO_DEL && **lp != ZRO_LBR && **lp != ZRO_RBR)
			(*lp)++;
		toktyp = ZRO_IDN;
		tok->len = INTCAST(*lp - tok->addr);
		break;
	}
	return toktyp;
}
