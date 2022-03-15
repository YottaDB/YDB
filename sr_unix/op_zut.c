/****************************************************************
 *								*
 * Copyright (c) 2015-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include <sys/time.h>
#include "gtm_time.h"
#include "op.h"
#include "arit.h"

LITREF	int4	ten_pwr[];

error_def(ERR_WEIRDSYSTIME);

#define DECIMAL_BASE	10	/* stolen from gdsfhead which is silly to include here */

void op_zut(mval *s)
{
	struct timespec	ts;
	gtm_int8	microseconds, msectmp;
	int		numdigs;
	int4		pwr;

	assertpro(-1 != clock_gettime(CLOCK_REALTIME, &ts));
#ifdef DEBUG
	/* The OS should never return an invalid time */
	if ((ts.tv_sec < 0) || (ts.tv_nsec < 0) || (ts.tv_nsec > NANOSECS_IN_SEC))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_WEIRDSYSTIME);
#endif
	/* $ZUT original supported only up to microsecond granularity. While it is tempting to
	 * expose upto nanosecond granularity, doing so is a major change to the interface.
	 */
	msectmp = microseconds = (1LL * MICROSECS_IN_SEC * ts.tv_sec) + (ts.tv_nsec / NANOSECS_IN_USEC);
	assert(0 < microseconds);

	/* Count the number of digits */
	for (numdigs = 0; msectmp; numdigs++, msectmp /= DECIMAL_BASE)
		;
	if (numdigs <= NUM_DEC_DG_1L)
	{
		s->m[0] = 0;
		s->m[1] = (int4)microseconds * ten_pwr[NUM_DEC_DG_1L - numdigs];
	} else
	{
		pwr = ten_pwr[numdigs - NUM_DEC_DG_1L];
		s->m[0] = (microseconds % pwr) * ten_pwr[NUM_DEC_DG_2L - numdigs];
		s->m[1] = microseconds / pwr;
	}
	s->mvtype = MV_NM;
	s->e = MV_XBIAS + numdigs;
	s->sgn = 0;
	return;
}
