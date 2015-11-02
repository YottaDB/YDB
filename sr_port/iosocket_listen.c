/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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

#define LISTENING		"LISTENING"
#define MAX_LISTEN_QUEUE_LENGTH	5

GBLREF tcp_library_struct	tcp_routines;

boolean_t iosocket_listen(io_desc *iod, unsigned short len)
{
	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;
       	char            *errptr;
        int4            errlen;

  	error_def(ERR_SOCKLISTEN);
        error_def(ERR_TEXT);
	error_def(ERR_LQLENGTHNA);
	error_def(ERR_SOCKACTNA);
	error_def(ERR_CURRSOCKOFR);
	error_def(ERR_LISTENPASSBND);

	if (MAX_LISTEN_QUEUE_LENGTH < len)
	{
		rts_error(VARLSTCNT(3) ERR_LQLENGTHNA, 1, len);
		return FALSE;
	}

	assert(iod->type == gtmsocket);
        dsocketptr = (d_socket_struct *)iod->dev_sp;
	socketptr = dsocketptr->socket[dsocketptr->current_socket];

	if (dsocketptr->current_socket >= dsocketptr->n_socket)
	{
		rts_error(VARLSTCNT(4) ERR_CURRSOCKOFR, 2, dsocketptr->current_socket, dsocketptr->n_socket);
		return FALSE;
	}

	if ((socketptr->state != socket_bound) || (socketptr->passive != TRUE))
	{
		rts_error(VARLSTCNT(1) ERR_LISTENPASSBND);
		return FALSE;
	}

	dsocketptr->dollar_key[0] = '\0';

        /* establish a queue of length len for incoming connections */
        if (-1 == tcp_routines.aa_listen(socketptr->sd, len))
        {
                errptr = (char *)STRERROR(errno);
                errlen = STRLEN(errptr);
                rts_error(VARLSTCNT(6) ERR_SOCKLISTEN, 0, ERR_TEXT, 2, errlen, errptr);
                return FALSE;
        }

	socketptr->state = socket_listening;

	len = SIZEOF(LISTENING) - 1;
	memcpy(&dsocketptr->dollar_key[0], LISTENING, len);
	dsocketptr->dollar_key[len++] = '|';
	memcpy(&dsocketptr->dollar_key[len], socketptr->handle, socketptr->handle_len);
	len += socketptr->handle_len;
	dsocketptr->dollar_key[len++] = '|';
	SPRINTF(&dsocketptr->dollar_key[len], "%d", socketptr->local.port);

	return TRUE;
}
