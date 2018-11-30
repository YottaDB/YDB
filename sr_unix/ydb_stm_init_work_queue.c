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

#include "libyottadb_int.h"
#include "mdq.h"

/* Routine to allocate and initialize a work block for a queue level and initialize it */
stm_workq *ydb_stm_init_work_queue(void)
{
	stm_workq		*wq;
	int			status;
	pthread_mutexattr_t	mattr;

	/* Note that there is no #undef of "malloc" in this module (like exists in "ydb_stm_getcallblk" and "ydb_stm_freecallblk")
	 * because this function is currently only invoked from the MAIN worker thread at all points in time (i.e. there are no
	 * concurrent threads that could potentially be invoking "malloc" at the same time).
	 */
	wq = malloc(SIZEOF(stm_workq));			/* Allocate the work queue descriptor block */
	memset((char *)wq, 0, SIZEOF(stm_workq));	/* Clear it */
	dqinit(&wq->stm_wqhead, que);			/* Initialize queue headers for request blocks for thread work */
	/* Initialize both the condition variable and the mutex. Since most of these calls never return errors, ignore their
	 * (always 0) return code.
	 */
	(void)pthread_cond_init(&wq->cond, NULL);	/* Initialize the queue's condition variable */
	INIT_STM_QUEUE_MUTEX(wq);			/* Initialize the queue's mutex */
	return wq;
}
