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
#include "op.h"
#include "mvalconv.h"

void op_fnzbitget(mval *dst, mval *bitstr, int pos)
{
	int str_len;
	unsigned char *byte_1, byte_n;
	int mp, np;
	error_def(ERR_INVBITSTR);
	error_def(ERR_INVBITPOS);

	MV_FORCE_STR(bitstr);

	if (!bitstr->str.len)
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);

	byte_1 = (unsigned char *)bitstr->str.addr;
	str_len = (bitstr->str.len -1) * 8;
	if ((*byte_1 < 0) || (*byte_1 > 7))
	{
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);
	}
	if ((pos < 1) || (pos > str_len - *byte_1))
	{
		rts_error(VARLSTCNT(1) ERR_INVBITPOS);
	}
	np = ((pos + 7) / 8) - 1;
	mp = pos % 8;
	if (mp == 0)
		mp = 8;
	byte_n = *(byte_1 + np + 1);
	byte_n = byte_n << (mp - 1);
	byte_n = byte_n >> 7;
	MV_FORCE_MVAL(dst, (int)byte_n);
}
