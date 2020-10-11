/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*	semstat2.c - display more information about semaphores (than semstat did)
 *
 *	semval - current value of the semaphore
 *	semncnt - count of processes waiting for the semaphore value to
 *			become greater than its current value.
 *	semzcnt - count of processes waiting for the semaphore value to
 *			become zero.
 *	sempid - last process id to perform an operation on the
 *			semaphore.
 *
 *	Usage:  semstat2 <semid1> <semid2> ... <semidn>
 */

#include "main_pragma.h"
#include <errno.h>
#include <sys/types.h>
#include "gtm_ipc.h"
#include <sys/sem.h>
#include "gtm_sem.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"

#undef EXIT
#define	EXIT	exit	/* Use system "exit" (not gtm_image_exit) directly since this is a standalone module */
#define	OUT_LINE	(80 + 1)

static void	usage (char *prog);

static void	usage (char *prog)
{
	FPRINTF(stderr, "%s:  Report information about semaphores\n", prog);
	FPRINTF(stderr, "\nUsage-\n");
	FPRINTF(stderr, "\t%s <semid1> <semid2> ... <semidn>\n\n", prog);
	FPRINTF(stderr, "information returned for each semaphore in set:\n");
	FPRINTF(stderr, "\tsemval\t\tcurrent value of semaphore\n");
	FPRINTF(stderr, "\tsemncnt\t\t# of procs waiting for semval to increase\n");
	FPRINTF(stderr, "\tsemzcnt\t\t# of procs waiting for semval to become zero\n");
	FPRINTF(stderr, "\tsempid\t\tPID of last proc performing operation on semaphore\n");
}


int main (int argc, char *argv[])
{
	const char 	*statname[]	= {"sempid",	"semzcnt", "semncnt",   "semval"};
	const int 	stat[]		= { GETPID,	GETZCNT,   GETNCNT,	GETVAL};
	char 		s[OUT_LINE];
	int 		i, j, k, sem, semval;
	struct sembuf	sop;
	struct semid_ds	semstat;
	union semun	semarg;

	if (argc == 1)
	{
		usage(argv[0]);
		EXIT(EXIT_FAILURE);
	}
	semarg.buf = &semstat;
	for (i = 1; i < argc; i++)
	{

		sem = ATOI(argv[i]);
		if (-1 == semctl(sem, 0, IPC_STAT, semarg))
		{
			FPRINTF(stderr, "Error obtaining semaphore status.\n");
			SNPRINTF(s, OUT_LINE, "semctl(%d)", sem);
			PERROR(s);
			continue;
		}
		PRINTF("semid %d: %hu semaphores in the set\n", sem, (unsigned short int)semarg.buf->sem_nsems);
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
				PRINTF("%s=%d%s", statname[k], semval, (k ? ", " : ")\n"));
			}
		}
	}
	EXIT(EXIT_SUCCESS);
}
