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

/* iosocket_read.c */

#include <sys/socket.h>
#include <netinet/in.h>
#include "mdef.h"
#include "io.h"
#include "gt_timer.h"
#include "iotcpdef.h"
#include "iosocketdef.h"

short	iosocket_read(mval *v, int4 timeout)
{
	return iosocket_readfl(v, 0, timeout); /* 0 means not fixed length */
}
