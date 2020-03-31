/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

/* Note, much of the code in this module runs WITHOUT the system lock so we cannot use the internal version of malloc/free so
 * we undefine those verisons to use the system versions in this module. Do this before including stdlib.h so the defines of those
 * routines do not get the macro substitutions.
 */
#undef malloc
#undef free
#include "gtm_signal.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"

#include "gtmio.h"
#include "libyottadb_int.h"
#include "generic_signal_handler.h"
#include "mdq.h"
#include "compswap.h"

GBLREF	void			*ydb_sig_handlers[];
GBLREF	sig_pending		sigPendingFreeQue;	/* Queue of free pending signal blocks - should be a short queue */
GBLREF	sig_pending		sigPendingQue;		/* Queue of pending signals handled in alternate signal handling mode */
GBLREF	sig_pending		sigInProgressQue;	/* Anchor for queue of signals being processed */
GBLREF	pthread_mutex_t		sigPendingMutex;	/* Used to control access to sigPendingQue */
GBLREF	pthread_t		gtm_main_thread_id;
GBLREF	boolean_t		gtm_main_thread_id_set;
GBLREF	boolean_t		exit_handler_active;	/* Exit handling has been completed - nothing else should run */
GBLREF  sigset_t		block_sigsent;
GBLREF	boolean_t		blocksig_initialized;
GBLREF	uint4			dollar_tlevel;
GBLREF	pthread_t		ydb_engine_threadsafe_mutex_holder[];

/* If the return code supplied is non-zero, fetch the appropriate error message and set it into the supplied ydb_buffer_t */
#define SET_ERRSTR_AND_RETURN_IF_NECESSARY(ERRNUM, ERRSTR)		\
{									\
	if (0 != ERRNUM)			   			\
	{								\
		SET_STAPI_ERRSTR_MULTI_THREAD_SAFE(ERRNUM, ERRSTR);	\
		return ERRNUM;						\
	}								\
}

/* Macro to drain a queue of sig_pending blocks and do posts on their embedded msems */
#define DRAIN_SPQUE_AND_POST(QUEUEHDR, ELEM)									\
{	/* Drain signals on this queue - release their waiters. Note, we don't care much about return codes	\
	 * at this point - it either works or it doesn't.							\
	 */													\
	sig_pending	*sigpblk;										\
														\
	while (SPQUE_NOT_EMPTY((QUEUEHDR), ELEM))								\
	{													\
		sigpblk = (QUEUEHDR)->ELEM.fl;			/* Remove element from queue */			\
		DQDEL(sigpblk,ELEM);										\
		sigpblk->retCode = YDB_ERR_CALLINAFTERXIT;							\
		sem_post(&sigpblk->sigHandled);			/* Let handler finish what it was doing */	\
	}													\
}

/* Lock given mutex taking care to make the mutex consistent if the previous holder had died. There is a small window where
 * elements are being removed from or added to the queue where this will cause the process to fail when we just make the
 * mutex consistent and keep going but the alternative is to always fail the process if a thread goes away. This lock is
 * only used by the threads in a single process - not across processes.
 */
static inline int get_sigpend_lock(pthread_mutex_t *sigpendMutex)
{
	int	rc;

	rc = pthread_mutex_lock(sigpendMutex);
	if (EOWNERDEAD == rc)
		/* Holder of lock died - recover by making consistent */
		rc = pthread_mutex_consistent(sigpendMutex);
	DBGSIGHND_ONLY(fprintf(stderr, "get_sigpend_lock: Acquired lock 0x"lvaddr"\n", (long long unsigned int)sigpendMutex);
		       fflush(stderr));
	return rc;
}

/* Release the lock created by get_sigpend_lock() */
static inline int rel_sigpend_lock(pthread_mutex_t *sigpendMutex)
{
	int	rc;

	DBGSIGHND_ONLY(fprintf(stderr, "rel_sigpend_lock: Releasing lock 0x"lvaddr"\n", (long long unsigned int)sigpendMutex);
		       fflush(stderr));
	rc = pthread_mutex_unlock(sigpendMutex);
	assert(0 == rc);
	return rc;
}

