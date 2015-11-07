/****************************************************************
 *								*
 *	Copyright 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

/* ceil_log2_table[i] = # of bits needed to represent i */
static int	ceil_log2_table[] = { 1, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4 };

/* Compute the ceiling(log_2(num)) where num is a 64-bit quantity */
int ceil_log2_64bit(gtm_uint64_t num)
{
	int		ret;

	assert(num);
	num--;
	ret = 0;
	if (((gtm_uint64_t)1 << 32) <= num)
	{
		ret += 32;
		num = num >> 32;
	}
	if ((1 << 16) <= num)
	{
		ret += 16;
		num = num >> 16;
	}
	if ((1 << 8) <= num)
	{
		ret += 8;
		num = num >> 8;
	}
	if ((1 << 4) <= num)
	{
		ret += 4;
		num = num >> 4;
	}
	assert(ARRAYSIZE(ceil_log2_table) > num);
	/* Now that "num" is a small number, use lookup table to speed up computation */
	ret += ceil_log2_table[num];
	return ret;
}

/* Compute the ceiling(log_2(num)) where num is a 32-bit quantity */
int ceil_log2_32bit(uint4 num)
{
	int		ret;

	assert(num);
	num--;
	ret = 0;
	if ((1 << 16) <= num)
	{
		ret += 16;
		num = num >> 16;
	}
	if ((1 << 8) <= num)
	{
		ret += 8;
		num = num >> 8;
	}
	if ((1 << 4) <= num)
	{
		ret += 4;
		num = num >> 4;
	}
	assert(ARRAYSIZE(ceil_log2_table) > num);
	/* Now that "num" is a small number, use lookup table to speed up computation */
	ret += ceil_log2_table[num];
	return ret;
}
