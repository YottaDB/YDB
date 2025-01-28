/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_time.h"

#include "op.h"
#include "mvalconv.h"
#include "xoshiro.h"
#include <errno.h>

GBLREF uint4	process_id;
GBLREF uint64_t sm64_x;
GBLREF uint64_t x256_s[4];

error_def(ERR_RANDARGNEG);

void op_fnrandom (int4 interval, mval *ret)
{
	int4		random;
	FILE		*file;

	if (1 >= interval)
	{
		if (1 > interval)
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_RANDARGNEG);
		/* Else: it is $random(1). The result/random-value is always 0. */
		random = 0;
	} else
	{
		if (0 == sm64_x)
		{	// initialize the random number generator if uninitialized
			file = fopen("/dev/urandom", "r");
			if (NULL == file)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
					      LEN_AND_LIT("fopen() of /dev/urandom"), CALLFROM, errno);
			else
			{
				if (0 == fread(&sm64_x, sizeof(sm64_x), 1, file))
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_SYSCALL, 5,
						      LEN_AND_LIT("fread() of /dev/urandom"), CALLFROM);
				else
				{
					if (0 != fclose(file))
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,
							      LEN_AND_LIT("fclose() of /dev/urandom"), CALLFROM, errno);
					for (int i = 0; i < 4; i++)
						x256_s[i] = sm64_next();
				}
			}
		}
		// get a random number and convert it to a positive 4 byte integer then mod it by the interval
		random	= (int4)(x256_next() >> 33) % interval;
	}
	MV_FORCE_MVAL(ret, random);
}
