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
#include "io.h"
#include "iosp.h"

GBLREF io_pair io_curr_device;

void iomt_wttab(short len)
{
	mstr spaces;
	int inlen;

	spaces.addr = (char *) SPACES_BLOCK;
	spaces.len = TAB_BUF_SZ;

	for (inlen = len; inlen > TAB_BUF_SZ; inlen -= TAB_BUF_SZ)
		iomt_write(&spaces);

	spaces.len = inlen;
	iomt_write(&spaces);

	return;
}
