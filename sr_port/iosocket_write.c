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

/* iosocket_write.c */

#include "mdef.h"

#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "io.h"
#include "iotcpdef.h"
#include "iotcproutine.h"
#include "gt_timer.h"
#include "iosocketdef.h"

#define	ONE_COMMA		"1,"

GBLREF io_pair			io_curr_device;
GBLREF tcp_library_struct	tcp_routines;

void	iosocket_write(mstr *v)
{
	io_desc		*iod;
	char		*out;
	int		inlen, outlen, size;
	d_socket_struct *dsocketptr;
	socket_struct	*socketptr;
	char		*errptr;
	int4		errlen;

	error_def(ERR_SOCKWRITE);
	error_def(ERR_TEXT);
	error_def(ERR_CURRSOCKOFR);

	iod = io_curr_device.out;

	assert(gtmsocket == iod->type);

	dsocketptr = (d_socket_struct *)iod->dev_sp;
	socketptr = dsocketptr->socket[dsocketptr->current_socket];

	if (dsocketptr->n_socket <= dsocketptr->current_socket)
	{
		rts_error(VARLSTCNT(4) ERR_CURRSOCKOFR, 2, dsocketptr->current_socket, dsocketptr->n_socket);
		return;
	}

	socketptr->lastop = TCP_WRITE;
	memcpy(dsocketptr->dollar_device, "0", sizeof("0"));
	inlen = v->len;
	outlen = iod->width - iod->dollar.x;

	if (!iod->wrap && inlen > outlen)
		inlen = outlen;
	if (!inlen)
		return;
	for (out = v->addr;  ;  out += size)
	{
		if (outlen > inlen)
			outlen = inlen;
		if ((size = tcp_routines.aa_send(socketptr->sd, out, outlen, (socketptr->urgent ? MSG_OOB : 0))) == -1)
		{
			iod->dollar.za = 9;
			memcpy(dsocketptr->dollar_device, ONE_COMMA, sizeof(ONE_COMMA));
			errptr = (char *)STRERROR(errno);
			errlen = strlen(errptr);
			memcpy(&dsocketptr->dollar_device[sizeof(ONE_COMMA) - 1], errptr, errlen);
			if ((iod->error_handler.len > 0) && (socketptr->ioerror))
				rts_error(VARLSTCNT(6) ERR_SOCKWRITE, 0, ERR_TEXT, 2, errlen, errptr);
			else
				return;
		}
		assert(size == outlen);
		iod->dollar.x += size;
		if ((inlen -= size) <= 0)
			break;

		iod->dollar.x = 0;	/* don't use wteol to terminate wrapped records for fixed. */
		iod->dollar.y++;	/* \n is reserved as an end-of-rec delimiter for variable format */
		if (iod->length)	/* and fixed format requires no padding for wrapped records */
			iod->dollar.y %= iod->length;

		outlen = iod->width;
	}
	iod->dollar.za = 0;

	return;
}
