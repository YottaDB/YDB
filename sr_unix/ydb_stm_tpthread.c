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
#include "gtm_signal.h"
#include <errno.h>

#include "libyottadb_int.h"
#include "mdq.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab_int4.h"     /* needed for tp.h */
#include "buddy_list.h"
#include "tp.h"
#include "trace_table.h"

GBLREF	stm_workq	*stmWorkQueue[];
GBLREF	uintptr_t	stmTPToken;			/* Counter used to generate unique token for SimpleThreadAPI TP */
GBLREF	sigset_t	block_sigsent;
GBLREF	boolean_t	blocksig_initialized;

STATICFNDCL void ydb_stm_tpthreadq_process(stm_workq *curTPWorkQHead);


/* Routine to manage worker thread(s) for the *_st() interface routines (Simple Thread API aka the
 * Simple Thread Method). Note for the time being, only one worker thread is created. In the future,
 * if/when YottaDB becomes fully-threaded, more worker threads may be allowed.
 *
 * Note there is no parameter or return value from this routine currently (both passed as NULL). The
 * routine signature is dictated by this routine being driven by pthread_create().
 */
void *ydb_stm_tpthread(void *parm)
{
	int		status, rc;
	boolean_t	stop = FALSE;
	stm_workq	*curTPWorkQHead;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TRCTBL_ENTRY(STAPITP_ENTRY, 0, "ydb_stm_tpthread", NULL, pthread_self());
	/* All of these TP worker threads have the set of signals that are "sent" disabled as it does not need them.
	 * This signal set is SIGINT, SIGQUIT, SIGTERM, SIGTSTP, SIGCONT, SIGALRM.
	 */
	assert(blocksig_initialized);
	SIGPROCMASK(SIG_BLOCK, &block_sigsent, NULL, rc);	/* Note these signals are only blocked on THIS thread */
	/* Initialize which queue we are looking for work in */
	assert(0 < TREF(curWorkQHeadIndx));
	curTPWorkQHead = stmWorkQueue[TREF(curWorkQHeadIndx)];	/* Initially pick requests from main work queue */
	assert(NULL != curTPWorkQHead);				/* Queue should be setup by now */
	/* Must have mutex locked before we start waiting */
	status = pthread_mutex_lock(&curTPWorkQHead->mutex);
	if (0 != status)
	{
		SETUP_SYSCALL_ERROR("pthread_mutex_lock(curWorkQHead)", status);
		assertpro(YDB_ERR_SYSCALL);			/* No return possible so abandon thread */
	}
	/* Before we wait the first time, veryify nobody snuck something onto the queue by processing anything there */
	ydb_stm_tpthreadq_process(curTPWorkQHead);
	while(!stop)
	{	/* Wait for some work to probably show up */
		status = pthread_cond_wait(&curTPWorkQHead->cond, &curTPWorkQHead->mutex);
		if (0 != status)
		{
			SETUP_SYSCALL_ERROR("pthread_cond_wait(curWorkQHead)", status);
			assertpro(YDB_ERR_SYSCALL);		/* No return possible so abandon thread */
		}
		ydb_stm_tpthreadq_process(curTPWorkQHead);	/* Process any entries on the queue */
	}
	return NULL;
}

/* Routine to actually process the thread work queue for the Simple Thread API/Method. Note there are two possible queues we
 * would be looking at.
 */
STATICFNDEF void ydb_stm_tpthreadq_process(stm_workq *curTPWorkQHead)
{
	stm_que_ent	*callblk;
	int		int_retval, status, save_errno;
	unsigned int	tptoken;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TRCTBL_ENTRY(STAPITP_ENTRY, 0, "ydb_stm_tpthreadq_process", curTPWorkQHead, pthread_self());
	/* Loop to work till queue is empty */
	while(TRUE)
	{	/* If queue is empty, we should just go right back to sleep */
		if (curTPWorkQHead->stm_wqhead.que.fl == &curTPWorkQHead->stm_wqhead)
			break;
		/* Remove the first element (going forward) from the work queue */
		callblk = curTPWorkQHead->stm_wqhead.que.fl;
		dqdel(callblk, que);		/* Removes our element from the queue */
		/* We don't want to hold the lock during our processing so release it now */
		status = pthread_mutex_unlock(&curTPWorkQHead->mutex);
		if (0 != status)
		{
			SETUP_SYSCALL_ERROR("pthread_mutex_unlock()", status);
			callblk->retval = (uintptr_t)YDB_ERR_SYSCALL;
			break;
		}
		TRCTBL_ENTRY(STAPITP_UNLOCKWORKQ, 0, curTPWorkQHead, callblk, pthread_self());
		TRCTBL_ENTRY(STAPITP_FUNCDISPATCH, callblk->calltyp, NULL, NULL, pthread_self());
		/* We have our request - dispatch it appropriately (currently only one choice) */
		switch(callblk->calltyp)
		{
			case LYDB_RTN_TP:
				/* Driving new TP level requires a new tptoken. Create one by incrementing our counter. Note
				 * that the atomic part of the increment is probably not necessary at this point but will be
				 * when multiple work queues exist after YottaDB is fully threaded.
				 */
				tptoken = INTERLOCK_ADD(&stmTPToken, UNUSED, 1);
				stmWorkQueue[TREF(curWorkQHeadIndx)]->tptoken = tptoken;
				int_retval = ydb_tp_s_common(TRUE, tptoken, (ydb_basicfnptr_t)callblk->args[0],
							     (void *)callblk->args[1], (const char *)callblk->args[2],
							     (int)callblk->args[3], (ydb_buffer_t *)callblk->args[4]);
				callblk->retval = (uintptr_t)int_retval;
				break;
			default:
				assertpro(FALSE);
		}
		/* If this is the first TP level finishing up, we need to put a task on the TP work queue of
		 * the main worker thread that causes it to switch the queues back to the main work queue so
		 * do that now before we signal this task as complete.
		 */
		if (1 == TREF(curWorkQHeadIndx))
		{
			status = ydb_stm_args0(tptoken, LYDB_RTN_TPCOMPLT);
			if (0 != status)
				callblk->retval = status;
		} else
			(TREF(curWorkQHeadIndx))--;	/* Reduce TP level */
		/* The request is complete - regrab the lock to check if any more entries on this queue (not so much
		 * possible now as in the future when/if the codebase becomes fully threaded.
		 */
		TRCTBL_ENTRY(STAPITP_LOCKWORKQ, FALSE, curTPWorkQHead, NULL, pthread_self());
		status = pthread_mutex_lock(&curTPWorkQHead->mutex);
		if (0 != status)
		{
			SETUP_SYSCALL_ERROR("pthread_mutex_lock()", status);
			assertpro(YDB_ERR_SYSCALL);		/* No return possible so abandon thread */
		}
		/* Signal to process that we are done with this request */
		TRCTBL_ENTRY(STAPITP_SIGCOND, 0, NULL, callblk, pthread_self());
		GTM_SEM_POST(&callblk->complete, status);
		if (0 != status)
		{
			save_errno = errno;
			SETUP_SYSCALL_ERROR("sem_post()", save_errno);
			callblk->retval = (uintptr_t)YDB_ERR_SYSCALL;
			/* No return here - just keep going if at all possible */
		}
	}
}
