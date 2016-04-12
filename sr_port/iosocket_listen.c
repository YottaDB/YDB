/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_listen.c */
/* checks the socket state -- socket_bind */
/* checks the socket type  -- passive     */
/* start listening */

#include "mdef.h"

#include <errno.h>
#include "gtm_inet.h"
#include "gtm_socket.h"
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "io_params.h"
#include "io.h"
#include "gt_timer.h"
#include "iosocketdef.h"
#include "error.h"

error_def(ERR_CURRSOCKOFR);
error_def(ERR_LISTENPASSBND);
error_def(ERR_LQLENGTHNA);
error_def(ERR_NOSOCKETINDEV);
error_def(ERR_SOCKLISTEN);
error_def(ERR_TEXT);

#define LISTENING		"LISTENING"
#define MAX_LISTEN_QUEUE_LENGTH	5

boolean_t iosocket_listen(io_desc *iod, unsigned short len)
{
	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;
	boolean_t	result, ch_set;

	assert(gtmsocket == iod->type);
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	if (0 >= dsocketptr->n_socket)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOSOCKETINDEV);
		return FALSE;
	}
	if (dsocketptr->current_socket >= dsocketptr->n_socket)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CURRSOCKOFR, 2, dsocketptr->current_socket, dsocketptr->n_socket);
		return FALSE;
	}
	socketptr = dsocketptr->socket[dsocketptr->current_socket];
	assert(socketptr && (socketptr->dev == dsocketptr));

	ESTABLISH_RET_GTMIO_CH(&iod->pair, FALSE, ch_set);
	result = iosocket_listen_sock(socketptr, len);
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return result;
}

boolean_t iosocket_listen_sock(socket_struct *socketptr, unsigned short len)
{
	char		*errptr;
	int4		errlen;
	d_socket_struct	*dsocketptr;

	if (MAX_LISTEN_QUEUE_LENGTH < len)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_LQLENGTHNA, 1, len);
		return FALSE;
	}
	if (((socketptr->state != socket_bound) && (socketptr->state != socket_listening)) || (TRUE != socketptr->passive))
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_LISTENPASSBND);
		return FALSE;
	}
	dsocketptr = socketptr->dev;
	dsocketptr->iod->dollar.key[0] = '\0';
	/* establish a queue of length len for incoming connections */
	if (-1 == listen(socketptr->sd, len))
	{
		errptr = (char *)STRERROR(errno);
		errlen = STRLEN(errptr);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SOCKLISTEN, 0, ERR_TEXT, 2, errlen, errptr);
		return FALSE;
	}
	socketptr->state = socket_listening;
	len = SIZEOF(LISTENING) - 1;
	memcpy(&dsocketptr->iod->dollar.key[0], LISTENING, len);
	dsocketptr->iod->dollar.key[len++] = '|';
	memcpy(&dsocketptr->iod->dollar.key[len], socketptr->handle, socketptr->handle_len);
	len += socketptr->handle_len;
	dsocketptr->iod->dollar.key[len++] = '|';
	if (socket_local != socketptr->protocol)
		SPRINTF(&dsocketptr->iod->dollar.key[len], "%d", socketptr->local.port);
#	ifndef VMS
	else
	{
		STRNCPY_STR(&dsocketptr->iod->dollar.key[len],
			((struct sockaddr_un *)(socketptr->local.sa))->sun_path, DD_BUFLEN - len - 1);
	}
#	endif
	return TRUE;
}
