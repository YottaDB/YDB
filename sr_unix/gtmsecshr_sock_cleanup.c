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

#include "mdef.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "io.h"
#include "gtmsecshr.h"
#include "error.h"
#include "send_msg.h"


GBLREF int			gtmsecshr_sockfd;
GBLREF struct sockaddr_un	gtmsecshr_sock_name;
GBLREF struct sockaddr_un	gtmsecshr_cli_sock_name;
GBLREF uint4			process_id;
GBLREF boolean_t		gtmsecshr_sock_init_done;

error_def(ERR_GTMSECSHR);
error_def(ERR_TEXT);

void gtmsecshr_sock_cleanup(int caller)
{
	int			save_errno;
	struct sockaddr_un	*sock_ptr;

	/* Close the secshr client socket */
	if (-1 != gtmsecshr_sockfd)
	{
		close(gtmsecshr_sockfd);
		gtmsecshr_sockfd = -1;
	}

	/* do the unlink */

	sock_ptr = (CLIENT == caller) ? &gtmsecshr_cli_sock_name : &gtmsecshr_sock_name;

	if (('\0' != sock_ptr->sun_path[0]) && (-1 == UNLINK(sock_ptr->sun_path))
		&& (ENOENT != errno))
	{
		save_errno = errno;
		send_msg(VARLSTCNT(12) ERR_GTMSECSHR, 1, process_id, ERR_TEXT, 2, RTS_ERROR_TEXT("unlinking socket"),
			ERR_TEXT, 2, RTS_ERROR_STRING(sock_ptr->sun_path), save_errno);
	}
	sock_ptr->sun_path[0] = '\0';	/* Even if error unlinking since it is useless now */

	gtmsecshr_sock_init_done = FALSE;
}
