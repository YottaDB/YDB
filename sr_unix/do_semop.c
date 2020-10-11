/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> e9a1c121 (GT.M V6.3-014)
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include <sys/sem.h>
#include "gtm_stdlib.h"
#include "gtm_ipc.h"
#include "gtm_fcntl.h"

#include "do_semop.h"
#include "gtm_c_stack_trace.h"
#include "gtm_c_stack_trace_semop.h"
#include "eintr_wrappers.h"

static struct sembuf    sop[1];

/* perform one semop, returning errno if it was unsuccessful */
/* maintain in parallel with eintr_wrapper_semop.h */
int do_semop(int sems, int num, int op, int flg)
{
	boolean_t		wait_option;
	int			rv = -1;

	wait_option = ((!(flg & IPC_NOWAIT)) && (0 == op));
	sop[0].sem_num = num;
	sop[0].sem_op = op;
	sop[0].sem_flg = flg;
	CHECK_SEMVAL_GRT_SEMOP(sems, num, op);
	if (wait_option)
	{
		rv = try_semop_get_c_stack(sems, sop, 1);	/* try with patience and possible stack trace of blocker */
		return rv;
<<<<<<< HEAD
	} else
	{
		while (-1 == (rv = semop(sems, sop, 1)) && EINTR == errno)
			eintr_handling_check();
		/* Note: Both "eintr_handling_check()" and "HANDLE_EINTR_OUTSIDE_SYSTEM_CALL" ensure "errno"
		 * is untouched so it is safe to use "errno" after the call to examine the pre-call value.
		 */
		HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
		return (-1 == rv) ? errno : 0;
=======
>>>>>>> e9a1c121 (GT.M V6.3-014)
	}
	while (-1 == (rv = semop(sems, sop, 1)) && ((EINTR == errno)))
		;
	return (-1 == rv) ? errno : 0;				/* return errno if not success */
}
