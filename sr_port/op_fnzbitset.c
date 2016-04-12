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

#include "gtm_string.h"

#include "stringpool.h"
#include "op.h"

GBLREF spdesc stringpool;

static unsigned char	mask[8] = {1,128,64,32,16,8,4,2};

void op_fnzbitset(mval *dst, mval *bitstr, int pos, int truthval)
{
	int		mp, np, str_len;
	unsigned char	*byte_1, *dist_byte, byte_len;

	error_def(ERR_INVBITSTR);
	error_def(ERR_INVBITPOS);

	MV_FORCE_STR(bitstr);

	if (!bitstr->str.len)
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);

	byte_len = *(unsigned char *)bitstr->str.addr;
	str_len = (bitstr->str.len - 1) * 8;
	if (7 < byte_len)
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);
	if ((1> pos) || (pos > str_len - byte_len))
		rts_error(VARLSTCNT(1) ERR_INVBITPOS);
	ENSURE_STP_FREE_SPACE(bitstr->str.len);
	dist_byte = (unsigned char *)stringpool.free;
	byte_1 = (unsigned char *)bitstr->str.addr;
	memcpy(dist_byte, byte_1, bitstr->str.len);

	np = (pos + 7)/8 - 1;
	mp = pos % 8;
	byte_1 += np + 1;
	dist_byte += np + 1;

	if (truthval)
		*dist_byte = *byte_1 | mask[mp];
	else
		*dist_byte = *byte_1 & ~mask[mp];

	dst->mvtype = MV_STR;
	dst->str.addr = (char *)stringpool.free;
	dst->str.len = bitstr->str.len;
	stringpool.free += bitstr->str.len;
}
