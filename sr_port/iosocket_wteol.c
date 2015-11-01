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

/* iosocket_wteol.c */

/* write the 0th delimiter and flush */

#include "mdef.h"

#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifndef __MVS__
#include <netinet/tcp.h>
#endif
#include "gtm_stdio.h"

#include "io.h"
#include "iotcpdef.h"
#include "iotcproutine.h"
#include "gt_timer.h"
#include "iosocketdef.h"

GBLREF tcp_library_struct	tcp_routines;

void	iosocket_wteol(short x, io_desc *iod)
{
	int		on = 1, off = 0, rv;
	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;
	char		*errptr, ch = '\n';
	int4		errlen;
	mstr		v;

	error_def(ERR_SOCKWRITE);
	error_def(ERR_TEXT);

	assert(gtmsocket == iod->type);
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	socketptr = dsocketptr->socket[dsocketptr->current_socket];

	if (socketptr->n_delimiter > 0)
		iosocket_write(&socketptr->delimiter[0]);

	iosocket_flush(iod);

	return;
}
