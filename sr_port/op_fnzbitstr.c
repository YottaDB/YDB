/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2021-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

GBLREF spdesc	stringpool;

error_def(ERR_INVBITLEN);

void op_fnzbitstr(mval *bitstr, int size, int truthval)
{
	unsigned char 	*byte_1, *byte_n;
	static const unsigned char mask[8]={0xFF,0xFE,0xFC,0xF8,0xF0,0xE0,0xC0,0x80};
	int	n;

	if ((size <= 0) || (size > 253952))
		RTS_ERROR_ABT(VARLSTCNT(1) ERR_INVBITLEN);
	n = (size + 7) / 8;
	ENSURE_STP_FREE_SPACE(n + 1);
	byte_1 = (unsigned char *)stringpool.free;
	*byte_1 = n * 8 - size;
	if (truthval)
	{
		memset((char *)stringpool.free + 1, 0xFF, n);
		byte_n = byte_1 + n;
		*byte_n &= mask[*byte_1];
	} else
		memset((char *)stringpool.free + 1, 0, n);
	bitstr->mvtype = MV_STR;
	bitstr->str.addr = (char *)stringpool.free;
	bitstr->str.len = n + 1;
	stringpool.free += n + 1;
}
