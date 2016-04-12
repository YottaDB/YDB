/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
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
	gtm_int8	local_wall_time_in_gmt, seconds_since_m_epoch;
	int		isdst;
	long		offset;
	struct tm	*ttime;

	GTM_LOCALTIME(ttime, &intime);
	*seconds  = (time_t)(ttime->tm_hour * HOUR) + (time_t)(ttime->tm_min * MINUTE) + (time_t)ttime->tm_sec;
#	ifdef  _BSD_SOURCE						/* the BSD structure provides the UTC offset in seconds */
	offset = -1L * ttime->tm_gmtoff;				/* using 1L here and 1LL below makes 32 bit platforms OK */
#	else								/* otherwise have to calulate it */
	isdst = ttime->tm_isdst;
	GTM_GMTIME(ttime, &intime);					/* recast intime to UTC */
	ttime->tm_isdst = isdst;
	GTM_MKTIME(local_wall_time_in_gmt, ttime);			/* turn it back into seconds */
	assert(local_wall_time_in_gmt != -1);
	offset = local_wall_time_in_gmt - intime;			/* subtract the original time to determine UTC offset */
#	endif
	seconds_since_m_epoch = (intime - offset) + (1LL * DAYS * ONEDAY);
	if (seconds_since_m_epoch < 0)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_WEIRDSYSTIME);
	*days = (uint4)(seconds_since_m_epoch / ONEDAY);		/* after adjusting for UTC we can get the days */
	return offset;
}
