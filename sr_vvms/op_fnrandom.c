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

#include <lib$routines.h>
#include "gtm_stdlib.h"
#include "gtm_string.h"

#include "mvalconv.h"

GBLREF int4 process_id;

error_def(ERR_RANDARGNEG);

void op_fnrandom (int4 interval, mval *ret)
{
	static int4	seed = 0;
	int4		day;
	double		randfloat;

	if (1 > interval)
		rts_error(VARLSTCNT(1) ERR_RANDARGNEG);
	if (0 == seed)
	{
		lib$day(&day, 0, &seed);
		seed *= process_id;
		srandom(seed);
	}
	randfloat = ((double)random()) / RAND_MAX;
	MV_FORCE_MVAL(ret, ((uint4)(interval * randfloat)));
}
