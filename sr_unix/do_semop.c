/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_sem.h"

#include "do_semop.h"
#include "gtm_c_stack_trace.h"
#include "gtm_c_stack_trace_semop.h"

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
	}
	while (-1 == (rv = semop(sems, sop, 1)) && ((EINTR == errno)))
		;
	return (-1 == rv) ? errno : 0;				/* return errno if not success */
}

/**
 * @brief Check for semaphore SEM_UNDO overflow
 *
 * If a semop() fails with an EINVAL, either the semaphore id was invalid OR the
 * operation asked for a SEM_UNDO, and the per-process SEM_UNDO table is full.
 *
 * This function disambiguates a semop which failed with an EINVAL by calling
 * semctl(IPC_STAT) to make sure the semaphore id was valid.  It should only
 * be called after such a failed semop() call.
 *
 * In general, we only expect to see SEM_UNDO overflows on AIX as the number
 * of available slots on AIX is pegged to 1024.  In actual practice, this
 * issue has only turned up in stress tests, but potentially users having
 * many open regions could encounter it.
 *
 * Our parameters mirror those passed to semop so we can check that SEM_UNDO was
 * specified, but we do not actually call semop(), but verify our semaphore id
 * with sem_ctl().
 *
 * @param semid Semaphore ID
 * @param sop Array of semaphore operations
 * @param sopcnt The number of entries in the array
 */
 boolean_t is_sem_undo_overflow(int semid, struct sembuf *sops, int sopcnt)
 {
	int i;
	int rv = -1;
	struct semid_ds ds;
	union semun arg;
	boolean_t have_undo = FALSE;

	/* If no operation specified SEM_UNDO, we immediately conclude we did
	 * not have a SEM_UNDO overflow
	 */
	for (i = 0; i < sopcnt; i++)
	{
		if (sop[i].sem_flg & SEM_UNDO)
			have_undo = TRUE;
	}
	if (!have_undo)
		return FALSE;

	/* We asked for a SEM_UNDO, so we could have overflow.
	 * If the semaphore id is OK, we do.
	 */
	arg.buf = &ds;
	while (-1 == (rv = semctl(semid, 0, IPC_STAT, arg)) && ((EINTR == errno)))
		;

	/* Looks like our semaphore id was invalid, so not SEM_UNDO overflow */
	if (-1 == rv)
	{
		assert(EINVAL == errno);
		return FALSE;
	}

	/* Good semaphore id, so must have been SEM_UNDO overflow */
	return TRUE;
 }
