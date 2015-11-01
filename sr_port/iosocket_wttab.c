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

/* iosocket_wttab.c */

#include <sys/socket.h>
#include <netinet/in.h>
#include "mdef.h"
#include "io.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"

LITREF unsigned char spaces_block[];

void iosocket_wttab(short len)
{
	mstr	temp;
	int 	ii, jj;

	temp.addr = (char *)spaces_block;
	if (0 != (ii = len / TAB_BUF_SZ))
	{
		temp.len = TAB_BUF_SZ;
		jj = ii;
		while (0 != jj--)
			iosocket_write(&temp);
	}
	if (0 != (ii = len - ii * TAB_BUF_SZ))
	{
		temp.len = ii;
		iosocket_write(&temp);
	}

	return;
}
