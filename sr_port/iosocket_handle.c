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

/* iosocket_handle.c */

/*	newhandle == TRUE
 *
 *		create a new handle
 *		return the old dsocketptr->n_socket
 *			(i.e. number of socket)
 *			(i.e. index of the new socket)
 *
 *	newhandle == FALSE
 *
 *		check if the handle exist
 *		yes ==> return the index
 *		no  ==> return -1
 *			(return the number of sockets would provide more information, but can cause
 *			 confliction with index = 0
 *				0 ==> socket exist and index is 0
 *				0 ==> there are 0 sockets exist)
 */

#include "mdef.h"

#include "gtm_time.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "io_params.h"
#include "io.h"
#include "iotcproutine.h"
#include "iotcpdef.h"
#include "gt_timer.h"
#include "iosocketdef.h"

int4 iosocket_handle(char *handle, int *len, boolean_t newhandle, d_socket_struct *dsocketptr)
{
	boolean_t	unique;
	int4 		ii, counter = 0, loop_flag = 1;

	while(loop_flag)
	{
		if (newhandle)
		{
			SPRINTF(handle, "h%ld%d", time((time_t *)0), counter);
			*len = (short)strlen(handle);
		}
		ii = 0;
		unique = TRUE;
		while(ii < dsocketptr->n_socket)
		{
			if ((*len == dsocketptr->socket[ii]->handle_len) &&
				(0 == memcmp(handle, dsocketptr->socket[ii]->handle, *len)))
			{
				unique = FALSE;
				break;
			}
			ii++;
		}
		if (!newhandle)
			return	(unique ? -1 : ii);
		if (unique)
			return ii;
		counter++;
	}
	/* it will never reach here */
	return -1;
}
