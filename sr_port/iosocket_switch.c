/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_switch.c */

#include "mdef.h"

#include "gtm_socket.h"
#include "gtm_inet.h"

#include "io.h"
#include "iosp.h"
#include "io_params.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"

GBLREF  int4	gtm_max_sockets;

boolean_t iosocket_switch(char *handle, int handle_len, d_socket_struct *from, d_socket_struct *to)
{
	int4 		index, ii;
	socket_struct	*socketptr;

	error_def(ERR_SOCKNOTFND);
	error_def(ERR_SOCKETEXIST);
	error_def(ERR_SOCKMAX);

	if ((NULL == from) ||
		(0 > (index = iosocket_handle(handle, &handle_len, FALSE, from))))
	{
		rts_error(VARLSTCNT(4) ERR_SOCKNOTFND, 2, handle_len, handle);
		return FALSE;
	} else
	{
		/* attach the socket to "to" and set it to current */

		assert(NULL != to);
		if (0 <= iosocket_handle(handle, &handle_len, FALSE, to))
		{
			rts_error(VARLSTCNT(4) ERR_SOCKETEXIST, 2, handle_len, handle);
			return FALSE;
		}
                if (gtm_max_sockets <= to->n_socket)
                {
                        rts_error(VARLSTCNT(3) ERR_SOCKMAX, 1, gtm_max_sockets);
                        return FALSE;
                }
		socketptr = from->socket[index];
		socketptr->dev = to;
		to->socket[to->n_socket++] = socketptr;
		to->current_socket = to->n_socket - 1;

		/* detach it from "from" */

		if (from->current_socket >= index)
			from->current_socket--;

		for(ii = index; ii < from->n_socket - 1; ii++)
		{
			from->socket[ii] = from->socket[ii + 1];
		}
		from->n_socket--;
		from->socket[from->n_socket] = NULL;
	}
	return TRUE;
}
