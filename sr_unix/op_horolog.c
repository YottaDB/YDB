/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_time.h"
#include "stringpool.h"
#include "op.h"
#include "dollarh.h"

GBLREF spdesc	stringpool;

void op_horolog(mval *s)
{
	uint4 days;
	time_t seconds;

	assert (stringpool.free <= stringpool.top);
	assert (stringpool.free >= stringpool.base);
	ENSURE_STP_FREE_SPACE(MAXNUMLEN + 1);
	seconds = time(0);
	dollarh(seconds, &days, &seconds);
	s->str.addr = (char *) stringpool.free;
	stringpool.free  = i2asc(stringpool.free, days);
	*stringpool.free++ = ',';
	stringpool.free = i2asc(stringpool.free, (uint4)seconds);
	s->str.len = INTCAST((char *)stringpool.free - s->str.addr);
	s->mvtype = MV_STR;
	return;
}
