/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "zcall.h"
#include <stdarg.h>

GBLREF zctabrtn	*zctab, *zctab_end;

void op_fnzcall(mval *dst, ...)
{
	va_list		var;
	mval 		*v;
	int4		n_mvals;	/* number of input parameters supplied in $ZCALL command */
	int		i;
	mval		*mvallist[256];
	zctabrtn	*zcrtn;
	unsigned char	*lastout;
	error_def	(ERR_ZCALLTABLE);
	error_def	(ERR_ZCRTENOTF);
	error_def	(ERR_ZCARGMSMTCH);


	VAR_START(var, dst);
	va_count(n_mvals);
	v = va_arg(var, mval *);
	MV_FORCE_STR(v);

	zcrtn = zctab;
	while (zcrtn < zctab_end)
	{
		if (!zcrtn->entry_length)
		{
			va_end(var);
			rts_error(VARLSTCNT(1) ERR_ZCALLTABLE);
		}
		if (zcrtn->callnamelen == 0)
		{
			va_end(var);
			rts_error(VARLSTCNT(1) ERR_ZCALLTABLE);
		}
		if ((zcrtn->callnamelen == v->str.len) &&
			!memcmp (zcrtn->callname, v->str.addr, v->str.len))
			break;
		zcrtn = (zctabrtn *) ((char *) zcrtn + zcrtn->entry_length);
	}

	if (zcrtn == zctab_end)
	{
		va_end(var);
		rts_error(VARLSTCNT(4) ERR_ZCRTENOTF, 2, v->str.len, v->str.addr);
	}

	n_mvals -= 2;
	if (n_mvals > zcrtn->n_inputs)
	{
		va_end(var);
		rts_error(VARLSTCNT(4) ERR_ZCARGMSMTCH, 2, n_mvals, zcrtn->n_inputs);
	}

	lastout = (unsigned char *) zcrtn
		  + ROUND_UP(SIZEOF(zctabrtn) + zcrtn->callnamelen - 1 + SIZEOF(zctabret), SIZEOF(int4))
		  + zcrtn->n_inputs * SIZEOF(zctabinput)
		  + zcrtn->n_outputs * SIZEOF(zctaboutput);

	if (ROUND_UP((int) lastout + 1, SIZEOF(int4)) != (unsigned char *) zcrtn + zcrtn->entry_length)
	{
		va_end(var);
		rts_error(VARLSTCNT(1) ERR_ZCALLTABLE);
	}

	for (i = 0;  i < n_mvals;  ++i)
		mvallist[i] = va_arg(var, mval *);
	va_end(var);

	zc_makespace (dst, 0, mvallist, &mvallist[n_mvals], zcrtn, *lastout);
	return;
}
