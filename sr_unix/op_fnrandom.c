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

#include "gtm_string.h"
#include "gtm_time.h"

#include "op.h"
#include "mvalconv.h"

GBLREF int4 process_id;

void op_fnrandom (int4 interval, mval *ret)
{
	double		drand48();
	void		srand48();
	static int4	seed = 0;
	error_def	(ERR_RANDARGNEG);

	if (interval < 1)
	{
		rts_error(VARLSTCNT(1) ERR_RANDARGNEG);
	}
	if (seed == 0)
	{
		seed = time(0) * process_id;
		srand48(seed);
	}
	MV_FORCE_MVAL(ret, ((uint4)(interval * drand48())));
}
