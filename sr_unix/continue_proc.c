/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "gtm_limits.h"

#include "io.h"
#include "gtmsecshr.h"
#include "secshr_client.h"
#include "send_msg.h"
#include "is_proc_alive.h"
#include "wbox_test_init.h"

error_def(ERR_NOSUCHPROC);

int continue_proc(pid_t pid)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DEBUG_ONLY(if (!TREF(gtm_usesecshr)))	/* Cause debug builds to talk to gtmsecshr more often */
	{
		if (WBTEST_ENABLED(WBTEST_HOLD_GTMSOURCE_SRV_LATCH))
		{
			/* Simulate the kill below, but ignore its return status so that we end up invoking gtmsecshr */
			kill(pid, SIGCONT);
			/* Wait until the target quits so that kill() call by gtmsecshr fails with ESRCH */
			while (is_proc_alive(pid, 0))
				LONG_SLEEP(1);
		}
		else if (0 == kill(pid, SIGCONT))
			return 0;
		else if (ESRCH == errno)
		{
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_NOSUCHPROC, 3, pid, RTS_ERROR_LITERAL("continue"));
			return ESRCH;
		} else
			assert(EINVAL != errno);
	}
	return send_mesg2gtmsecshr(CONTINUE_PROCESS, pid, NULL, 0);
}
