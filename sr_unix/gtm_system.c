/****************************************************************
 *								*
 * Copyright (c) 2013-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "gtm_string.h"

#include <sys/wait.h>
#include <errno.h>

#include "have_crit.h"
#include "fork_init.h"
#include "eintr_wrappers.h"
#include "ydb_getenv.h"
#include "wbox_test_init.h"

#define RESTOREMASK(RC)	SIGPROCMASK(SIG_SETMASK, &savemask, NULL, RC)

error_def(ERR_INVSTRLEN);

int gtm_system(const char *cmdline)
{
	return gtm_system_internal(NULL, NULL, NULL, cmdline);
}

int gtm_system_internal(const char *sh, const char *opt, const char *rtn, const char *cmdline)
{
	sigset_t		mask, savemask;
	pid_t			pid;
	int			stat;		/* child exit status */
	int			rc, ret;	/* return value from waitpid */
	int			len, shlen;
	intrpt_state_t		prev_intrpt_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DEFER_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM, prev_intrpt_state);

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGQUIT);
	SIGPROCMASK(SIG_BLOCK, &mask, &savemask, rc);
	if (rc)
	{
		ENABLE_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM, prev_intrpt_state);
		return -1;
	}
	/* Shell and command options */
	if (NULL == opt)
		opt = "-c";
	if (NULL == sh)
		sh = ydb_getenv(YDBENVINDX_GENERIC_SHELL, NULL_SUFFIX, NULL_IS_YDB_ENV_MATCH);
	sh = (NULL == sh) || ('\0' == *sh) ? "/bin/sh" : sh;
	/* Below FORK is not used as interrupts are already disabled at the
	 * beginning of this function
	 */
	pid = fork(); /* BYPASSOK */
	if (0 > pid)
	{
		RESTOREMASK(rc);
		ENABLE_INTERRUPTS(INTRPT_IN_FORK_OR_SYSTEM, prev_intrpt_state);
		return -1;
	} else if (0 == pid)
	{
		RESTOREMASK(rc);
		assert((NULL == cmdline) || ('\0' != *cmdline));
		assert(NULL != sh);
		assert(NULL != opt);
		if (WBTEST_ENABLED(WBTEST_BADEXEC_OP_ZEDIT))
			STRCPY(sh, "");
		/* "execl" (i.e. EXECL macro usage below) requires NULL to be the last parameter. All parameters before that
		 * should be non-NULL. So depending on the NULL-ness of rtn and cmdline construct the appropriate "execl"
		 * invocation. sh and opt are guaranteed non-NULL (asserted above) so they can be safely included always.
		 */
		if (NULL == cmdline)
		{
			if (NULL == rtn)
				EXECL(sh, sh, opt, NULL);
			else
				EXECL(sh, sh, opt, rtn, NULL);
		} else if (NULL == rtn)
			EXECL(sh, sh, opt, cmdline, NULL);
		else
			EXECL(sh, sh, opt, rtn, cmdline, NULL);
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
