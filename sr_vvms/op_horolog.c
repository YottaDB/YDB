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
#include "stringpool.h"
#include "op.h"

GBLREF spdesc stringpool;

void op_horolog(mval *s)
{
	int4 seconds,days;
	uint4 lib$day();

	assert (stringpool.free <= stringpool.top);
	assert (stringpool.free >= stringpool.base);
	ENSURE_STP_FREE_SPACE(MAXNUMLEN);
	lib$day(&days,0,&seconds);
	days += DAYS;
	seconds /= CENTISECONDS;
	s->str.addr = stringpool.free;
	stringpool.free  = i2s(&days);
	*stringpool.free++ = ',';
	stringpool.free = i2s(&seconds);
	s->str.len = (char *) stringpool.free - s->str.addr;
	s->mvtype = MV_STR;
	return;
}
