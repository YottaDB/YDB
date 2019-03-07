/****************************************************************
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries. *
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
GBLREF	sigset_t	block_sigsent;
GBLREF	boolean_t	blocksig_initialized;
GBLREF	uint4		simpleapi_dollar_trestart;
GBLREF	uint4		dollar_trestart;
GBLREF	boolean_t	forced_simplethreadapi_exit;
#ifdef DEBUG
GBLREF	uint64_t 	stmTPToken;			/* Counter used to generate unique token for SimpleThreadAPI TP */
#endif

STATICFNDCL void ydb_stm_tpthreadq_process(stm_workq *curTPWorkQHead, boolean_t *forced_simplethreadapi_exit_seen);

/* Routine to manage worker thread(s) for the *_st() interface routines (Simple Thread API aka the
 * Simple Thread Method). Note for the time being, only one worker thread is created. In the future,
 * if/when YottaDB becomes fully-threaded, more worker threads may be allowed.
 *
 * Note there is no parameter or return value from this routine currently (both passed as NULL). The
 * routine signature is dictated by this routine being driven by pthread_create().
 */
void *ydb_stm_tpthread(void *parm)
{
	int		tpdepth, status, rc;
	stm_workq	*curTPWorkQHead;
	boolean_t	forced_simplethreadapi_exit_seen = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TRCTBL_ENTRY(STAPITP_ENTRY, 0, "ydb_stm_tpthread", NULL, pthread_self());
	/* All of these TP worker threads have the set of signals that are "sent" disabled as it does not need them.
	 * This signal set is SIGINT, SIGQUIT, SIGTERM, SIGTSTP, SIGCONT, SIGALRM.
	 */
	assert(blocksig_initialized);
	/* Note we do not use SIGPROCMASK macro below because it would incorrectly invoke "sigprocmask" which is a no-no
	 * given this code is running inside a thread for sure (even though "multi_thread_in_use" global variable is not
	 * set to TRUE).
	 */
	rc = pthread_sigmask(SIG_BLOCK, &block_sigsent, NULL);	/* Note these signals are only blocked on THIS thread */
	assert(0 == rc);
	tpdepth = dollar_tlevel + 1;
	/* Initialize which queue we are looking for work in */
	assert(0 < tpdepth);
	assert(STMWORKQUEUEDIM > tpdepth);
	curTPWorkQHead = stmWorkQueue[tpdepth];	/* Initially pick requests from main work queue */
	assert(NULL != curTPWorkQHead);				/* Queue should be setup by now */
	/* Must have mutex locked before we start waiting */
	status = pthread_mutex_lock(&curTPWorkQHead->mutex);
	if (0 != status)
	{
		SETUP_SYSCALL_ERROR("pthread_mutex_lock(curTPWorkQHead)", status);
		assertpro(FALSE && YDB_ERR_SYSCALL);			/* No return possible so abandon thread */
	}
	for ( ; ; )
	{
		/* Before we wait, verify nobody snuck something onto the queue by processing anything there */
		ydb_stm_tpthreadq_process(curTPWorkQHead, &forced_simplethreadapi_exit_seen);	/* Process any entries on queue */
		if (forced_simplethreadapi_exit_seen)
		{	/* We saw "forced_simplethreadapi_exit" to be TRUE while inside "ydb_stm_tpthreadq_process".
			 * Just like we do in "ydb_stm_thread", exit. Note that here, we do not have multiple
			 * queues to worry about like there so the code is relatively simpler.
			 */
			assert(curTPWorkQHead->stm_wqhead.que.fl == &curTPWorkQHead->stm_wqhead);
			break;	/* No more requests queued up AND we have been signaled to exit. So exit. */
		}
		/* Wait for some work to probably show up */
		status = pthread_cond_wait(&curTPWorkQHead->cond, &curTPWorkQHead->mutex);
		if (0 != status)
		{
			SETUP_SYSCALL_ERROR("pthread_cond_wait(curTPWorkQHead)", status);
			assertpro(FALSE && YDB_ERR_SYSCALL);		/* No return possible so abandon thread */
		}
	}
	return NULL;
}

