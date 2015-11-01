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

#include <sys/socket.h>
#include <netinet/in.h>
#ifndef __MVS__
#include <netinet/tcp.h>
#endif

#include "gt_timer.h"
#include "io.h"
#include "iotcpdef.h"
#include "iotcproutine.h"
#include "iottdef.h"
#include "iosocketdef.h"

GBLREF tcp_library_struct	tcp_routines;

void	iosocket_wteol(short val, io_desc *io_ptr)
{
	d_socket_struct	*dsocketptr;
	socket_struct	*socketptr;
	char		*ch, *top;
	int		eol_cnt;

	assert(gtmsocket == io_ptr->type);
	dsocketptr = (d_socket_struct *)io_ptr->dev_sp;
	socketptr = dsocketptr->socket[dsocketptr->current_socket];
	assert(val);
	io_ptr->esc_state = START;
	if (socketptr->n_delimiter > 0)
	{
		for (eol_cnt = val; eol_cnt--; )
		{
			io_ptr->dollar.x = 0; /* so that iosocket_write doesn't try to wrap (based on escape state and width) */
			iosocket_write(&socketptr->delimiter[0]);
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
	return;
}
