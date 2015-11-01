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

/* iosocket_wtff.c */

#include <sys/socket.h>
#include <netinet/in.h>
#include "mdef.h"
#include "io.h"
#include "gt_timer.h"
#include "iotcpdef.h"
#include "iosocketdef.h"

GBLREF io_pair		io_curr_device;

void iosocket_wtff(void)
{
	io_desc		*iod;

	iod = io_curr_device.out;
	iosocket_flush(iod);
	iod->dollar.x = 0;
	iod->dollar.y = 0;

	return;
}
