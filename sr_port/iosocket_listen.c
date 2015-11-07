/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "iotcproutine.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"

GBLREF tcp_library_struct	tcp_routines;

error_def(ERR_CURRSOCKOFR);
error_def(ERR_LISTENPASSBND);
error_def(ERR_LQLENGTHNA);
error_def(ERR_SOCKLISTEN);
error_def(ERR_TEXT);

#define LISTENING		"LISTENING"
#define MAX_LISTEN_QUEUE_LENGTH	5

boolean_t iosocket_listen(io_desc *iod, unsigned short len)
{
	char		*errptr;
	int4		errlen;
	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;

	if (MAX_LISTEN_QUEUE_LENGTH < len)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_LQLENGTHNA, 1, len);
		return FALSE;
	}
	assert(gtmsocket == iod->type);
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	socketptr = dsocketptr->socket[dsocketptr->current_socket];
	if (dsocketptr->current_socket >= dsocketptr->n_socket)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CURRSOCKOFR, 2, dsocketptr->current_socket, dsocketptr->n_socket);
		return FALSE;
	}
	if ((socketptr->state != socket_bound) || (TRUE != socketptr->passive))
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_LISTENPASSBND);
		return FALSE;
	}
	dsocketptr->iod->dollar.key[0] = '\0';
	/* establish a queue of length len for incoming connections */
	if (-1 == tcp_routines.aa_listen(socketptr->sd, len))
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
	SPRINTF(&dsocketptr->iod->dollar.key[len], "%d", socketptr->local.port);
	return TRUE;
}
