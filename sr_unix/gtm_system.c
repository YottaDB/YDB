/****************************************************************
 *								*
 * Copyright (c) 2013-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_unistd.h"
#include "gtm_stdlib.h"
#include "gtm_signal.h"

#include <sys/wait.h>
#include <errno.h>

#include "have_crit.h"
#include "fork_init.h"

#include "eintr_wrappers.h"

#define RESTOREMASK(RC)					\
{							\
	sigaction(SIGINT, &old_intrpt, NULL);		\
	sigaction(SIGQUIT, &old_quit, NULL);		\
	SIGPROCMASK(SIG_SETMASK, &savemask, NULL, RC);	\
}

int gtm_system(const char *cmdline)
{
	struct sigaction	ignore, old_intrpt, old_quit;
	sigset_t		mask, savemask;
	pid_t			pid;
	int			stat;		/* child exit status */
	int			rc, ret;	/* return value from waitpid */
	intrpt_state_t		prev_intrpt_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DEFER_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM, prev_intrpt_state);

	sigemptyset(&ignore.sa_mask);
	ignore.sa_handler = SIG_IGN;
	ignore.sa_flags = 0;

	if (sigaction(SIGINT, &ignore, &old_intrpt))
	{
		ENABLE_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM, prev_intrpt_state);
		return -1;
	}
	if (sigaction(SIGQUIT, &ignore, &old_quit))
	{
		sigaction(SIGINT, &old_intrpt, NULL);
		ENABLE_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM, prev_intrpt_state);
		return -1;
	}
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	SIGPROCMASK(SIG_BLOCK, &mask, &savemask, rc);
	if (rc)
	{
		sigaction(SIGINT, &old_intrpt, NULL);
		sigaction(SIGQUIT, &old_quit, NULL);
		ENABLE_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM, prev_intrpt_state);
		return -1;
	}
	/* Below FORK is not used as interrupts are already disabled at the
	 * beginning of this function
	 */
	pid = fork(); /* BYPASSOK */
	if (0 > pid)
	{
		RESTOREMASK(rc);
		ENABLE_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM, prev_intrpt_state);
		return -1;
	}
	else if (0 == pid)
	{
		RESTOREMASK(rc);
		execl("/bin/sh", "sh", "-c", cmdline, (char *)0);
		UNDERSCORE_EXIT(127);
	} else
	{
		ENABLE_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM, prev_intrpt_state);
		WAITPID(pid, &stat, 0, ret);
		if ((-1 == ret) && (EINTR != errno))
			stat = -1;
		RESTOREMASK(rc);
		return stat;
	}
}
