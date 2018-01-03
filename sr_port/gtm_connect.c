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

/*  get socket routines address */
#include "mdef.h"

#include "gtm_netdb.h"
#include "gtm_unistd.h"
#include <errno.h>
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_string.h"
#include "gtm_select.h"

int	gtm_connect(int socket, struct sockaddr *address, size_t address_len)
{
	int			res, sockerror;
	GTM_SOCKLEN_TYPE	sockerrorlen;
	fd_set			writefds;

	res = connect(socket, address, (GTM_SOCKLEN_TYPE)address_len);
	if ((-1 == res) && ((EINTR == errno) || (EINPROGRESS == errno)
#if (defined(__osf__) && defined(__alpha)) || defined(__sun) || defined(__vms)
			|| (EWOULDBLOCK == errno)
#endif
			 ))
	{/* connection attempt will continue so wait for completion */
		do
		{	/* a plain connect will usually timeout after 75 seconds with ETIMEDOUT */
			FD_ZERO(&writefds);
			FD_SET(socket, &writefds);
			res = select(socket + 1, NULL, &writefds, NULL, NULL);
			if (-1 == res && EINTR == errno)
				continue;
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

	return(res);
}
