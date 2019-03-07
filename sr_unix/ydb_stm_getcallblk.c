/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

/* Note: We cannot use "ydb_malloc/ydb_free" here as this function can be called from any threads running
 * in the current process while the MAIN or TP worker thread could be concurrently running "ydb_malloc"
 * (which is a no-no). Therefore we undef "malloc"/"free" (which would have been defined by "sr_unix/mdefsp.h")
 * here so we use the real system version instead. Currently there is no "free" invoked in this function
 * but the #undef is done so we are safe even if a future "free" call gets added below.
 */
#undef malloc
#undef free

#include "gtm_stdlib.h"
#include "gtm_semaphore.h"
#include <errno.h>

#include "libyottadb_int.h"
#include "mdq.h"

GBLREF	stm_freeq		stmFreeQueue;

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
		SETUP_SYSCALL_ERROR("pthread_mutex_lock(&stmFreeQueue.mutex)", status);
		assert(FALSE);
		return (stm_que_ent *)-1;
	}
	/* See if there's anything on the free queue we can pull off */
	if ((void *)stmFreeQueue.stm_cbqhead.que.fl != (void *)&stmFreeQueue.stm_cbqhead)
	{	/* Something is there, record it and remove it from the queue */
		callblk = (stm_que_ent *)stmFreeQueue.stm_cbqhead.que.fl;
		dqdel(callblk, que);		/* Removes callblk from the queue */
	} else
	{	/* No free blocks available so create a new one. */
		callblk = malloc(SIZEOF(stm_que_ent));
		memset(callblk, 0, SIZEOF(stm_que_ent));
		GTM_SEM_INIT(&callblk->complete, 0, 0, status);		/* Create initially locked */
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
	 * always returns immediately. The expected status here is that the semaphore is already locked
	 * since it is both created that way and the last operation on a recycled block is that it
	 * was locked by a sem_wait in ydb_stm_thread(). But in debug, we make sure it is locked and
	 * generate an assert fail if it isn't.
	 */
#	ifdef DEBUG
	GTM_SEM_TRYWAIT(&callblk->complete, status);	/* Attempt to lock what should already be locked */
	assert((0 > status) && (EAGAIN == errno));
#	endif
	assert(callblk && (-1 != (intptr_t)callblk));
	return callblk;
}
