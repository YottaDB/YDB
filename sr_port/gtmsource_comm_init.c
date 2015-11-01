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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_sp.h"
#include "repl_comm.h"

GBLDEF	int			gtmsource_sock_fd = -1;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	uint4			repl_max_send_buffsize, repl_max_recv_buffsize;

int gtmsource_comm_init(void)
{
	/* Initialize communication stuff */

	const	int	disable_keepalive = 0;
	struct	linger	disable_linger = {0, 0};

	error_def(ERR_REPLCOMM);
	error_def(ERR_TEXT);

	if (-1 != gtmsource_sock_fd) /* Initialization done already */
		return(0);

	/* Create the socket used for communicating with secondary */
	if (-1 == (gtmsource_sock_fd = socket(AF_INET, SOCK_STREAM, 0)))
	{
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error with source server socket create"), ERRNO);
		return(-1);
	}

	/* A connection breakage should get rid of the socket */

	if ( 0 > setsockopt(gtmsource_sock_fd, SOL_SOCKET, SO_LINGER, (const void *)&disable_linger, sizeof(disable_linger)))
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error with source server socket disable linger"), ERRNO);

#ifdef REPL_DISABLE_KEEPALIVE
	if (0 > setsockopt(gtmsource_sock_fd, SOL_SOCKET, SO_KEEPALIVE, (const void *)&disable_keepalive,
		sizeof(disable_keepalive)))
	{
		/* Till SIGPIPE is handled properly */
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error with source server socket disable keepalive"), ERRNO);
	}
#endif

	if (0 > get_sock_buff_size(gtmsource_sock_fd, &repl_max_send_buffsize, &repl_max_recv_buffsize))
	{
		rts_error(VARLSTCNT(7) ERR_REPLCOMM, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error getting socket send/recv buffsizes"), ERRNO);
		return -1;
	}

	return(0);
}
