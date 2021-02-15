/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

error_def(ERR_INVBITSTR);

void op_fnzbitlen(mval *dst, mval *bitstr)
{
	int len, str_len;
	unsigned char *byte_1;

	MV_FORCE_STR(bitstr);

	if (!bitstr->str.len)
		RTS_ERROR_ABT(VARLSTCNT(1) ERR_INVBITSTR);

	byte_1 = (unsigned char *)bitstr->str.addr;
	str_len = (bitstr->str.len - 1) * 8;
	if (7 < *byte_1)
		RTS_ERROR_ABT(VARLSTCNT(1) ERR_INVBITSTR);
	else
		len = str_len - *byte_1;

	MV_FORCE_MVAL(dst, len);

}
