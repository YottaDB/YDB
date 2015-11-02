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

void op_fnzbitor(mval *dst, mval *bitstr1, mval *bitstr2)
{
	bool short1;
	int str_len1, str_len2;
	unsigned char *byte_1, *byte_n;
	unsigned char *byte1_1, *byte1_n, byte1_len;
	unsigned char *byte2_1, *byte2_n, byte2_len;
	int n, n0;
	error_def(ERR_INVBITSTR);

	MV_FORCE_STR(bitstr1);
	MV_FORCE_STR(bitstr2);

	if (!bitstr1->str.len || !bitstr2->str.len)
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);

	byte1_len = *(unsigned char *)bitstr1->str.addr;
	if (byte1_len > 7)
	{
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);
	}
	byte2_len = *(unsigned char *)bitstr2->str.addr;
	if (byte2_len > 7)
	{
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);
	}
	str_len1 = bitstr1->str.len - 1;
	str_len2 = bitstr2->str.len - 1;

	if ((str_len1 * 8 - byte1_len) < (str_len2 * 8 - byte2_len))
	{
		short1 = TRUE;
		n = str_len2;
		n0 = str_len1;
	} else
	{
		short1 = FALSE;
		n = str_len1;
		n0 = str_len2;
	}

	ENSURE_STP_FREE_SPACE(n + 1);
	byte_1 = (unsigned char *)stringpool.free;
  	*byte_1 = short1 ? byte2_len : byte1_len;
	byte1_1 = (unsigned char *)bitstr1->str.addr;
	byte2_1 = (unsigned char *)bitstr2->str.addr;

	for(byte_n = byte_1 + 1, byte1_n = byte1_1 + 1, byte2_n = byte2_1 + 1;
		 byte_n <= (byte_1 + n0);
		 byte_n++, byte1_n++, byte2_n++)
	{
		*byte_n = *byte1_n | *byte2_n;
	}
	if (n != n0)
		memcpy(byte_n, short1 ? byte2_n : byte1_n, n - n0);
	dst->mvtype = MV_STR;
	dst->str.addr = (char *)stringpool.free;
	dst->str.len = n + 1;
	stringpool.free += n + 1;
}
