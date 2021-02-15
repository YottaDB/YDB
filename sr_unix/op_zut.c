/****************************************************************
 *								*
 * Copyright (c) 2015-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#define FLOAT_SKEW	10	/* 1 order of magnitude microsec shrinkage to prevent floating point arithmetic from allowing
				 * ordered comparisons with other time ISVs to seem like time can go backward
				 */

void op_zut(mval *s)
{
	struct timeval	tv;
	gtm_int8	microseconds, msectmp;
	int		numdigs;
	int4		pwr;

	assertpro(-1 != gettimeofday(&tv, NULL));
	microseconds = (1LL * MICROSECS_IN_SEC * tv.tv_sec) + tv.tv_usec;
	if ((microseconds < 0) && (microseconds > E_18))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_WEIRDSYSTIME);
	msectmp = microseconds;
	/* Count the number of digits */
	for (numdigs = 0; msectmp; numdigs++, msectmp /= DECIMAL_BASE)
		;
	if (numdigs <= NUM_DEC_DG_1L)
	{
		s->m[0] = 0;
		s->m[1] = (int4)microseconds * ten_pwr[NUM_DEC_DG_1L - numdigs];
	} else
	{
		microseconds -= FLOAT_SKEW;	/* to prevent floating arithmetic from making time appear to run backwards */
		pwr = ten_pwr[numdigs - NUM_DEC_DG_1L];
		s->m[0] = (microseconds % pwr) * ten_pwr[NUM_DEC_DG_2L - numdigs];
		s->m[1] = microseconds / pwr;
	}
	s->mvtype = MV_NM;
	s->e = MV_XBIAS + numdigs;
	s->sgn = 0;
	return;
}
