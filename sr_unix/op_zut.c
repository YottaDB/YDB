/****************************************************************
 *								*
 * Copyright (c) 2015 Fidelity National Information 		*
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

void op_zut(mval *s)
{
	struct timeval	tv;
	gtm_int8	microseconds, msectmp;
	int		numdigs;
	int4		pwr;

	assertpro(-1 != gettimeofday(&tv, NULL));
	microseconds = (1LL * MICROSEC_IN_SEC * tv.tv_sec) + tv.tv_usec;
	if ((microseconds < 0) && (microseconds > E_18))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_WEIRDSYSTIME);
	if (microseconds < E_6)
	{
		s->m[1] = ((int4)microseconds * 1000);
		s->mvtype = MV_INT | MV_NM;
	} else
	{
		msectmp = microseconds;
		/* Count the number of digits */
		for (numdigs = 0; msectmp; numdigs++, msectmp /= 10);
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
	}
	s->sgn = 0;
	return;
}
