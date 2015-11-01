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

#ifndef MUTEX_MSEM_WAKE

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "gtm_stdio.h"
#include "gtm_unistd.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mutex.h"
#include "send_msg.h"

GBLREF int			mutex_sock_fd;
GBLREF struct sockaddr_un	mutex_sock_address;

error_def(ERR_MUTEXERR);
error_def(ERR_TEXT);

void mutex_sock_cleanup(void)
{
	int save_errno;

	/* Close the mutex wake socket */
	if (-1 != mutex_sock_fd)
	{
		close(mutex_sock_fd);
		mutex_sock_fd = -1;
	}
	if ((NULL != mutex_sock_address.sun_path) && (-1 == UNLINK(mutex_sock_address.sun_path))
		&& (ENOENT != errno))
	{
		save_errno = errno;
		send_msg(VARLSTCNT(11) ERR_MUTEXERR, 0, ERR_TEXT, 2, RTS_ERROR_TEXT("unlinking socket"),
			ERR_TEXT, 2, LEN_AND_STR(mutex_sock_address.sun_path), save_errno);
	}
	DUMP_MUTEX_TRACE_CNTRS;
}

#endif /*MUTEX_MSEM_WAKE*/