/* Routine to pull an unused pending signal descriptor block off of the free queue or, if none available, allocate one and
 * return it. Initialize the block before returning. The blocks contain a memory semaphore to be used to notify the
 * thread that posted the block to the work queue when the signal is processed. Originally wanted this to be a pthread_mutex
 * instead but since this lock is initially locked by one thread and freed by a different one, that was not allowed so switched
 * to a memory semaphore instead which does allow this.
 */
static inline sig_pending *get_sigpend_blk(int *retcode)
{
	int		rc;
	sig_pending	*sigpblk;

	sigpblk = NULL;
	/* If we have a cached blk, we can use that but need to make sure it is unlocked as it may have been previously
	 * used (and locked) by another thread. If we have to create a new one, initialize it.
	 */
	if (SPQUE_IS_EMPTY(&sigPendingFreeQue, que))
	{	/* Create and initialize a new block */
		sigpblk = (sig_pending *)malloc(SIZEOF(sig_pending));
		rc = sem_init(&sigpblk->sigHandled, 0, 0);		/* Init msem shared b'tween threads initially locked */
		assertpro(0 == rc);					/* Should be impossible to fail */
	} else
	{
		sigpblk = sigPendingFreeQue.que.fl;			/* De-queue first element */
		DQDEL(sigpblk, que);
		rc = 0;
	}
	*retcode = rc;							/* Set actual return code for reporting */
	return (0 == *retcode) ? sigpblk : NULL;
}

/* Routine to run-down the signal pending queues - drain both queues with waiters and post their internal msems allowing the
 * signal handling to complete and regular routines to resume. This also has the effect of allowing the Go routines that are
 * sending us the signals to return and note that they need to shutdown cleanly.
 */
int drain_signal_queues(ydb_buffer_t *errstr)
{
	int		rc;

	assert(blocksig_initialized);
	rc = get_sigpend_lock(&sigPendingMutex);
	SET_ERRSTR_AND_RETURN_IF_NECESSARY(rc, errstr);
	DRAIN_SPQUE_AND_POST(&sigPendingQue, que);
	DRAIN_SPQUE_AND_POST(&sigInProgressQue, que);
	rel_sigpend_lock(&sigPendingMutex);
	/* Now this signal request goes back immediately also */
	SET_ERRSTR_AND_RETURN_IF_NECESSARY(YDB_ERR_CALLINAFTERXIT, errstr);	/* This will always return */
	assert(FALSE);
	return 0;		/* Should never get here but need for compiler */
}

/* Routine driven from a language wrapper (e.g. Go wrapper) when it is desirable for the main routine language to do the signal
 * handling then notify YottaDB that those signals have occurred. This is the call back into YottaDB to dispatch those signals
 * to the handler defined to deal with them (if any). The input errstr is filled in with any error that occurs.
 *
 * Note this routine runs WITHOUT the YDB engine lock so extreme care must be taken not to use engine routines that could
 * compromise YDB's non-threaded integrity.
 */
