/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2020 YottaDB LLC and/or its subsidiaries. *
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
#include "libyottadb.h"

GBLREF  mval			dollar_zstatus;

/* Routine to fetch $ZSTATUS after an error has been raised. If not enough room to fetch message
 * plus a trailing null byte, copy what we can (if any) and return an INVSTRLEN error.
 */
int ydb_zstatus(char *msg, int len)
{
	int	msg_len;

	msg_len = (len <= dollar_zstatus.str.len) ? len - 1 : dollar_zstatus.str.len;
	if (0 > msg_len)
		return YDB_ERR_INVSTRLEN;			/* Check if room even for null terminator */
	if (0 < msg_len)
		memcpy(msg, dollar_zstatus.str.addr, msg_len);	/* Copy what we can of message to buffer */
	msg[msg_len] = 0;					/* Add null terminator */
	return (msg_len == dollar_zstatus.str.len) ? YDB_OK : YDB_ERR_INVSTRLEN;
}
