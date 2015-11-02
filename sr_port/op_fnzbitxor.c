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

static const unsigned char mask[8]={0xFF,0xFE,0xFC,0xF8,0xF0,0xE0,0xC0,0x80};

void op_fnzbitxor(mval *dst, mval *bitstr1, mval *bitstr2)
{
	int 		n, str_len1, str_len2, new_str_len;
	unsigned char	*byte_1, *byte_n;
	unsigned char	*byte1_1, *byte1_n, byte1_len;
	unsigned char	*byte2_1, *byte2_n, byte2_len;

	error_def(ERR_INVBITSTR);

	MV_FORCE_STR(bitstr1);
	MV_FORCE_STR(bitstr2);

	if (!bitstr1->str.len || !bitstr2->str.len)
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);

	byte1_len = *(unsigned char *)bitstr1->str.addr;
	str_len1 = (bitstr1->str.len - 1) * 8;
	if (7 < byte1_len)
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);

	byte2_len = *(unsigned char *)bitstr2->str.addr;
	str_len2 = (bitstr2->str.len -1) * 8;
	if (7 < byte2_len)
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);

	if (str_len1 - byte1_len > str_len2 - byte2_len)
		new_str_len = str_len2 - byte2_len;
	else
		new_str_len = str_len1 - byte1_len;
	n = (new_str_len + 7)/8 ;
	ENSURE_STP_FREE_SPACE(n + 1);
	byte_1 = (unsigned char *)stringpool.free;
	*byte_1 = n * 8 - new_str_len;
	byte1_1 = (unsigned char *)bitstr1->str.addr;
	byte2_1 = (unsigned char *)bitstr2->str.addr;

	for(byte_n = byte_1 + 1, byte1_n = byte1_1 + 1, byte2_n = byte2_1 + 1 ;
		 byte_n <= (byte_1 + n); byte_n++, byte1_n++, byte2_n++)
	{
		*byte_n = *byte1_n ^ *byte2_n;
	}

	*--byte_n &= mask[*byte_1];
	dst->mvtype = MV_STR;
	dst->str.addr = (char *)stringpool.free;
	dst->str.len = n + 1;
	stringpool.free += n + 1;
}
