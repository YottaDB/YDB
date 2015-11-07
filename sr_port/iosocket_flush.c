/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_flush.c */

#include "mdef.h"

#include "gtm_socket.h"
#include "gtm_inet.h"
#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "io.h"
#include "gt_timer.h"
#include "iosocketdef.h"

error_def(ERR_CURRSOCKOFR);
error_def(ERR_NOSOCKETINDEV);
error_def(ERR_SOCKPASSDATAMIX);
error_def(ERR_SOCKWRITE);
error_def(ERR_TEXT);

GBLREF	io_pair	io_std_device;

void iosocket_flush(io_desc *iod)
{

	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;
	ssize_t		status;
	int             on = 1, off = 0;
        char            *errptr;
        int4            errlen;

	assert(gtmsocket == iod->type);

	dsocketptr = (d_socket_struct *)iod->dev_sp;
	socketptr = dsocketptr->socket[dsocketptr->current_socket];

	if (0 >= dsocketptr->n_socket)
	{
#		ifndef VMS
		if (iod == io_std_device.out)
			ionl_flush(iod);
		else
#		endif
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOSOCKETINDEV);
		return;
	}
	if (dsocketptr->current_socket >= dsocketptr->n_socket)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CURRSOCKOFR, 2, dsocketptr->current_socket, dsocketptr->n_socket);
		return;
	}
	ENSURE_DATA_SOCKET(socketptr);
	if (socketptr->obuffer_timer_set)
	{
		cancel_timer((TID)socketptr);
		socketptr->obuffer_timer_set = FALSE;
	}
	if (!socketptr->obuffer_output_active)
	{	/* just to be safe */
		status = 1;			/* OK value */
		if ((0 < socketptr->obuffer_length) && (0 == socketptr->obuffer_errno))
		{
			socketptr->obuffer_output_active = TRUE;
			status = iosocket_output_buffer(socketptr);
			socketptr->obuffer_output_active = FALSE;
		}
		if ((0 < socketptr->obuffer_size) && ((0 >= status) || (0 != socketptr->obuffer_errno)))
			iosocket_buffer_error(socketptr);	/* pre-existing error or error flushing buffer */
	}
#ifdef C9A06001531
	/* pending change request C9A06001531 */
        memcpy(iod->dollar.device, "0", SIZEOF("0"));
        if ( -1 == setsockopt(socketptr->sd, SOL_SOCKET, TCP_NODELAY, &on, SIZEOF(on)) ||
		(-1 == setsockopt(socketptr->sd, SOL_SOCKET, TCP_NODELAY, &off, SIZEOF(off))))
        {
		errptr = (char *)STRERROR(errno);
                errlen = strlen(errptr);
                iod->dollar.za = 9;
		MEMCPY_LIT(iod->dollar.device, "1,");
                memcpy(&iod->dollar.device[SIZEOF("1,") - 1], errptr, errlen + 1);	/* we want the null */
		if (socketptr->ioerror)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_SOCKWRITE, 0, ERR_TEXT, 2, errlen, errptr);
		return;
        }

#endif
	return;
}
