/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

error_def(ERR_INVBITSTR);

void op_fnzbitnot(mval *dst,mval *bitstr)
{
	int		n, str_len;
	unsigned char	*byte_1, *byte_n, *dist_byte, byte_len;

	MV_FORCE_STR(bitstr);
	if (!bitstr->str.len)
		RTS_ERROR_ABT(VARLSTCNT(1) ERR_INVBITSTR);

	byte_len = *(unsigned char *)bitstr->str.addr;
	str_len = (bitstr->str.len - 1) * 8;
	if (7 < byte_len)
		RTS_ERROR_ABT(VARLSTCNT(1) ERR_INVBITSTR);

	ENSURE_STP_FREE_SPACE(bitstr->str.len);
	byte_1 = (unsigned char *)bitstr->str.addr;
	dist_byte = (unsigned char *)stringpool.free;
	*dist_byte = *byte_1;
	dist_byte++;

	n = bitstr->str.len;
	for (byte_n = byte_1 + 1; byte_n < (byte_1 + n); byte_n++, dist_byte++)
		*dist_byte = ~(*byte_n);

	dst->mvtype = MV_STR;
	dst->str.addr = (char *)stringpool.free;
	dst->str.len = bitstr->str.len;
	stringpool.free += bitstr->str.len;
}
