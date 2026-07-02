/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_handle.c */

/*	iosocket_new_handle
 *
 *		create a new handle
 *		return the old dsocketptr->n_socket
 *			(i.e. number of socket)
 *			(i.e. index of the new socket)
 *
 *	iosocket_get_handle
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
#include "gt_timer.h"
#include "iosocketdef.h"

int4 iosocket_new_handle(char *handle, int *len_p, const d_socket_struct *dsocketptr)
{
	boolean_t	unique;
	int4 		i;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	do
	{
		SNPRINTF(handle, MAX_HANDLE_LEN, "h%ld%03d", time((time_t *)0), ((TREF(socket_handle_counter))++ % 1000));
		*len_p = (int)strlen(handle);
		i = 0;
		unique = TRUE;
		while(i < dsocketptr->n_socket)
		{
			if ((*len_p == dsocketptr->socket[i]->handle_len) &&
				(0 == memcmp(handle, dsocketptr->socket[i]->handle, *len_p)))
			{
				unique = FALSE;
				break;
			}
			i++;
		}
		if (unique)
			return i;
	} while (TRUE);
	assert(FALSE);
	/* it will never reach here */
	return -1;

}

int4 iosocket_get_handle(const char *handle, int len, const d_socket_struct *dsocketptr)
{
	int4 		i;

	for (i = 0; i < dsocketptr->n_socket; i++)
	{
		if ((len == dsocketptr->socket[i]->handle_len) &&
			(0 == memcmp(handle, dsocketptr->socket[i]->handle, len)))
			return i;
	}
	return -1;
}
