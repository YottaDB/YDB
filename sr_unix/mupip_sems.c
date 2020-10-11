/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/sem.h>
#include "gtm_ipc.h"
#include "cli.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gtmio.h"
#include "iosp.h"
#include "mupip_sems.h"
#include "mupip_exit.h"
#include "gtm_sem.h"


#undef EXIT
#define	EXIT	exit	/* Use system "exit" (not gtm_image_exit) directly since this is a standalone module */
#define	OUT_LINE	80 + 1

/*	mupip_sems.c - display information about GT.M semaphores
 *
 *	semval - current value of the semaphore
 *	semncnt - count of processes waiting for the semaphore value to
 *			become greater than its current value.
 *	semzcnt - count of processes waiting for the semaphore value to
 *			become zero.
 *	sempid - last process id to perform an operation on the
 *			semaphore.
 */

void mupip_sems(void)
{
	const char		*statname[]	= {"sempid","semzcnt", "semncnt", "semval"};
	const int		stat[]		= { GETPID,  GETZCNT,   GETNCNT,   GETVAL};
	char			s[OUT_LINE];
	int			i, j, k, sem, semval;
	struct	semid_ds	semstat;
	union	semun		semarg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	semarg.buf = &semstat;
	for (i = 0; i < TREF(parms_cnt); i++)
	{	/*  in order to handle multiple smephores, this loop directly uses the array built by cli */
		strncpy(s, TAREF1(parm_ary, i), MAX_DIGITS_IN_INT);	/* because cli routines don't handle single type lists */
		s[MAX_DIGITS_IN_INT] = 0;
		sem = ATOI(s);
		if (-1 == semctl(sem, 0, IPC_STAT, semarg))
		{
			FPRINTF(stderr, "Error obtaining semaphore status.\n");
			SNPRINTF(s, OUT_LINE, "semctl(%s)", s);
			PERROR(s);
			continue;
		}
#		ifndef _AIX	/* at this writing, the key is known to be available for Linux, but not for AIX */
		FPRINTF(stderr, "semid %d :: semkey (0x%lx): %hu semaphores in the set\n", sem, semstat.sem_perm.__key,
		       (unsigned short int)semarg.buf->sem_nsems);
#		else
		FPRINTF(stderr, "semid %d: %hu semaphores in the set\n", sem, (unsigned short int)semarg.buf->sem_nsems);
#		endif
		for (j = 0; j < semarg.buf->sem_nsems; j++)
		{
			PRINTF("sem %2d: (", j);
			for (k = 3; 0 <= k ; k--)
			{
				if (-1 == (semval = semctl(sem, j, stat[k])))
				{
					FPRINTF(stderr, "Error obtaining semaphore %d %s.\n", j, statname[k]);
					SNPRINTF(s, OUT_LINE, "semctl(%d)", sem);
					PERROR(s);
					continue;
				}
				PRINTF("%s=%*d%s", statname[k], 8, semval, (k ? ", " : ")\n"));
			}
		}
	}
	mupip_exit(SS_NORMAL);
}
