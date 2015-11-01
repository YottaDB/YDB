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

#include <stdlib.h>
#include <signal.h>

#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "error.h"

void gtm_dump_core(void)
{
        struct sigaction        act;
        char                    newname[20];
        int                     suffix, status;
        struct stat             fs1;

        sigemptyset(&act.sa_mask);
#ifdef _AIX
	act.sa_flags = SA_FULLDUMP;
#else
        act.sa_flags = 0;
#endif
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
                        sprintf(&newname[0], "core%d", suffix);         /* Make new file name */
                        status = Stat(&newname[0], &fs1);               /* This file exist ? */
                        if (0 != status)
                                status = rename("core", &newname[0]);   /* No, attempt the rename */
                        else
                                status = -1;                            /* Yes, reset status for another iteration */
                }
        }
        kill(getpid(), SIGQUIT);
	sleep(30);	/* In case of async kill */
}
