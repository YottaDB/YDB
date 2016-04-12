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
#include <sys/time.h>
#include "gtm_time.h"
#include "stringpool.h"
#include "op.h"
#include "dollarh.h"

GBLREF spdesc	stringpool;

/* If you update this function, consider updating op_zhorolog() as well, however at this writing they use different services */
void op_horolog(mval *s)
{
	uint4		days;
	time_t		seconds;
	unsigned char	*strpool_free;

	assert(stringpool.free <= stringpool.top);
	assert(stringpool.free >= stringpool.base);
	ENSURE_STP_FREE_SPACE(MAXNUMLEN + 1);
	strpool_free = stringpool.free;
	seconds = time(NULL);
	dollarh(seconds, &days, &seconds);
	s->str.addr = (char *)strpool_free;
	strpool_free = i2asc(strpool_free, days);
	*strpool_free++ = ',';
	strpool_free = i2asc(strpool_free, (uint4)seconds);
	s->str.len = INTCAST((char *)strpool_free - s->str.addr);
	s->mvtype = MV_STR;
	stringpool.free = strpool_free;
	return;
}
