/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "op.h"
#include "mvalconv.h"

void op_fnzbitcoun(mval *dst, mval *bitstr)
{
	int str_len;
	unsigned char *byte_1, *byte_n, byte_0;
	int m, n, i;
	int bit_count;
	static unsigned char mask[8] = {128, 64, 32, 16, 8, 4, 2, 1};
	error_def(ERR_INVBITSTR);

	MV_FORCE_STR(bitstr);

	if (!bitstr->str.len)
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);

	byte_1 = (unsigned char *)bitstr->str.addr;
	str_len = (bitstr->str.len - 1) * 8;
	if (7 < *byte_1)
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);
	n = (str_len - *byte_1 + 7)/8 - 1;
	m = (str_len - *byte_1) % 8;
	if (0 == m)
		m = 8;
	bit_count = 0;
	for (byte_n = byte_1 + 1; byte_n <= (byte_1 + n); byte_n++)
	{
		for (i = 0; 8 > i; i++)
		{
			if (byte_0 = *byte_n & mask[i])
				bit_count++;
		}
	}
	for (i = 0; i < m; i++)
	{
		if (byte_0 = *byte_n & mask[i])
			bit_count++;
	}
	MV_FORCE_MVAL(dst, bit_count);
}
