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
#include "crit_wake.h"
#include "send_msg.h"
#include "have_crit.h"

int crit_wake (sm_uint_ptr_t pid)
{
	error_def(ERR_NOSUCHPROC);

	if (0 == kill(*pid, SIGALRM))
		return 0;
	else if (ESRCH == errno)
	{
		send_msg(VARLSTCNT(5) ERR_NOSUCHPROC, 3, *pid, RTS_ERROR_LITERAL("wake"));
		return(ESRCH);
	} else
		assert(EINVAL != errno);
	/* if you are in crit don't send, the other process's timer will wake it up any way */
	if (0 != have_crit(CRIT_HAVE_ANY_REG))
		return 0;
	return send_mesg2gtmsecshr(WAKE_MESSAGE, *pid, (char *)NULL, 0);
}
