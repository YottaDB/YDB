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
#include "mvalconv.h"
#include "op.h"

void op_fnzbitfind(mval *dst, mval *bitstr, int truthval, int pos)
{
	int str_len, find_bit;
	unsigned char *byte_1, *byte_n, byte_0;
	int m, n, mp, np, i, i1, j;
	static const unsigned char mask[8] = {0x80,0x40,0x20,0x10,0x8,0x4,0x2,0x1};
	error_def(ERR_INVBITSTR);

	MV_FORCE_STR(bitstr);

	if (!bitstr->str.len)
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);

	byte_1 = (unsigned char *)bitstr->str.addr;
	str_len = (bitstr->str.len -1) * 8;
	if (*byte_1 > 7)
	{
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);
	}
	if (pos < 1)
	{
		pos = 1; /* It is the way it works in DATA TREE */
	}
	else if (pos > str_len - *byte_1)
	{
		find_bit = 0;
		MV_FORCE_MVAL(dst, find_bit);
		return; /* It is the way it works in DATA TREE */
	}
	np = (pos + 7)/8;
	mp = pos % 8;
	if (mp == 0)
		mp = 8;
	n = (str_len - *byte_1 + 7)/8 - 1;
	m = (str_len - *byte_1) % 8;
	if (m == 0)
		m = 8;
	byte_n = byte_1 + np;
	find_bit = 0;
	if (truthval)
	{
		if (np == n + 1)
			i1 = m;
		else
			i1 = 8;
		for (i = mp - 1; i < i1; i++)
		{
			if (byte_0 = *byte_n & mask[i])
			{
				find_bit = i + 2 + (np - 1)*8;
				MV_FORCE_MVAL(dst, find_bit);
				return;
			}
		}
		if (np == n + 1)
		{
			MV_FORCE_MVAL(dst, find_bit);
			return;
		}
		for(j = 1; j <= n - np; j++)
		{
			byte_n++;
			for (i = 0; i < 8; i++)
			{
				if (byte_0 = *byte_n & mask[i])
				{
					find_bit = i + 2 + (np + j - 1)*8;
					MV_FORCE_MVAL(dst, find_bit);
					return;
				}
			}
		}
		byte_n++;
		for (i = 0; i < m; i++)
		{
			if (byte_0 = *byte_n & mask[i])
			{
				find_bit = i + 2 + n*8;
				MV_FORCE_MVAL(dst, find_bit);
				return;
			}
		}
	}
	else
	{
		if (np == n + 1)
			i1 = m;
		else
			i1 = 8;
		for (i = mp - 1; i < i1; i++)
		{
			if (!(byte_0 = *byte_n & mask[i]))
			{
				find_bit = i + 2 + (np - 1)*8;
				MV_FORCE_MVAL(dst, find_bit);
				return;
			}
		}
		if (np == n + 1)
		{
			MV_FORCE_MVAL(dst, find_bit);
			return;
		}
		for(j = 1; j <= n - np; j++)
		{
			byte_n++;
			for (i = 0; i < 8; i++)
			{
				if (!(byte_0 = *byte_n & mask[i]))
				{
					find_bit = i + 2 + (np + j - 1)*8;
					MV_FORCE_MVAL(dst, find_bit);
					return;
				}
			}
		}
		byte_n++;
		for (i = 0; i < m; i++)
		{
			if (!(byte_0 = *byte_n & mask[i]))
			{
				find_bit = i + 2 + n*8;
				MV_FORCE_MVAL(dst, find_bit);
				return;
			}
		}
	}
	MV_FORCE_MVAL(dst, find_bit);
	return;
}
