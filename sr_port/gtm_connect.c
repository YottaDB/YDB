/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*  get socket routines address */
#include "mdef.h"

#include "gtm_netdb.h"
#include "gtm_unistd.h"
#include <errno.h>
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_string.h"
<<<<<<< HEAD
#include "gtm_select.h"
#include "eintr_wrappers.h"
=======
#include "gtm_poll.h"
>>>>>>> f9ca5ad6 (GT.M V7.1-000)

int	gtm_connect(int socket, struct sockaddr *address, size_t address_len)
{
	int			res, sockerror;
	GTM_SOCKLEN_TYPE	sockerrorlen;
	int			poll_timeout;
	nfds_t			poll_nfds;
	struct pollfd		poll_fdlist[1];

	res = connect(socket, address, (GTM_SOCKLEN_TYPE)address_len);
	if ((-1 == res) && ((EINTR == errno) || (EINPROGRESS == errno)))
	{	/* connection attempt will continue so wait for completion */
		do
		{	/* a plain connect will usually timeout after 75 seconds with ETIMEDOUT */
<<<<<<< HEAD
			FD_ZERO(&writefds);
			FD_SET(socket, &writefds);
			res = select(socket + 1, NULL, &writefds, NULL, NULL);
			if ((-1 == res) && (EINTR == errno))
			{
				eintr_handling_check();
=======
			poll_fdlist[0].fd = socket;
			poll_fdlist[0].events = POLLOUT;
			poll_nfds = 1;
			poll_timeout = -1;
			res = poll(&poll_fdlist[0], poll_nfds, poll_timeout);
			if (-1 == res && EINTR == errno)
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
				continue;
			}
			if (0 < res)
			{	/* check for socket error */
				sockerrorlen = SIZEOF(sockerror);
				res = getsockopt(socket, SOL_SOCKET, SO_ERROR, &sockerror, &sockerrorlen);
				if (0 == res && 0 != sockerror)
				{	/* return socket error */
					res = -1;
					errno = sockerror;
				}
			}
			break;
		} while (TRUE);
	} else if (-1 == res && EISCONN == errno)
		res = 0;		/* socket is already connected */
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
	return(res);
}