int ydb_sig_dispatch(ydb_buffer_t *errstr, int signum)
{
	int		rc, sigrc, oldCurSigChkIndx, newCurSigChkIndx;
	sig_pending	*sigpblk;
	pthread_t	sigThreadId;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(USING_ALTERNATE_SIGHANDLING);
	if (exit_handler_active)
	{	/* No signal processing after exit handler starts running - drain signal pending and processing queues */
		rc = drain_signal_queues(errstr);
		if (0 != rc)
			return rc;
		assert(FALSE);		/* drain_signal_queues() ALWAYS returns non-0 rc */
	}
	LIBYOTTADB_RUNTIME_CHECK((int), errstr);	/* Note macro may return from this routine if error */
	VERIFY_THREADED_API((int), errstr);		/* Note macro may return from this routine if error */
	/* Note, any stream IO outside the system lock needs to be done normally or it causes failures */
	DBGSIGHND_ONLY(fprintf(stderr, "ydb_sig_dispatch: Received dispatch request for signal %d\n", signum); fflush(stderr));
	assert((NSIG >= signum) && (0 < signum));
	if (NULL != ydb_sig_handlers[signum])
	{	/* Handler found for this signal */
		DBGSIGHND_ONLY(fprintf(stderr, "ydb_sig_dispatch: Handler found for signal %d - queueing signal to be processed\n",
				       signum); fflush(stderr));
		/* These routines run largely outside of the YDB engine lock and even outside real signal handlers since Go only
		 * "notified" us of signals but handles them itself. But to prevent other threads from interrupting us with a notify
		 * signal, block the commonly sent signals for the duration of this lock.
		 */
		assert(blocksig_initialized);
		rc = get_sigpend_lock(&sigPendingMutex);
		SET_ERRSTR_AND_RETURN_IF_NECESSARY(rc, errstr);
		sigpblk = get_sigpend_blk(&rc);		/* Get a pending signal block and fill it in */
		SET_ERRSTR_AND_RETURN_IF_NECESSARY(rc, errstr);
		assert(NULL != sigpblk);
		/* Fill in rest of block before we queue it */
		sigpblk->sigHandler = ydb_sig_handlers[signum];
		sigpblk->sigNum = signum;
		sigpblk->retCode = 0;
		sigpblk->posted = FALSE;
		DQRINS(&sigPendingQue, que, sigpblk);	/* Queue FIFO */
		rc = rel_sigpend_lock(&sigPendingMutex);
		SET_ERRSTR_AND_RETURN_IF_NECESSARY(rc, errstr);
		/* Tell signal thread (aka main thread aka ydb_stm_thread()) that a signal needs processing */
		sigThreadId = gtm_main_thread_id;
		if (gtm_main_thread_id_set)	/* Take care to not send signal if signal thread has already terminated */
			pthread_kill(sigThreadId, YDBSIGNOTIFY);	/* .. as that can cause a SIGSEGV */
		DBGSIGHND_ONLY(fprintf(stderr, "ydb_sig_dispatch: Waiting on signal to be handled\n"); fflush(stderr));
		/* Note, because this code is only used when the main is Go and because we know that Go uses SA_RESTART
		 * when defining all their condition handlers, we do not need to deal with EINTR return codes out of
		 * sem_wait() here (since Linux 2.6.22). See "man 7 signal" on Linux.
		 */
		rc = sem_wait(&sigpblk->sigHandled);		/* Acquire this lock - when we get it, signal is processed */
		SET_ERRSTR_AND_RETURN_IF_NECESSARY(rc, errstr);
		/* Fetch how the signal fared in its processing, then requeue the block */
		sigrc = sigpblk->retCode;
		if (!exit_handler_active)
		{	/* If not exiting yet, put on free queue for re-use */
			rc = get_sigpend_lock(&sigPendingMutex);
			SET_ERRSTR_AND_RETURN_IF_NECESSARY(rc, errstr);
			DQINS(&sigPendingFreeQue, que, sigpblk);	/* Put on free queue for reuse */
			assert(SPQUE_NOT_EMPTY(&sigPendingFreeQue, que));
			rc = rel_sigpend_lock(&sigPendingMutex);
			SET_ERRSTR_AND_RETURN_IF_NECESSARY(rc, errstr);
		} else
			free(sigpblk);				/* Otherwise just release it as we don't need it anymore */
		/* Now see how our signal fared and return its error if needbe */
		DBGSIGHND_ONLY(fprintf(stderr, "ydb_sig_dispatch: Handling complete for signal %d (rc=%d)\n", signum, sigrc);
			       fflush(stderr));
		SET_ERRSTR_AND_RETURN_IF_NECESSARY(sigrc, errstr);
	} else
	{
		DBGSIGHND_ONLY(fprintf(stderr, "ydb_sig_dispatch: No handler found - ignoring\n"); fflush(stderr));
	}
	return YDB_OK;
}

