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

/* iosocket_wteol.c */
/* write the 0th delimiter and flush */

#include "mdef.h"

#include "gtm_socket.h"
#include "gtm_inet.h"

#include "gt_timer.h"
#include "io.h"
#include "iottdef.h"
#include "iosocketdef.h"
#include "error.h"

#ifndef VMS
GBLREF	io_pair		io_std_device;
#endif

error_def(ERR_CURRSOCKOFR);
error_def(ERR_NOSOCKETINDEV);
error_def(ERR_SOCKPASSDATAMIX);

void	iosocket_wteol(int4 val, io_desc *io_ptr)
{
	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;
	char		*ch, *top;
	int		eol_cnt;
	boolean_t	ch_set;

	assert(gtmsocket == io_ptr->type);
	dsocketptr = (d_socket_struct *)io_ptr->dev_sp;
	ESTABLISH_GTMIO_CH(&io_ptr->pair, ch_set);
	if (0 >= dsocketptr->n_socket)
	{
#		ifndef VMS
		if (io_ptr == io_std_device.out)
			ionl_wteol(val, io_ptr);
		else
#		endif
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOSOCKETINDEV);
		REVERT_GTMIO_CH(&io_ptr->pair, ch_set);
		return;
	}
	if (dsocketptr->current_socket >= dsocketptr->n_socket)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CURRSOCKOFR, 2, dsocketptr->current_socket, dsocketptr->n_socket);
		return;
	}
	socketptr = dsocketptr->socket[dsocketptr->current_socket];
	ENSURE_DATA_SOCKET(socketptr);
	assert(val);
	io_ptr->esc_state = START;
	if (socketptr->n_delimiter > 0)
	{
		for (eol_cnt = val; eol_cnt--; )
		{
			io_ptr->dollar.x = 0; /* so that iosocket_write doesn't try to wrap (based on escape state and width) */
			iosocket_write_real(&socketptr->odelimiter0, FALSE);
		}
	}
	/* $X is maintained in VMS without the below assignment (resetting to 0) because the NATIVE_TTEOL is \015\012
	 * and the <CR> (\015) triggers appropriate maintenance of $X.  In UNIX, NATIVE_TTEOL is \012, so
	 * FILTER=CHARACTER effectively turns off all $X maintenance (except for WRAP logic).
	 * In VMS the below assignment is not necessary, but harmless; it is always logically correct.
	 */
	io_ptr->dollar.x = 0;
	if (!(io_ptr->write_filter & CHAR_FILTER) || !socketptr->delim0containsLF)
	{	/* If FILTER won't do it, also maintain $Y */
		io_ptr->dollar.y += val;
		if (io_ptr->length)
			io_ptr->dollar.y %= io_ptr->length;
	}
	iosocket_flush(io_ptr);
	REVERT_GTMIO_CH(&io_ptr->pair, ch_set);
	return;
}
