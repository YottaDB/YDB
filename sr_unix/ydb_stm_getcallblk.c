/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_semaphore.h"
#include <errno.h>

#include "libyottadb_int.h"
#include "mdq.h"

GBLREF	stm_freeq stmFreeQueue;

stm_que_ent *ydb_stm_getcallblk(void)
{
	int		status, save_errno;
	stm_que_ent	*callblk;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* First step for much of anything is to obtain the free queue lock */
	status = pthread_mutex_lock(&stmFreeQueue.mutex);
	if (0 != status)
	{
		SETUP_SYSCALL_ERROR("pthread_mutex_lock()", status);
		assert(FALSE);
		return (stm_que_ent *)-1;
	}
	/* See if there's anything on the free queue we can pull off */
	if ((void *)stmFreeQueue.stm_cbqhead.que.fl != (void *)&stmFreeQueue.stm_cbqhead)
	{	/* Something is there, record it and remove it from the queue */
		callblk = (stm_que_ent *)stmFreeQueue.stm_cbqhead.que.fl;
		dqdel(callblk, que);		/* Removes callblk from the queue */
	} else
	{	/* No free blocks available so create a new one */
		callblk = malloc(SIZEOF(stm_que_ent));
		memset(callblk, 0, SIZEOF(stm_que_ent));
		sem_init(&callblk->complete, 0, 1);	/* Create initially unlocked */
	}
	DEBUG_ONLY(callblk->mainqcaller = callblk->tpqcaller = NULL);
	/* Release our lock on the queue header */
	status = pthread_mutex_unlock(&stmFreeQueue.mutex);
	if (0 != status)
	{
		SETUP_SYSCALL_ERROR("pthread_mutex_unlock()", status);
		assert(FALSE);
		return (stm_que_ent *)-1;
	}
	/* The "complete" field in a callblk is not an actual lock. It is a wait mechanism that the
	 * thread putting an entry on the queue can then wait on this lock and wakeup when a worker
	 * thread "unlocks" it which it does when the task is complete. Our purpose here is just to
	 * make sure it is locked so it can be waited on. To do that, we do a sem_trywait() which
	 * always returns immediately. We always expect it to be unlocked but deal with it if it
	 * already was locked but assertfail in that case so we know we had a case of one being locked.
	 */
	GTM_SEM_TRYWAIT(&callblk->complete, status);	/* Semaphore may or may not be locked already */
	if ((0 != status) && (EAGAIN != errno))
	{
		save_errno = errno;
		SETUP_SYSCALL_ERROR("sem_trywait()", save_errno);
		assert(FALSE);
		return (stm_que_ent *)-1;
	}
	assert(EAGAIN != errno);			/* Find out if this ever happens */
	assert(callblk && (-1 != (intptr_t)callblk));
	return callblk;
}
