/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#if defined(__MVS__) && !defined(_ISOC99_SOURCE)
#define _ISOC99_SOURCE
#endif

#include "mdef.h"

#include <sys/time.h>
#include <errno.h>

#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
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

GBLDEF	int			gtmsource_sock_fd = FD_INVALID;
GBLREF	jnlpool_addrs		jnlpool;

int gtmsource_comm_init(void)
{
	/* Initialize communication stuff */

	const	int	disable_keepalive = 0;
	struct	linger	disable_linger = {0, 0};
	char	error_string[1024];
	int	err_status;

	error_def(ERR_REPLCOMM);
	error_def(ERR_TEXT);

	if (FD_INVALID != gtmsource_sock_fd) /* Initialization done already */
		return(0);

	/* Create the socket used for communicating with secondary */
	if (FD_INVALID == (gtmsource_sock_fd = socket(AF_INET, SOCK_STREAM, 0)))
	{
		err_status = ERRNO;
		SNPRINTF(error_string, SIZEOF(error_string), "Error with source server socket create : %s", STRERROR(err_status));
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(error_string));
		return(-1);
	}

	/* A connection breakage should get rid of the socket */

	if (-1 == setsockopt(gtmsource_sock_fd, SOL_SOCKET, SO_LINGER, (const void *)&disable_linger, SIZEOF(disable_linger)))
	{
		err_status = ERRNO;
		SNPRINTF(error_string, SIZEOF(error_string), "Error with source server socket disable linger : %s",
				STRERROR(err_status));
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(error_string));
	}

#ifdef REPL_DISABLE_KEEPALIVE
	if (-1 == setsockopt(gtmsource_sock_fd, SOL_SOCKET, SO_KEEPALIVE, (const void *)&disable_keepalive,
		SIZEOF(disable_keepalive)))
	{ /* Till SIGPIPE is handled properly */
		err_status = ERRNO;
		SNPRINTF(error_string, SIZEOF(error_string), "Error with source server socket disable keepalive : %s",
				STRERROR(err_status));
		rts_error(VARLSTCNT(6) ERR_REPLCOMM, 0, ERR_TEXT, 2, RTS_ERROR_STRING(error_string));
	}
#endif
	return(0);
}
