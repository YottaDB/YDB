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

#include "libyottadb_int.h"
#include "mdq.h"

GBLREF	stm_freeq	stmFreeQueue;			/* Structure used to maintain free queue of stm_que_ent blocks */

int ydb_stm_freecallblk(stm_que_ent *callblk)
{
	int	status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Acquire the freeq lock so we can put this block on the free list */
	status = pthread_mutex_lock(&stmFreeQueue.mutex);
	if (0 != status)
	{
		SETUP_SYSCALL_ERROR("pthread_mutex_lock(stmFreeQueue)", status);
		return YDB_ERR_SYSCALL;
	}
	dqins(&stmFreeQueue.stm_cbqhead, que, callblk);
	status = pthread_mutex_unlock(&stmFreeQueue.mutex);
	if (0 != status)
	{
		SETUP_SYSCALL_ERROR("pthread_mutex_unlock(stmFreeQueue)", status);
		return YDB_ERR_SYSCALL;
	}
	return 0;
}
