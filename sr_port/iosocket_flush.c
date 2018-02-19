/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "error.h"
#include "min_max.h"

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
	int		on = 1, off = 0;
	char		*errptr;
	int4		errlen;
	boolean_t	ch_set;

	assert(gtmsocket == iod->type);
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	socketptr = dsocketptr->socket[dsocketptr->current_socket];
	ESTABLISH_GTMIO_CH(&iod->pair, ch_set);
	if (0 >= dsocketptr->n_socket)
	{
		if (iod == io_std_device.out)
			ionl_flush(iod);
		else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOSOCKETINDEV);
		REVERT_GTMIO_CH(&iod->pair, ch_set);
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
	REVERT_GTMIO_CH(&iod->pair, ch_set);
	return;
}