/* Routine that is only driven when we have the YDB engine lock. It processes any pending signals on the alternate signal queue
 * and posts to their embedded memory semaphore when that processing is complete. We use memory semaphores here because we
 * needed to be able to lock the semaphore in one thread and post it in another - basically using it as a waiting mechanism
 * instead of as a lock. While this was possible with the old "Linux Threads" thread model (now replaced), the newer more
 * POSIX compliant threading model does not allow one thread to unlock a pthread_mutex_t locked in a different thread.
 *
 * Note the signal handling debugging does not use DBGSIGHND in this case since this routine called by gtm_fwrite() and causes an
 * eternal loop - at least until the stack is consumed anyway.
 */
void process_pending_signals(void)
{
	int		rc;
	sig_pending	*sigpblk;

	if (exit_handler_active)
		return;			/* Engine being shutdown - ignore async signals */
	DBGSIGHND_ONLY(fprintf(stderr, "process_pending_signals: Starting alternate pending signal processing\n"); fflush(stderr));
	/* This assert checks if either we are in TP OR we hold the engine lock. We cannot easily check the engine lock when
	 * a process is in TP because we don't have the current TP token (contains lock index) so half a check is better than none.
	 */
	assert((0 < dollar_tlevel) || (pthread_equal(ydb_engine_threadsafe_mutex_holder[0], pthread_self())));
	do
	{	/* First fetch an element off the pending queue under lock */
		assert(blocksig_initialized);
		rc = get_sigpend_lock(&sigPendingMutex);
		/* Nobody to report error back to so ignore it at this point */
		if (0 != rc)
		{
			assert(FALSE);
			break;
		}
		/* Remove element describing a pending signal from the queue */
		if (SPQUE_IS_EMPTY(&sigPendingQue, que))
		{	/* Something else took care of it - we have nothing to do */
			rel_sigpend_lock(&sigPendingMutex);
			break;
		}
		/* Get a signal off the pending queue */
		sigpblk = sigPendingQue.que.fl;
		DQDEL(sigpblk, que);
		/* Place element on the in-progress queue and release the lock */
		DQINS(&sigInProgressQue, que, sigpblk);
		assert(SPQUE_NOT_EMPTY(&sigInProgressQue, que));
		rel_sigpend_lock(&sigPendingMutex);
		DBGSIGHND_ONLY(fprintf(stderr, "process_pending_signals: Processing pending signal %d\n", sigpblk->sigNum);
			       fflush(stderr));
		/* Drive the signal processor with the signal number as an argument saving the return code in the block */
		sigpblk->retCode = (*sigpblk->sigHandler)(sigpblk->sigNum);
		/* Signal has now been processed - get the lock again, verify it has not already been posted, and post it if not,
		 * and remove from in-process queue. It is possible it got posted as part of exit-handling rundown and this
		 * thread just hasn't died yet but it best to do avoid double posting attempts.
		 */
		rc = get_sigpend_lock(&sigPendingMutex);
		/* Nobody to report error back to so ignore it at this point */
		if (0 != rc)
		{
			assert(FALSE);
			break;
		}
		/* When exit handling runs, it posts all pending/in-process signals and marks them posted so they can shutdown.
		 * If this has not already been done to this request, post the msem in the block to signal completion to the
		 * waiting goroutine.
		 */
		if (!sigpblk->posted)
		{
			rc = sem_post(&sigpblk->sigHandled);
			assert(0 == rc);
			DQDEL(sigpblk, que);			/* Remove from in-progress queue */
			sigpblk->posted = TRUE;
		}
		rel_sigpend_lock(&sigPendingMutex);
	} while (SPQUE_NOT_EMPTY(&sigPendingQue, que) && !exit_handler_active);
}
