/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

GBLREF uint4 process_id;

error_def(ERR_RANDARGNEG);

double          drand48(void);
void            srand48(long int);

void op_fnrandom (int4 interval, mval *ret)
{
	static int4	seed = 0;
	int4		random;

	if (interval < 1)
	{
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_RANDARGNEG);
	}
	if (seed == 0)
	{
		seed = (int4)(time(0) * process_id);
		srand48(seed);
	}
	random	= (int4)(interval * drand48());
	random	= random & 0x7fffffff;  /* to make sure that the sign bit(msb) is off*/
	MV_FORCE_MVAL(ret, random);
}
