/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_time.h"
#include <sys/time.h>

#include "dollarh.h"
#include "have_crit.h"

error_def(ERR_WEIRDSYSTIME);

long dollarh(time_t intime, uint4 *days, time_t *seconds)
{
	struct tm	*ttime;
	gtm_int8	local_wall_time_in_gmt, seconds_since_m_epoch;
	long		offset;
	int		isdst;

	GTM_LOCALTIME(ttime, &intime);
	*seconds  = (time_t)(ttime->tm_hour * HOUR) + (ttime->tm_min * MINUTE) + ttime->tm_sec;
#	ifdef  _BSD_SOURCE
	offset = -1L * ttime->tm_gmtoff;
#	else
	isdst = ttime->tm_isdst;
	GTM_GMTIME(ttime, &intime);
	ttime->tm_isdst = isdst;
	GTM_MKTIME(local_wall_time_in_gmt, ttime);
	assert(local_wall_time_in_gmt != -1);
	offset = local_wall_time_in_gmt - intime;
#	endif
	seconds_since_m_epoch = (intime - offset) + (1LL * DAYS * ONEDAY);
	if (seconds_since_m_epoch < 0)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_WEIRDSYSTIME);
	*days = (uint4)(seconds_since_m_epoch / ONEDAY);
	return offset;
}
