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
#include <signal.h>

#include "io.h"
#include "gtmsecshr.h"
#include "send_msg.h"

int continue_proc(pid_t pid)
{
	error_def(ERR_NOSUCHPROC);

	if (0 == kill(pid, SIGCONT))
		return(0);
	else if (ESRCH == errno)
	{
		send_msg(VARLSTCNT(5) ERR_NOSUCHPROC, 3, pid, RTS_ERROR_LITERAL("continue"));
		return(ESRCH);
	} else
		assert(EINVAL != errno);
	return(send_mesg2gtmsecshr(CONTINUE_PROCESS, pid, (char *)NULL, 0));
}
