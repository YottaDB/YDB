/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "zcall.h"

GBLREF zctabrtn	*zctab, *zctab_end;
GBLREF zcpackage *zcpack_start, *zcpack_end;

zctabrtn *op_fgnlookup (mval *package, mval *extref)
{
	zctabrtn	*zcrtn, *zcrtn_top;
	zcpackage	*zcpack;
	error_def	(ERR_ZCALLTABLE);

	assert(MV_IS_STRING(package));	/* package and routine are literal strings */
	assert(MV_IS_STRING(extref));

	if (package->str.len)
	{
		zcpack = zcpack_start;
		while (zcpack < zcpack_end)
		{
			if (*zcpack->packname == 0) rts_error (ERR_ZCALLTABLE);
			if ((*zcpack->packname == package->str.len) &&
				!memcmp (zcpack->packname + 1, package->str.addr, package->str.len))
				break;
			zcpack++;
		}
		if (zcpack >= zcpack_end)
		{	return (zctabrtn *) 0;
		}
		zcrtn = zcpack->begin;
		zcrtn_top = zcpack->end;
		if ((zcrtn > zctab_end || zcrtn < zctab) ||
			(zcrtn_top > zctab_end || zcrtn_top < zctab) || (zcrtn > zcrtn_top))
			 rts_error(ERR_ZCALLTABLE);
	}
	else
	{	zcrtn = zctab;
		zcrtn_top = zctab_end;
	}

	while (zcrtn < zcrtn_top)
	{
		if ((zcrtn >= zctab_end) || (zcrtn->entry_length == 0) || (zcrtn->callnamelen == 0))
			rts_error (ERR_ZCALLTABLE);
		if ((zcrtn->callnamelen == extref->str.len) &&
			!memcmp (zcrtn->callname, extref->str.addr, extref->str.len))
			break;
		zcrtn = (zctabrtn *) ((char *) zcrtn + zcrtn->entry_length);
	}
	if (zcrtn >= zcrtn_top)
	{	zcrtn = (zctabrtn *) 0;
	}
	return zcrtn;
}
