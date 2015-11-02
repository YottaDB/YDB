/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ipc.h"
#include <sys/sem.h>
#include <errno.h>
#include "gtm_fcntl.h"

#include "do_semop.h"
#include "gtm_c_stack_trace.h"

/* perform one semop, returning errno if it was unsuccessful */
int do_semop(int sems, int num, int op, int flg)
{
	static struct sembuf    sop[1];
	int			rv = -1;
	boolean_t		wait_option = !(flg & IPC_NOWAIT);
	sop[0].sem_num = num;
	sop[0].sem_op = op;
	sop[0].sem_flg = flg;
	TRY_SEMOP_GET_C_STACK(wait_option, sems, sop, 1, rv);
	if (-1 == rv)
		return errno;
	return 0;
}
