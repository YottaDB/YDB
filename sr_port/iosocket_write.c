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
#include "dollarx.h"

GBLREF io_pair			io_curr_device;
GBLREF tcp_library_struct	tcp_routines;

void	iosocket_write(mstr *v)
{
	io_desc		*iod;
	char		*out;
	int		inlen, len, status;
	d_socket_struct *dsocketptr;
	socket_struct	*socketptr;

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
	if (0 != (inlen = v->len))
	{
		for (out = v->addr;  ; out += len)
		{
			if (!iod->wrap)
				len = inlen;
			else
			{
				if ((iod->dollar.x >= iod->width) && (START == iod->esc_state))
				{
					/* Should this really be iosocket_Wteol() (for FILTER)? IF we call iosocket_wteol(),
					 * there will be recursion iosocket_Write -> iosocket_Wteol ->iosocket_Write */
					DOTCPSEND(socketptr->sd, socketptr->delimiter[0].addr, socketptr->delimiter[0].len,
							(socketptr->urgent ? MSG_OOB : 0), status);
					if (0 != status)
					{
						SOCKERROR(iod, dsocketptr, socketptr, ERR_SOCKWRITE, status);
						return;
					}
					iod->dollar.y++;
					iod->dollar.x = 0;
				}
				if ((START != iod->esc_state) || ((int)(iod->dollar.x + inlen) <= (int)iod->width))
					len = inlen;
				else
					len = iod->width - iod->dollar.x;
			}
			assert(0 != len);
			DOTCPSEND(socketptr->sd, out, len, (socketptr->urgent ? MSG_OOB : 0), status);
			if (0 != status)
			{
				SOCKERROR(iod, dsocketptr, socketptr, ERR_SOCKWRITE, status);
				return;
			}
			dollarx(iod, (uchar_ptr_t)out, (uchar_ptr_t)out + len);
			inlen -= len;
			if (0 >= inlen)
				break;
		}
		iod->dollar.za = 0;
	}
	return;
}
