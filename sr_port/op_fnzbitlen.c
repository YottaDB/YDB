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

void op_fnzbitlen(mval *dst, mval *bitstr)
{
	int len, str_len;
	unsigned char *byte_1;
	error_def(ERR_INVBITSTR);

	MV_FORCE_STR(bitstr);

	if (!bitstr->str.len)
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);

	byte_1 = (unsigned char *)bitstr->str.addr;
	str_len = (bitstr->str.len - 1) * 8;
	if (7 < *byte_1)
		rts_error(VARLSTCNT(1) ERR_INVBITSTR);
	else
		len = str_len - *byte_1;

	MV_FORCE_MVAL(dst, len);

}
