/****************************************************************
 *								*
 * Copyright (c) 2015-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022-2023 YottaDB LLC and/or its subsidiaries.	*
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
#include "stringpool.h"
#include "op.h"
#include "dollarh.h"

GBLREF spdesc	stringpool;

error_def(ERR_WEIRDSYSTIME);

/* the first argument is the destination mval
 * the second argument is a flag indicating whether it the standard ISV ($HOROLOG - FALSE) or $ZHOROLOG - TRUE
 */

void op_zhorolog(mval *s, boolean_t z)
{
	uint4		days;
	time_t		seconds;
	struct timespec	ts;
	unsigned char	*strpool_free;
	long		gmtoffset;

	assert(stringpool.free <= stringpool.top);
	assert(stringpool.free >= stringpool.base);
	ENSURE_STP_FREE_SPACE(MAXNUMLEN + 1);
	strpool_free = stringpool.free;
	assertpro(-1 != clock_gettime(CLOCK_REALTIME, &ts));
#	ifdef DEBUG
	/* The OS should never return an invalid time */
	if ((ts.tv_sec < 0) || (ts.tv_nsec < 0) || (ts.tv_nsec > NANOSECS_IN_SEC))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_WEIRDSYSTIME);
#	endif
	seconds = ts.tv_sec;
	gmtoffset = dollarh(seconds, &days, &seconds);
	s->str.addr = (char *)strpool_free;
	strpool_free = i2asc(strpool_free, days);
	*strpool_free++ = ',';
	strpool_free = i2asc(strpool_free, (uint4)seconds);
	if (z)
	{
		*strpool_free++ = ',';
		strpool_free = i2asc(strpool_free, (uint4)(ts.tv_nsec / NANOSECS_IN_USEC));
		*strpool_free++ = ',';
		if (gmtoffset >= 0) /* The sign check is neccessary because i2ascl doesn't handle negative values */
			strpool_free = i2ascl(strpool_free, gmtoffset);
		else
		{
			*strpool_free++ = '-';
			strpool_free = i2ascl(strpool_free, -1UL * gmtoffset);
		}
	}
	s->str.len = INTCAST((char *)strpool_free - s->str.addr);
	s->mvtype = MV_STR;
	stringpool.free = strpool_free;
	return;
}