/* Routine to actually process the thread work queue for the Simple Thread API/Method. Note there are two possible queues we
 * would be looking at.
 */
STATICFNDEF void ydb_stm_tpthreadq_process(stm_workq *curTPWorkQHead, boolean_t *forced_simplethreadapi_exit_seen)
{
	stm_que_ent		*callblk;
	int			int_retval, rlbk_retval, status, save_errno;
	ydb_tp2fnptr_t		tpfn;
	void			*tpfnparm;
	boolean_t		nested_tp;
	libyottadb_routines	lydbrtn;
	uint64_t		tptoken;
	ydb_buffer_t		*errstr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TRCTBL_ENTRY(STAPITP_ENTRY, 0, "ydb_stm_tpthreadq_process", curTPWorkQHead, pthread_self());
	/* Loop to work till queue is empty */
	while (TRUE)
	{	/* If queue is empty, we should just go right back to sleep */
		if (curTPWorkQHead->stm_wqhead.que.fl == &curTPWorkQHead->stm_wqhead)
		{
			if (forced_simplethreadapi_exit)
			{	/* TP worker thread has been signaled to exit (i.e. "ydb_exit" has been called) */
				*forced_simplethreadapi_exit_seen = TRUE;
			}
			break;
		}
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
		assert(LYDB_RTN_TP == callblk->calltyp);
		for ( ; ;) /* for loop only there to let us break from various cases without having a deep if-then-else structure */
		{
			if (forced_simplethreadapi_exit)
			{	/* TP worker thread has been signaled to exit (i.e. "ydb_exit" has been called).
				 * Do not service any more SimpleThreadAPI requests. Return right away with appropriate error.
				 */
				callblk->retval = YDB_ERR_CALLINAFTERXIT;
				SET_STAPI_ERRSTR_MULTI_THREAD_SAFE(YDB_ERR_CALLINAFTERXIT, callblk->errstr);
				*forced_simplethreadapi_exit_seen = TRUE;
				break;
			}
			/* Note that all YottaDB engine calls are handled only by the MAIN worker thread and never
			 * by the current TP worker thread. The latter is used only to invoke the user-defined
			 * callback function. This lets the logic for handling timer pops stay in the MAIN worker thread
			 * keeping it simple (as opposed to introducing locks between the MAIN and TP worker threads
			 * in case the TP worker thread can also do YottaDB engine calls like "ydb_tp_s_common").
			 */
			nested_tp = (boolean_t)dollar_tlevel;
			tptoken = callblk->tptoken;
			assert(tptoken == USER_VISIBLE_TPTOKEN(dollar_tlevel, stmTPToken));
			assert(YDB_NOTTP != tptoken);
			errstr = callblk->errstr;
			/*
			 * callblk->args[0] = tpfn      parameter in "ydb_tp_s_common"
			 * callblk->args[1] = tpfnparm  parameter in "ydb_tp_s_common"
			 * callblk->args[2] = transid   parameter in "ydb_tp_s_common"
			 * callblk->args[3] = namecount parameter in "ydb_tp_s_common"
			 * callblk->args[4] = varnames  parameter in "ydb_tp_s_common"
			 *
			 * For the LYDB_RTN_TP_START or LYDB_RTN_TP_START_TLVL0 case, we do not invoke the user-defined
			 *	callback function so do not need to pass tpfn or tpfnparm.
			 * For the LYDB_RTN_TP_RESTART/LYDB_RTN_TP_COMMIT/LYDB_RTN_TP_RESTART_TLVL0/LYDB_RTN_TP_COMMIT_TLVL0
			 *	cases, we do not need ANY of the above 5 parameters.
			 * The ydb_stm_args* calls done below take that into account.
			 * Once the "op_tstart" is done above, we do not need "transid", "namecount" and "varnames"
			 *	parameters for the later calls. So we use ydb_stm_args0 below.
			 * Also, after each "ydb_stm_args3" or "ydb_stm_args0" call ensure a LIBYOTTADB_DONE
			 *	was done by the corresponding "ydb_tp_s_common" call in the MAIN worker thread
			 *	in all cases by asserting that TREF(libyottadb_active_rtn) is LYDB_RTN_NONE.
			 */
			assert(LYDB_RTN_NONE == TREF(libyottadb_active_rtn));
			/* Start the TP transaction by asking the MAIN worker thread to do the "op_tstart"
			 * (in "ydb_tp_s_common").
			 */
			lydbrtn = (!nested_tp ? LYDB_RTN_TP_START_TLVL0 : LYDB_RTN_TP_START);
			int_retval = ydb_stm_args3(tptoken, errstr, lydbrtn,
							callblk->args[2], callblk->args[3], callblk->args[4]);
			assert(LYDB_RTN_NONE == TREF(libyottadb_active_rtn));
			assert(YDB_TP_RESTART != int_retval);
			if (YDB_OK != int_retval)
			{
				if (!nested_tp && dollar_tlevel)
				{	/* If outermost TP had an error, rollback any TP that might have been created
					 * (we do not expect any since dollar_tlevel++ is done only after all error code
					 * paths have been checked in "op_tstart") before returning error.
					 */
					assert(FALSE);
					rlbk_retval = ydb_stm_args0(tptoken, errstr, LYDB_RTN_TP_ROLLBACK_TLVL0);
					assert(LYDB_RTN_NONE == TREF(libyottadb_active_rtn));
					assert(YDB_TP_ROLLBACK == rlbk_retval);
					/* Note: "int_retval" records the "op_tstart" error code while
					 *       "rlbk_retval" holds the "op_trollback" error code (we do not expect any).
					 * Use the "op_tstart" error code (the first error) in the call block.
					 */
				}
				callblk->retval = (uintptr_t)int_retval;
				break;
			}
			tpfn = (ydb_basicfnptr_t)callblk->args[0];
			tpfnparm = (void *)callblk->args[1];
			for ( ; ; )
			{	/* Loop to handle TP restarts */
				if (!nested_tp)
				{	/* Maintain "simpleapi_dollar_trestart" just like "ydb_tp_s_common"
					 * does for the "LYDB_RTN_TP" case.
					 */
					simpleapi_dollar_trestart = dollar_trestart;
				}
				/* Invoke the user-defined TP callback function in the TP worker thread (current thread) */
				int_retval = (*tpfn)(tptoken, errstr, tpfnparm);
				if (YDB_OK == int_retval)
				{	/* Commit the TP transaction by asking MAIN worker thread to do the "op_tcommit"
					 * (in "ydb_tp_s_common").
					 */
					lydbrtn = (!nested_tp ? LYDB_RTN_TP_COMMIT_TLVL0 : LYDB_RTN_TP_COMMIT);
					int_retval = ydb_stm_args0(tptoken, errstr, lydbrtn);
				}
				assert(LYDB_RTN_NONE == TREF(libyottadb_active_rtn));
				if (nested_tp)
				{	/* If nested TP, return success/error code directly back to caller of "ydb_tp_st" */
					break;
				}
				/* If we reach here, it means we are in the outermost TP */
				if (YDB_OK == int_retval)
				{	/* Outermost TP committed successfully. Return success to caller of "ydb_tp_st". */
					break;
				}
				if (YDB_TP_RESTART != int_retval)
				{	/* Outermost TP and error code is not a TPRESTART.
					 * Return it directly to caller of "ydb_tp_st" but before that roll back the
					 *	TP transaction (if not already done).
					 * ROLLBACK the TP transaction by asking MAIN worker thread to do the
					 *	"op_trollback" (in "ydb_tp_s_common").
					 * Note that it is possible "int_retval" is YDB_TP_ROLLBACK (e.g. if the callback
					 *	function returned YDB_TP_ROLLBACK).
					 */
					if (dollar_tlevel)
					{	/* Rollback could have happened in some cases already (for example
						 * if "ydb_tp_s_common(LYDB_RTN_TP_COMMIT)" invocation failed with a GBLOFLOW
						 * error). We do not want to invoke a rollback again in that case since
						 * the tptoken no longer matches the current "dollar_tlevel" (would return with
						 * a YDB_ERR_INVTPTRANS error). Hence the "if (dollar_tlevel)" check above.
						 */
						rlbk_retval = ydb_stm_args0(tptoken, errstr, LYDB_RTN_TP_ROLLBACK_TLVL0);
						/* Note that it is possible the above returns a YDB_ERR_CALLINAFTERXIT error
						 * in case the MAIN/TP worker thread(s) have been asked to terminate.
						 * Take that into account in the below assert.
						 * In this case, the TP transaction would not be rolled back yet (so dollar_tlevel
						 * would still be non-zero) but it is okay to return in this case with this
						 * error code since the MAIN worker thread will run the exit handler which will
						 * do the needed "op_trollback".
						 */
						assert((YDB_TP_ROLLBACK == rlbk_retval)
							|| (YDB_ERR_CALLINAFTERXIT == rlbk_retval)
								&& (int_retval == rlbk_retval) && dollar_tlevel);
					}
					assert(LYDB_RTN_NONE == TREF(libyottadb_active_rtn));
					/* Note: "int_retval" records the primary error code while
					 *       "rlbk_retval" holds the "op_trollback" error code (we do not expect any
					 *			except YDB_ERR_CALLINAFTERXIT in which case we expect int_retval
					 *			and rlbk_retval to be the same as asserted above).
					 * Use the "op_tstart" error code (the first error) in the call block.
					 */
					break;
				}
				/* Restart the outermost TP transaction by asking the MAIN worker thread
				 * to do the "tp_restart" (in "ydb_tp_s_common").
				 */
				int_retval = ydb_stm_args0(tptoken, errstr, LYDB_RTN_TP_RESTART_TLVL0);
				assert(LYDB_RTN_NONE == TREF(libyottadb_active_rtn));
				if (YDB_ERR_CALLINAFTERXIT == int_retval)
					break;	/* The MAIN/TP worker thread(s) have been asked to exit. Return right away. */
				assert(YDB_OK == int_retval);
			}
			callblk->retval = (uintptr_t)int_retval;
			break;
		}
		if (!*forced_simplethreadapi_exit_seen)
		{	/* Signal the MAIN worker thread that this TP is done */
			status = ydb_stm_args0(tptoken, errstr, LYDB_RTN_TPCOMPLT);
			if (0 != status)
			{
				assert(YDB_ERR_CALLINAFTERXIT == status);
				callblk->retval = status;
			}
		}
		/* else: No need to signal end of TP transaction to MAIN worker thread. It would have also seen
		 *       "forced_simplethreadapi_exit" and would be cleaning up its queues as appropriate.
		 */
		/* The request is complete - regrab the lock to check if any more entries on this queue (not so much
		 * possible now as in the future when/if the codebase becomes fully threaded.
		 */
		TRCTBL_ENTRY(STAPITP_LOCKWORKQ, FALSE, curTPWorkQHead, NULL, pthread_self());
		status = pthread_mutex_lock(&curTPWorkQHead->mutex);
		if (0 != status)
		{
			SETUP_SYSCALL_ERROR("pthread_mutex_lock()", status);
			assertpro(FALSE && YDB_ERR_SYSCALL);		/* No return possible so abandon thread */
		}
		/* Signal to process that we are done with this request */
		TRCTBL_ENTRY(STAPITP_SIGCOND, 0, NULL, callblk, pthread_self());
		GTM_SEM_POST(&callblk->complete, status);
		if (0 != status)
		{
			save_errno = errno;
			SETUP_SYSCALL_ERROR("sem_post()", save_errno);
			assert(FALSE);
			callblk->retval = (uintptr_t)YDB_ERR_SYSCALL;
			/* No return here - just keep going if at all possible */
		}
	}
}
