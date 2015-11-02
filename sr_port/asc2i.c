/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

GBLREF	seq_num		seq_num_zero;
GBLREF	seq_num		seq_num_minus_one;

int4 asc2i(uchar_ptr_t p, int4 len)
{
	uchar_ptr_t	c;
	int4		ret;

	ret = 0;
	for (c = p + len; c > p; p++)
	{	if (*p > '9' || *p < '0')
			return -1;
		ret = ret * 10;
		ret += *p - '0';
	}
	return ret;
}

qw_num asc2l(uchar_ptr_t p, int4 len)
{
	uchar_ptr_t	c;
	qw_num		ret;

	QWASSIGN(ret, seq_num_zero);
	for (c = p + len; c > p; p++)
	{	if (*p > '9' || *p < '0')
			return seq_num_minus_one;
		QWMULBYDW(ret, ret, 10);
		QWINCRBYDW(ret, *p - '0');
	}
	return ret;
}
