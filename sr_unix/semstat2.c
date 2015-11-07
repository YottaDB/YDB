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
	int 		i, j;
	char 		s[81];
	int 		sem, semval, semncnt, semzcnt, sempid;
	struct	 	sembuf sop;
	struct semid_ds		semstat;
	union semun	semarg;

	if (argc == 1)
	{
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	semarg.buf = &semstat;
	for(i=1; i< argc; i++)
	{

		sem = ATOI(argv[i]);
		if ( semctl(sem, 0, IPC_STAT, semarg) == -1 )
		{
			FPRINTF(stderr, "Error obtaining semaphore status.\n");
			SPRINTF(s, "semctl(%d)", sem);
			PERROR(s);
			continue;
		}
		PRINTF("semid %d: %hu semaphores in the set\n", sem, (unsigned short int)semarg.buf->sem_nsems);
		for(j=0; j < semarg.buf->sem_nsems; j++)
		{

			if ( (semval = semctl(sem, j, GETVAL)) == -1 )
			{
				FPRINTF(stderr, "Error obtaining semaphore %d value.\n", j);
				SPRINTF(s, "semctl(%d)", sem);
				PERROR(s);
				continue;
			}
			PRINTF("sem %d: (semval=%d, ", j, semval);
			if ( (semncnt = semctl(sem, j, GETNCNT)) == -1 )
			{
				FPRINTF(stderr, "\nError obtaining semaphore %d ncnt.\n", j);
				SPRINTF(s, "semctl(%d)", sem);
				PERROR(s);
				continue;
			}
			PRINTF("semncnt=%d, ", semncnt);
			if ( (semzcnt = semctl(sem, j, GETZCNT)) == -1 )
			{
				FPRINTF(stderr, "\nError obtaining semaphore %d zcnt.\n", j);
				SPRINTF(s, "semctl(%d)", sem);
				PERROR(s);
				continue;
			}
			PRINTF("semzcnt=%d, ", semzcnt);
			if ( (sempid= semctl(sem, j, GETPID)) == -1 )
			{
				FPRINTF(stderr, "\nError obtaining semaphore %d PID.\n", j);
				SPRINTF(s, "semctl(%d)", sem);
				PERROR(s);
				continue;
			}
			PRINTF("sempid=%d)\n", sempid);
		}
	}
	exit(EXIT_SUCCESS);
}
