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

/* iosocket_rdone.c */

#include <sys/socket.h>
#include <netinet/in.h>
#include "mdef.h"
#include "io.h"
#include "gt_timer.h"
#include "iotcpdef.h"
#include "iosocketdef.h"

short	iosocket_rdone(mint *v, int4 timeout)
{
	mval	tmp;
	short	ret;

	ret = iosocket_readfl(&tmp, 1, timeout);
	if (ret)
	{
		*v = (int4)*(unsigned char *)(tmp.str.addr);
	} else
		*v = -1;

	return ret;
}
