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
 * here so we use the real system version instead. Currently there is no "malloc"/"free" invoked in this function
 * but the #undef is done so we are safe even if a future "malloc"/"free" call gets added below.
 */
#undef malloc
#undef free

#include "gtm_stdlib.h"		/* Not needed now but will be when a "free" call is added below in the future */
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
