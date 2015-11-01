/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include "gtm_fcntl.h"

#include "do_semop.h"

/* perform one semop, returning errno if it was unsuccessful */
int do_semop(int sems, int num, int op, int flg)
{
	struct sembuf	sop;
	int		rv;

	sop.sem_num = num;
	sop.sem_op = op;
	sop.sem_flg = flg;
	while (-1 == (rv = semop(sems, &sop, 1)) && EINTR == errno)
		;

	if (-1 == rv)
		return errno;
	return 0;
}
