/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <stdarg.h>

#include "zcall.h"
#include "have_crit.h"

GBLREF volatile int4	gtmMallocDepth;

error_def(ERR_ZCALLTABLE);
error_def(ERR_ZCRTENOTF);
error_def(ERR_ZCARGMSMTCH);

void op_fnfgncal (mval *dst, ...)
{
	va_list		var;
	int4		mask, i, argcnt;
	mval		*mvallist[256];		/* maximum of fewer than 256 arguments passed via VAX calls instruction */
	zctabrtn	*zcrtn;
	unsigned char	*lastout;

	va_start(var, dst);
	zcrtn = va_arg(var, zctabrtn *);
	if (!zcrtn)
	{
		va_end(var);
		rts_error (VARLSTCNT(4) ERR_ZCRTENOTF, 2, 0, 0);
	}
	mask = va_arg(var, int4);
	argcnt = va_arg(var, int4);

	if (argcnt > zcrtn->n_inputs)
	{
		va_end(var);
		rts_error (ERR_ZCARGMSMTCH, 2, argcnt, zcrtn->n_inputs);
	}

	lastout = (unsigned char *) zcrtn
		  + ROUND_UP(SIZEOF(zctabrtn) + zcrtn->callnamelen - 1 + SIZEOF(zctabret), SIZEOF(int4))
		  + zcrtn->n_inputs * SIZEOF(zctabinput)
		  + zcrtn->n_outputs * SIZEOF(zctaboutput);

	if (ROUND_UP((int) lastout + 1, SIZEOF(int4)) != (unsigned char *) zcrtn + zcrtn->entry_length)
	{
		va_end(var);
		rts_error (ERR_ZCALLTABLE);
	}

	for (i = 0;   i < argcnt;  i++)
		mvallist[i] = va_arg(var, mval *);
	va_end(var);

	assert(INTRPT_OK_TO_INTERRUPT == intrpt_ok_state); /* interrupts should be enabled for external calls */

	zc_makespace (dst, mask, mvallist, &mvallist[argcnt], zcrtn, *lastout);

	return;
}
