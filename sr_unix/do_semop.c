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
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#ifdef __MVS__
#include <errno.h>
#else
#include <sys/errno.h>
#endif
#include "do_semop.h"

GBLREF int errno;

/* perform one semop, returning errno if it was unsuccessful */
int do_semop(int sems, int num, int op, int flg)
{
	struct sembuf	sop;
	int		rv;

	sop.sem_num = num;
	sop.sem_op = op;
	sop.sem_flg = flg;
	while ( (rv=semop(sems,&sop,1)) == -1 && errno == EINTR )
		;

	if (rv == -1)
	{	return errno;
	}
	return 0;
}
