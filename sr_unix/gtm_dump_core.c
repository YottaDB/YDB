/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "gtm_signal.h"	/* for SIGPROCMASK */

#include "error.h"
#include "gtmcrypt.h"

void gtm_dump_core(void)
{
	struct sigaction        act;
	char                    newname[20];
	int                     rc, suffix, status;
	struct stat             fs1;
	sigset_t		unblock_sigquit;

	/* Scrub any encryption related information before taking a core dump */
#	ifndef DEBUG_NOSCRUB
	GTMCRYPT_CLOSE;
#	endif

	sigemptyset(&act.sa_mask);
#	ifdef _AIX
	act.sa_flags = SA_FULLDUMP;
#	else
	act.sa_flags = 0;
#	endif
	act.sa_handler = SIG_DFL;
	sigaction(SIGQUIT, &act, 0);

	/* We are about to generate a core file. If one already exists on the disk,
	   make a simplistic attempt to rename it so we can get the most useful info
	   possible. */

	if (0 == Stat("core", &fs1))            /* If core exists (and stat command works) */
	{
		status = -1;
		for (suffix = 1; 0 != status && suffix < 100; ++suffix)
		{
			SPRINTF(&newname[0], "core%d", suffix);         /* Make new file name */
			status = Stat(&newname[0], &fs1);               /* This file exist ? */
			if (0 != status)
				status = RENAME("core", &newname[0]);   /* No, attempt the rename */
			else
				status = -1;                            /* Yes, reset status for another iteration */
		}
	}
	/* Even if signals are disabled at this point (for instance online rollback), the SIGQUIT below will be useless. So,
	 * unblock SIGQUIT unconditionally as we are anyways about to die.
	 */
	sigemptyset(&unblock_sigquit);
	sigaddset(&unblock_sigquit, SIGQUIT);
	SIGPROCMASK(SIG_UNBLOCK, &unblock_sigquit, NULL, rc);
	kill(getpid(), SIGQUIT);
	/* The below sleep function should NOT be converted to LONG_SLEEP() despite what ftpput says. This sleep is just
	 * waiting for the preceding signal to take effect so it should not run hiber_start() since this is NOT the main
	 * process but a fork-inspired facsimile spawned for the sole purpose of generating an appropriate core.
	 */
	sleep(60);	/* BYPASSOK */
	UNDERSCORE_EXIT(EXIT_FAILURE);
}
