/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2026 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsbgtr.h"
#include "mutex.h"
#include "relqueopi.h"
#include "relqop.h"
#include "is_proc_alive.h"
#include "gtmsecshr.h"
#include "gtm_rel_quant.h"
#include "mutex_deadlock_check.h"
#include "gtmio.h"
#include "gtm_c_stack_trace.h"
#include "anticipatory_freeze.h"
#ifdef DEBUG
#include "wbox_test_init.h"
#endif

#define	CAT_CHECK_FREQUENCY		1024	/* Check if mutex type needs to be switched every 1024 crit successes */
#define	PTHREAD_MUTEX_SWITCH_STREAK	16	/* Do the switch only if the condition is satisfied for 16 consecutive times */
#define	DFT_DAT_RATIO_THRESHOLD		0.60	/* If change in CFT (called DFT) is greater than 60% of the change in
						 * CAT (called DAT), we will switch from pthread to ydb mutex.
						 */

#ifdef DEBUG_YDB_MUTEX
GBLDEF	char	*ydb_mutex_benchmark;

#	define	DEBUG_GET_BENCHMARK_ENV						\
	{									\
		if (NULL == ydb_mutex_benchmark)				\
		{								\
			ydb_mutex_benchmark = getenv("ydb_mutex_benchmark");	\
			if (NULL == ydb_mutex_benchmark)			\
				ydb_mutex_benchmark = "";			\
		}								\
	}

#	define	DEBUG_SEND_MUTEX_STREAK_SYSLOG_MESSAGE(CSA, CNL)						\
	{													\
		char	buff[256];										\
														\
		SNPRINTF(buff, SIZEOF(buff), "%s : MUTEX_TYPE = [%d] : MUTEX-STREAK = [%d] : "			\
			"CAT = %lld : CFT = %lld : "								\
			"CQT = %lld : CYT = %lld : DQT = %lld : DYT = %lld : DFT = %lld : "			\
			"REF = %d : CRITREF = %d : MSEMREF = %d\n",						\
			ydb_mutex_benchmark,									\
			CSA->critical->curr_mutex_type,								\
			CNL->switch_streak,									\
			CNL->gvstats_rec.n_crit_success,							\
			CNL->gvstats_rec.n_crit_failed,								\
			CNL->gvstats_rec.n_crit_que_slps,							\
			CNL->gvstats_rec.n_crit_yields,								\
			delta_n_crit_que_slps,									\
			delta_n_crit_yields,									\
			delta_n_crit_failed,									\
			CNL->ref_cnt,										\
			CSA->critical->n_crit_waiters,								\
			CSA->critical->n_msem_waiters);								\
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(buff));			\
	}
#else
#	define	DEBUG_SEND_MUTEX_STREAK_SYSLOG_MESSAGE(CSA, CNL)
#	define	DEBUG_GET_BENCHMARK_ENV
#endif

#define	PROBE_BG_TRACE_PRO_ANY(CSA, EVENT)					\
{										\
	sgmnt_data_ptr_t	lcl_csd;					\
										\
	lcl_csd = CSA->hdr;							\
	assert((NULL != lcl_csd)						\
		|| (CSA == &FILE_INFO(jnlpool->jnlpool_dummy_reg)->s_addrs));	\
	if (NULL != lcl_csd)							\
		BG_TRACE_PRO_ANY(CSA, EVENT);					\
}

/* NOTE:
 * 1) This macro can do one of 3 things. A "continue" or "return" or neither. Caller is expected to handle this.
 * 2) The below macro does not have MBSTART/MBEND as it does "continue" and expects that to continue on to the next iteration
 *    of the for loop in the caller. Having MBSTART/MBEND would introduce a do/while loop in between and break that assumption.
 */
#define	ONE_MUTEX_TRY(CSA, CNL, ADDR, ATSTART, SAVE_JNLPOOL, CURR_MUTEX_TYPE)							\
{																\
	GBLREF jnlpool_addrs_ptr_t	jnlpool;										\
																\
	if (GET_SWAPLOCK(&(ADDR)->semaphore))											\
	{															\
		if (ADDR->curr_mutex_type != CURR_MUTEX_TYPE)									\
		{	/* We got the ydb mutex lock but mutex type concurrently switched from ydb to pthread.			\
			 * Go back to trying to get the pthread mutex lock. Caller is expected to be inside a			\
			 * "for" loop so a "continue" in this macro takes us to the next iteration of the "for"			\
			 * loop. Caller should detect this and take appropriate action.						\
			 */													\
			RELEASE_SWAPLOCK(&ADDR->semaphore);									\
			continue;												\
		}														\
		if (IS_MUTEX_TYPE_ADAPTIVE(CURR_MUTEX_TYPE))									\
		{														\
			if (0 == CNL->gvstats_rec.n_crit_success % CAT_CHECK_FREQUENCY)						\
			{													\
				uint8				delta_n_crit_que_slps;						\
				DEBUG_YDB_MUTEX_ONLY(uint8	delta_n_crit_yields);						\
				DEBUG_YDB_MUTEX_ONLY(uint8	delta_n_crit_failed);						\
																\
				DEBUG_GET_BENCHMARK_ENV;									\
				delta_n_crit_que_slps = CNL->gvstats_rec.n_crit_que_slps - CNL->prev_n_crit_que_slps;		\
				DEBUG_YDB_MUTEX_ONLY(delta_n_crit_yields							\
						= CNL->gvstats_rec.n_crit_yields - CNL->prev_n_crit_yields);			\
				DEBUG_YDB_MUTEX_ONLY(delta_n_crit_failed							\
						= CNL->gvstats_rec.n_crit_failed - CNL->prev_n_crit_failed);			\
				CNL->prev_n_crit_que_slps = CNL->gvstats_rec.n_crit_que_slps;					\
				DEBUG_YDB_MUTEX_ONLY(CNL->prev_n_crit_yields = CNL->gvstats_rec.n_crit_yields);			\
				DEBUG_YDB_MUTEX_ONLY(CNL->prev_n_crit_failed = CNL->gvstats_rec.n_crit_failed);			\
				if (CAT_CHECK_FREQUENCY < (delta_n_crit_que_slps + CSA->critical->n_msem_waiters))		\
				{												\
					CNL->switch_streak++;									\
					DEBUG_SEND_MUTEX_STREAK_SYSLOG_MESSAGE(CSA, CNL);					\
					if (PTHREAD_MUTEX_SWITCH_STREAK <= CNL->switch_streak)					\
					{											\
						/* Below initialization is needed by pthread mutex logic */			\
						CNL->prev_n_crit_failed = CNL->gvstats_rec.n_crit_failed;			\
						CNL->switch_streak = 0;								\
						/* Ensure that ADDR->curr_mutex_type update occurs LAST (before releasing	\
						 * lock) using a write memory barrier.						\
						 */										\
						SHM_WRITE_MEMORY_BARRIER;							\
						ADDR->curr_mutex_type = mutex_type_adaptive_pthread;				\
						RELEASE_SWAPLOCK(&ADDR->semaphore);						\
						/* Wake up all processes in msem wait queue so they can				\
						 * switch to pthread mutex.							\
						 */										\
						for ( ; 0 != ADDR->prochead.que.fl; )						\
						{										\
							boolean_t	woke_self_or_none;					\
																\
							mutex_wakeup(ADDR, &woke_self_or_none);					\
						}										\
						continue;									\
					}											\
				} else												\
				{												\
					DEBUG_YDB_MUTEX_ONLY(if (0 < CNL->switch_streak))					\
						DEBUG_SEND_MUTEX_STREAK_SYSLOG_MESSAGE(CSA, CNL);				\
					CNL->switch_streak = 0;									\
				}												\
			}													\
		}														\
		(CSA)->critical->crit_cycle++;											\
		CSA->now_crit = TRUE;												\
		INCR_GVSTATS_COUNTER((CSA), CNL, n_crit_success, 1);								\
		DEBUG_YDB_MUTEX_ONLY(INTERLOCK_ADD(&ADDR->n_crit_waiters, -1));							\
		if ((CSA)->crit_probe)												\
		{														\
			ABS_TIME 	ATEND;											\
																\
			sys_get_curr_time(&ATEND);		/* end time for the probcrit */					\
			ATEND = sub_abs_time(&ATEND, &(ATSTART));	/* times currently use usec but might someday use ns*/	\
			(CSA)->probecrit_rec.t_get_crit =  (((gtm_uint64_t)ATEND.tv_sec * NANOSECS_IN_SEC) + ATEND.tv_nsec);	\
			(CSA)->probecrit_rec.p_crit_failed = 0;									\
			(CSA)->probecrit_rec.p_crit_yields = 0;									\
			(CSA)->probecrit_rec.p_crit_que_slps = 0;								\
		}														\
		assert(SAVE_JNLPOOL == jnlpool);										\
		return cdb_sc_normal;												\
	}															\
}

#define IS_ONLNRLBK_ACTIVE(CSA)	(0 != CSA->nl->onln_rlbk_pid)

GBLREF int			num_additional_processors;
GBLREF jnl_gbls_t		jgbl;
GBLREF jnlpool_addrs_ptr_t	jnlpool;
GBLREF uint4			process_id;
GBLREF uint4			mu_upgrade_in_prog;

DECLARE_MUTEX_TRACE_CNTRS
DECLARE_MUTEX_TEST_SIGNAL_FLAG

/* The following functions are used only with ydb mutex (not with pthread mutex) */
void			mutex_salvage(sgmnt_addrs *csa);
void			mutex_clean_dead_owner(gd_region* reg, uint4 holder_pid);

error_def(ERR_MUTEXERR);
error_def(ERR_MUTEXFRCDTERM);
error_def(ERR_MUTEXLCKALERT);
error_def(ERR_ORLBKINPROG);
error_def(ERR_REORGUPCNFLCT);
error_def(ERR_TEXT);
error_def(ERR_WCBLOCKED);

/*
 *	General:
 *		Uses compare-and-swap logic to obtain/release a semaphore
 *		in shared memory.
 *
 *	Interface:
 *		void gtm_mutex_init(reg, n, crash)
 *			Initialize mutex structure for region reg with n
 *		queue slots. If crash is TRUE, then this is a "crash"
 *		reinitialization; otherwise, it's a "clean" initialization.
 *
 *		enum cdb_sc gtm_mutex_lock(csa, mutex_lock_type, state)
 *			mutex for region reg corresponding to csa
 *
 *		enum cdb_sc mutex_unlockw(reg);
 *			Unlock mutex for region reg
 *
 *		For routines taking the seq argument, if seq != crash count,
 *		return cdb_sc_critreset.
 *
 *
 *	Mutex structure must be quadword aligned
 *
 *
 *	Mutex structure :
 *
 *		---------------------------------
 *		|	   semaphore		|
 *		---------------------------------
 *		|	  crash count		|
 *		---------------------------------
 *		|	   stuckexec		|	<-UNIX only
 *		--------------------------------
 *		|       # of que slots		|
 *		--------------------------------
 *		|_ fl waiting process que head _|
 *		|_ bl			       _|
 *		|_ global_latch		       _|
 *		---------------------------------
 *		|_ fl unused slots queue head  _|
 *		|_ bl			       _|
 *		|_ global_latch		       _|
 *		---------------------------------
 *		|_ fl	first queue entry      _|
 *		|_ bl			       _|
 *		|_ pid			       _|
 *		---------------------------------
 *		|_ fl	second queue entry     _|
 *		|_ bl			       _|
 *		|_ pid			       _|
 *		---------------------------------
 *		:	:	:	:	:
 *		---------------------------------
 *		|_ fl	last queue entry       _|
 *		|_ bl			       _|
 *		|_ pid			       _|
 *		---------------------------------
 *
 *		Fields may be interspersed with fillers for alignment purposes.
 */

/* This function is called only with ydb mutex (not with pthread mutex) */
static	void	clean_initialize(mutex_struct_ptr_t addr, int n, bool crash)
{
	mutex_que_entry_ptr_t	q_free_entry;

	assert(n > 0);
	addr->queslots = n;
	/* Initialize the waiting process queue to be empty */
	addr->prochead.que.fl = addr->prochead.que.bl = 0;
	SET_LATCH_GLOBAL(&addr->prochead.latch, LOCK_AVAILABLE);
	/* Initialize the free queue to be empty */
	addr->freehead.que.fl = addr->freehead.que.bl = 0;
	SET_LATCH_GLOBAL(&addr->freehead.latch, LOCK_AVAILABLE);
	/* Clear the first free entry */
	q_free_entry = (mutex_que_entry_ptr_t)((sm_uc_ptr_t)&addr->freehead + SIZEOF(mutex_que_head));
	q_free_entry->que.fl = q_free_entry->que.bl = 0;
	q_free_entry->pid = 0;
	while (n--)
	{
		if (-1 == sem_init(&q_free_entry->mutex_wake_msem, TRUE, 0))  /* Shared lock with no initial resources (locked) */
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(7) ERR_MUTEXERR, 0, ERR_TEXT, 2,
				RTS_ERROR_TEXT("Error with mutex wait memory semaphore initialization"), errno);
		/* Initialize fl,bl links to 0 before INSQTI as it (gtm_insqti in relqueopi.c) asserts this */
		DEBUG_ONLY(((que_ent_ptr_t)q_free_entry)->fl = 0;)
		DEBUG_ONLY(((que_ent_ptr_t)q_free_entry)->bl = 0;)
		if (!crash)
			insqt((que_ent_ptr_t)q_free_entry++, (que_ent_ptr_t)&addr->freehead);
		else if (INTERLOCK_FAIL == INSQTI((que_ent_ptr_t)q_free_entry++, (que_head_ptr_t)&addr->freehead))
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(6) ERR_MUTEXERR, 0, ERR_TEXT, 2,
				RTS_ERROR_TEXT("Interlock instruction failure in mutex initialize"));
	}
	SET_LATCH_GLOBAL(&addr->semaphore, LOCK_AVAILABLE);
	SET_LATCH_GLOBAL((global_latch_t *)&addr->stuckexec, LOCK_AVAILABLE);
	if (!crash)
	{
		SET_LATCH(&addr->crashcnt, 0);
	}
	return;
}

/* This function is called only with ydb mutex (not with pthread mutex) */
static	void	crash_initialize(mutex_struct_ptr_t addr, int n, bool crash)
{
	mutex_que_entry_ptr_t	next_entry;

	INTERLOCK_ADD(&addr->crashcnt, 1);
	addr->freehead.que.fl = addr->freehead.que.bl = 0;
	next_entry = (mutex_que_entry_ptr_t)&addr->prochead;
	do
	{
		if (0 == next_entry->que.fl)
		{
			/* Wait queue empty; do a clean initialization */
			clean_initialize(addr, n, crash);
			return;
		}
		next_entry = (mutex_que_entry_ptr_t)((sm_uc_ptr_t)next_entry + next_entry->que.fl);
		if (next_entry <= (mutex_que_entry_ptr_t)&addr->prochead ||
		    next_entry >= (mutex_que_entry_ptr_t)&addr->prochead + n + 1 ||
		    (0 != ((INTPTR_T)next_entry & (SIZEOF(mutex_que_entry) - 1))))
		{
			/*
			 * next_entry == &addr->prochead => loop is done;
			 * next_entry below queue head => queue is corrupt;
			 * next_entry above queue top => queue is corrupt;
			 * next_entry is not (SIZEOF(queue) entry)-byte
			 * aligned => queue is corrupt ...
			 * ... in all cases do a clean initialization
			 */
			clean_initialize(addr, n, crash);
			return;
		}
		/* Wake up process */
		if (next_entry->pid != process_id)
			mutex_wake_proc(&next_entry->mutex_wake_msem);
	} while (TRUE);
}

/* This function is called only with ydb mutex (not with pthread mutex) */
static	enum cdb_sc mutex_long_sleep(mutex_struct_ptr_t addr, sgmnt_addrs *csa, mutex_que_entry_ptr_t free_slot)
{
	enum cdb_sc		status;
	int			wakeup_status;
	int			save_errno;
	boolean_t		woke_self_or_none;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef DEBUG
	if (ydb_white_box_test_case_enabled
		&& (WBTEST_SENDTO_EPERM == ydb_white_box_test_case_number))
	{
		FPRINTF(stderr, "MUPIP BACKUP is about to start long sleep\n");
	}
#	endif
	if (LOCK_AVAILABLE == addr->semaphore.u.parts.latch_pid)
	{
		MUTEX_DPRINT2("%d: Nobody in crit (II) wake procs\n", process_id);
		MUTEX_TRACE_CNTR(mutex_trc_mutex_slp_fn_noslp);
		status = mutex_wakeup(addr, &woke_self_or_none);
		if ((cdb_sc_normal == status) && woke_self_or_none)
			return (cdb_sc_normal);
		else if (cdb_sc_dbccerr == status)
			return (cdb_sc_dbccerr);
	}
	do
	{
		if (free_slot->pid != process_id)
		{	/* My msemaphore is already used by another process.
		   	 * In other words, I was woken up, but missed my wakeup call.
			 * I should return immediately.
			 */
			wakeup_status = 0;
		} else
		{
			int		res;
			struct timespec abs_time;

			res = clock_gettime(CLOCK_REALTIME, &abs_time);
			if (-1 == res)
			{
				save_errno = errno;
				assert(FALSE);
				RTS_ERROR_CSA_ABT(csa, VARLSTCNT(8)
					ERR_SYSCALL, 5, LEN_AND_LIT("clock_gettime"), CALLFROM, save_errno);
			}
			abs_time.tv_sec += MUTEX_CONST_TIMEOUT_VAL;
			/*
			 * the check for EINTR below is valid and should not be converted to an EINTR
			 * wrapper macro, because another condition is checked for the while loop.
			 */
			while (0 != (wakeup_status = sem_timedwait(&free_slot->mutex_wake_msem, &abs_time))) /* NOTE: Assignment */
			{
				save_errno = errno;
				if (ETIMEDOUT == save_errno)	/* semaphore lock attempt timed out */
					break;
				else if (EINTR == save_errno)	/* signal interrupted the call */
					eintr_handling_check();
				else
				{
					HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
					RTS_ERROR_CSA_ABT(csa, VARLSTCNT(7) ERR_MUTEXERR, 0, ERR_TEXT, 2,
						RTS_ERROR_TEXT("Error with mutex wake msem"), save_errno);
				}
			}
			if (0 == wakeup_status)
				HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
			/* wakeup_status is set to 0, if I was able to lock...somebody woke me up;
			 * wakeup_status is set to non-zero, if I timed out and should go to recovery.
			 */
		}
		/* If I was woken up and am a writer, others are blocking on me. So, I shall try to get the lock NOW */
		if (0 == wakeup_status)
			return (cdb_sc_normal);
		else
			mutex_deadlock_check(addr, csa); /* Timed out: See if any deadlocks and fix if detected */
		status = mutex_wakeup(addr, &woke_self_or_none);
		if (cdb_sc_dbccerr == status)
			return (cdb_sc_dbccerr);
		/* else status is cdb_sc_normal */
		assert(cdb_sc_normal == status);
		if (woke_self_or_none)
			return (cdb_sc_normal);
		/* There are others above me in the queue or I missed my wakeup call. In the latter case,
		 * sem_timedwait() will return immediately and there won't be further sleeps.
		 */
	} while (TRUE);
}

/* This function is called only with ydb mutex (not with pthread mutex) */
enum cdb_sc mutex_wakeup(mutex_struct_ptr_t addr, boolean_t *woke_self_or_none)
{
	mutex_que_entry_ptr_t	free_entry;
	int			status;

	*woke_self_or_none = TRUE;	/* TRUE because I have woken none at this point */
	free_entry = (mutex_que_entry_ptr_t)REMQHI((que_head_ptr_t)&addr->prochead);
	if ((mutex_que_entry_ptr_t)NULL == free_entry)
		return (cdb_sc_normal);	/* Empty wait queue */
	if ((mutex_que_entry_ptr_t)INTERLOCK_FAIL == free_entry)
		return (cdb_sc_dbccerr);
	INTERLOCK_ADD(&addr->n_msem_waiters, -1);
	if (free_entry->pid != process_id)
	{
		*woke_self_or_none = FALSE;
		mutex_wake_proc(&free_entry->mutex_wake_msem);
	} else
		*woke_self_or_none = TRUE;
	/* This makes this entry not belong to any process before inserting it into the free queue. */
	free_entry->pid = 0;
	status = INSQTI((que_ent_ptr_t)free_entry, (que_head_ptr_t)&addr->freehead);
	if (INTERLOCK_FAIL == status)
		return (cdb_sc_dbccerr);
	return (cdb_sc_normal); /* No more wakes */
}

/* Unlock the msem to wake the process waiting on it */
void mutex_wake_proc(sem_t *mutex_wake_msem_ptr)
{
	int	rc;

	GTM_SEM_POST(mutex_wake_msem_ptr, rc);
	if (0 > rc)
	{
		assert(FALSE);
		RTS_ERROR_ABT(VARLSTCNT(7) ERR_MUTEXERR, 0, ERR_TEXT, 2, RTS_ERROR_TEXT("Error with sem_post()"), errno);
	}
	return;
}

void	gtm_mutex_init(gd_region *reg, int n, bool crash)
{
	sgmnt_addrs		*csa;
	int4			status;
	pthread_mutexattr_t	crit_attr;
	mutex_type_t		mutex_type;

	/* Assert that the below macros yield the correct values as these macros are used in various places */
	assert(IS_MUTEX_TYPE_PTHREAD(mutex_type_pthread));
	assert(IS_MUTEX_TYPE_PTHREAD(mutex_type_adaptive_pthread));
	assert(!IS_MUTEX_TYPE_PTHREAD(mutex_type_ydb));
	assert(!IS_MUTEX_TYPE_PTHREAD(mutex_type_adaptive_ydb));
	assert(!IS_MUTEX_TYPE_YDB(mutex_type_pthread));
	assert(!IS_MUTEX_TYPE_YDB(mutex_type_adaptive_pthread));
	assert(IS_MUTEX_TYPE_YDB(mutex_type_ydb));
	assert(IS_MUTEX_TYPE_YDB(mutex_type_adaptive_ydb));
	assert(!IS_MUTEX_TYPE_ADAPTIVE(mutex_type_pthread));
	assert(!IS_MUTEX_TYPE_ADAPTIVE(mutex_type_ydb));
	assert(IS_MUTEX_TYPE_ADAPTIVE(mutex_type_adaptive_pthread));
	assert(IS_MUTEX_TYPE_ADAPTIVE(mutex_type_adaptive_ydb));
	csa = &FILE_INFO(reg)->s_addrs;
	if (NULL == csa->hdr || IS_STATSDB_REGNAME(reg))
		mutex_type = mutex_type_ydb;	/* For jnlpool OR statsdb regions, set mutex type to ydb. Not adaptive as
						 * otherwise we would then need to provide a way for the user to see the current
						 * setting inside adaptive (could be ydb or pthread).
						 */
	else
		mutex_type = csa->hdr->mutex_type;
	if (mutex_type_pthread != mutex_type)
	{
		/* Set up ydb mutex (will be used at start) */
		if (!crash)
			clean_initialize(csa->critical, n, crash);
		else
			crash_initialize(csa->critical, n, crash);
	}
	if (mutex_type_ydb != mutex_type)
	{
		/* Also set up pthread mutex (for when it will be needed in case we later switch from ydb mutex to pthread mutex) */
		if (crash)
		{
			pthread_mutex_unlock(&csa->critical->mutex);		/* We may not have it locked, so ignore errors. */
			status = pthread_mutex_destroy(&csa->critical->mutex);
			if (0 != status)
			{
				RTS_ERROR_CSA_ABT(csa, VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("pthread_mutex_destroy"),
					CALLFROM, status);
			}
		}
		status = pthread_mutexattr_init(&crit_attr);
		if (0 != status)
		{
			RTS_ERROR_CSA_ABT(csa, VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("pthread_mutexattr_init"),
				CALLFROM, status);
		}
		status = pthread_mutexattr_settype(&crit_attr, PTHREAD_MUTEX_ERRORCHECK);
		if (0 != status)
		{
			RTS_ERROR_CSA_ABT(csa, VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("pthread_mutexattr_settype"),
				CALLFROM, status);
		}
		status = pthread_mutexattr_setpshared(&crit_attr, PTHREAD_PROCESS_SHARED);
		if (0 != status)
		{
			RTS_ERROR_CSA_ABT(csa, VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("pthread_mutexattr_setpshared"),
				CALLFROM, status);
		}
#		ifdef PTHREAD_MUTEX_ROBUST_SUPPORTED
		status = pthread_mutexattr_setrobust(&crit_attr, PTHREAD_MUTEX_ROBUST);
		if (0 != status)
		{
			RTS_ERROR_CSA_ABT(csa, VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("pthread_mutexattr_setrobust"),
				CALLFROM, status);
		}
#		endif
		status = pthread_mutex_init(&csa->critical->mutex, &crit_attr);
		if (0 != status)
		{
			RTS_ERROR_CSA_ABT(csa, VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("pthread_mutex_init"),
				CALLFROM, status);
		}
	}
	if (0 == csa->critical->crit_cycle)
	{	/* A 0 value of crit_cycle implies this is a call to "gtm_mutex_init()" right after db shm creation
		 * (when all shm values are guaranteed to be 0 initialized). In that case, initialize the mutex manager.
		 */
		csa->critical->curr_mutex_type = mutex_type;
		assert(0 == csa->critical->stuck_cycle);	/* Should be 0 initialized at shared memory creation time */
	}
	return;
}

enum cdb_sc gtm_mutex_lock(sgmnt_addrs *csa, mutex_lock_t mutex_lock_type, wait_state state)
{
	/*  --- These fields are used by both the ydb and pthread mutex logic --- */
	node_local			*cnl;
	ABS_TIME			atstart;
	latch_t				local_crit_cycle = 0;
	int4				local_stuck_cycle = 0;
	mutex_struct_ptr_t		addr;
	int				status;
	/*  --- These fields are used only by the pthread mutex logic --- */
	ABS_TIME			atend;
	struct timespec			timeout;
	uint4				timeout_count;
	/*  --- These fields are used only by the ydb     mutex logic --- */
	boolean_t			try_recovery;
	gtm_int64_t			hard_spin_cnt, sleep_spin_cnt;
	mutex_que_entry_ptr_t		free_slot;
	uint4				in_crit_pid;
	DEBUG_ONLY(jnlpool_addrs_ptr_t	save_jnlpool);
	time_t				curr_time;
	uint4				curr_time_uint4, next_alert_uint4;
	mutex_spin_parms_ptr_t		mutex_spin_parms;
	int				rc;
	gtm_uint64_t			n_crit_yields;
	int				max_sleep_spin_count, max_hard_spin_count;
	mutex_type_t			curr_mutex_type;
	boolean_t			mutex_switched;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!csa->now_crit);
	assert(NULL != csa->region);
	assert(csa == &FILE_INFO(csa->region)->s_addrs);
	cnl = csa->nl;
	assert(NULL != cnl);
	assert((MUTEX_LOCK_WRITE_IMMEDIATE == mutex_lock_type) || (MUTEX_LOCK_WRITE == mutex_lock_type));
	MUTEX_TRACE_CNTR((MUTEX_LOCK_WRITE == mutex_lock_type) ? mutex_trc_lockw : mutex_trc_lockwim);
	addr = csa->critical;
	DEBUG_YDB_MUTEX_ONLY(INTERLOCK_ADD(&addr->n_crit_waiters, 1));
	for ( ; ; )
	{
		curr_mutex_type = addr->curr_mutex_type;
		mutex_switched = FALSE;
		if (IS_MUTEX_TYPE_PTHREAD(curr_mutex_type))
		{
			if (csa->crit_probe)
			{
				csa->probecrit_rec.p_crit_que_slots = 0;
				sys_get_curr_time(&atstart);				/* start time for the probecrit */
			}
			/* Do a trylock first. If we are locking immediate, we are done.
			 * Otherwise we have the opportunity to update stats before doing a longer timed lock attempt.
			 */
			status = pthread_mutex_trylock(&addr->mutex);
			timeout_count = 0;
			do
			{
				if (((EBUSY == status) && (MUTEX_LOCK_WRITE == mutex_lock_type)) || (ETIMEDOUT == status))
				{	/* Got EBUSY from the trylock above or ETIMEDOUT from a previous iteration. */
					UPDATE_CRIT_COUNTER(csa, state);
					INCR_GVSTATS_COUNTER(csa, cnl, n_crit_failed, 1);
					INCR_GVSTATS_COUNTER(csa, cnl, sq_crit_failed, 1);
					if (cnl->doing_epoch)
						INCR_GVSTATS_COUNTER(csa, cnl, n_crits_in_epch, 1);
					local_crit_cycle = addr->crit_cycle;
					local_stuck_cycle = addr->stuck_cycle;
					if (mu_upgrade_in_prog && IS_ONLNRLBK_ACTIVE(csa))
						RTS_ERROR_CSA_ABT(CSA_ARG(NULL) VARLSTCNT(7) ERR_REORGUPCNFLCT, 5,
								LEN_AND_LIT("REORG -UPGRADE"),
								LEN_AND_LIT("MUPIP ROLLBACK -ONLINE in progress"),
								csa->nl->onln_rlbk_pid);
					status = clock_gettime(CLOCK_REALTIME, &timeout);
					if (0 != status)
						RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5,
							LEN_AND_LIT("clock_gettime"), CALLFROM, errno, 0);
					timeout.tv_sec += MUTEX_CONST_TIMEOUT_VAL;
					status = pthread_mutex_timedlock(&addr->mutex, &timeout);
				}
				switch (status)
				{
					case EOWNERDEAD:
						mutex_clean_dead_owner(csa->region, cnl->in_crit);
						/* Record salvage event in db file header if applicable.
						 * Take care not to do it for jnlpool which has no concept of a db cache.
						 * In that case csa->hdr is NULL so check accordingly.
						 */
						assert((NULL != csa->hdr)
							|| (jnlpool && (csa == &FILE_INFO(jnlpool->jnlpool_dummy_reg)->s_addrs)));
						if (NULL != csa->hdr)
						{
							SET_TRACEABLE_VAR(cnl->wc_blocked, WC_BLOCK_RECOVER);
							BG_TRACE_PRO_ANY(csa, wcb_mutex_salvage);
								/* no need to use PROBE_BG_TRACE_PRO_ANY macro
								 * since we already checked for csa->hdr NULL.
								 */
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6,
								LEN_AND_LIT("wcb_mutex_salvage"),
								process_id, &csa->ti->curr_tn, DB_LEN_STR(csa->region));
						}
#						ifdef PTHREAD_MUTEX_CONSISTENT_SUPPORTED
						status = pthread_mutex_consistent(&addr->mutex);
						if (0 != status)
							RTS_ERROR_CSA_ABT(csa, VARLSTCNT(8) ERR_SYSCALL, 5,
								LEN_AND_LIT("pthread_mutex_consistent"), CALLFROM, status);
#						endif
						/* fall through */
					case 0:
						if (addr->curr_mutex_type != curr_mutex_type)
						{	/* We got the pthread mutex lock but mutex type concurrently
							 * switched from pthread to ydb. Go back to trying to get the
							 * ydb mutex lock.
							 */
							pthread_mutex_unlock(&addr->mutex);
							mutex_switched = TRUE;
							break;
						}
						if (IS_MUTEX_TYPE_ADAPTIVE(curr_mutex_type))
						{
							if (0 == cnl->gvstats_rec.n_crit_success % CAT_CHECK_FREQUENCY)
							{
								uint8				delta_n_crit_failed;
								DEBUG_YDB_MUTEX_ONLY(uint8	delta_n_crit_que_slps = 0);
								DEBUG_YDB_MUTEX_ONLY(uint8	delta_n_crit_yields = 0);

								DEBUG_GET_BENCHMARK_ENV;
								delta_n_crit_failed = cnl->gvstats_rec.n_crit_failed
												- cnl->prev_n_crit_failed;
								cnl->prev_n_crit_failed = cnl->gvstats_rec.n_crit_failed;
								if ((CAT_CHECK_FREQUENCY * DFT_DAT_RATIO_THRESHOLD)
									< delta_n_crit_failed)
								{
									cnl->switch_streak++;
									DEBUG_SEND_MUTEX_STREAK_SYSLOG_MESSAGE(csa, cnl);
									if (PTHREAD_MUTEX_SWITCH_STREAK <= cnl->switch_streak)
									{
										/* Below init is needed by ydb mutex logic */
										cnl->prev_n_crit_que_slps
											= cnl->gvstats_rec.n_crit_que_slps;
										cnl->switch_streak = 0;
										/* Ensure curr_mutex_type set occurs LAST (before
										 * releasing lock) using a write memory barrier.
										 */
										SHM_WRITE_MEMORY_BARRIER;
										addr->curr_mutex_type = mutex_type_adaptive_ydb;
										pthread_mutex_unlock(&addr->mutex);
										break;
									}
								} else
								{
									DEBUG_YDB_MUTEX_ONLY(if (0 < cnl->switch_streak))
										DEBUG_SEND_MUTEX_STREAK_SYSLOG_MESSAGE(csa, cnl);
									cnl->switch_streak = 0;
								}
							}
						}
						csa->now_crit = TRUE;
						if (csa->crit_probe)
						{
							sys_get_curr_time(&atend);		/* end time for the probcrit */
							atend = sub_abs_time(&atend, &atstart);
							csa->probecrit_rec.t_get_crit
								= ((gtm_uint64_t)atend.tv_sec * NANOSECS_IN_SEC) + atend.tv_nsec;
							csa->probecrit_rec.p_crit_failed = 0;
							csa->probecrit_rec.p_crit_yields = 0;
							csa->probecrit_rec.p_crit_que_slps = 0;
						}
						INCR_GVSTATS_COUNTER(csa, cnl, n_crit_success, 1);
						DEBUG_YDB_MUTEX_ONLY(INTERLOCK_ADD(&addr->n_crit_waiters, -1));
						addr->crit_cycle++;
						return cdb_sc_normal;
					case EBUSY:
						assert(MUTEX_LOCK_WRITE_IMMEDIATE == mutex_lock_type);
						DEBUG_YDB_MUTEX_ONLY(INTERLOCK_ADD(&addr->n_crit_waiters, -1));
						return cdb_sc_nolock;
					case ETIMEDOUT:
						if (addr->curr_mutex_type != curr_mutex_type)
						{	/* We have not yet gotten the pthread mutex lock but mutex type
							 * concurrently switched from pthread to ydb. Go back to trying to
							 * get the ydb mutex lock.
							 */
							mutex_switched = TRUE;
							break;
						}
						/* Timed out: See if any deadlocks and fix if detected */
						mutex_deadlock_check(addr, csa);
						assert((MUTEX_CONST_TIMEOUT_VAL * 2) == MUTEXLCKALERT_INTERVAL);
						if ((0 == (++timeout_count % 2)) && (addr->crit_cycle == local_crit_cycle))
						{
							uint4	onln_rlbk_pid;

							if (IS_REPL_INST_FROZEN)
								break;
							onln_rlbk_pid = cnl->onln_rlbk_pid;
							if (0 != onln_rlbk_pid)
							{
								send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_ORLBKINPROG, 3,
										onln_rlbk_pid, DB_LEN_STR(csa->region));
								break;
							}
							if (INTERLOCK_ADD(&addr->stuck_cycle, 1) == (local_stuck_cycle + 1))
							{
								uint4	in_crit_pid;

								/* Take a copy of "cnl->in_crit" in a local variable as it
								 * can concurrently change and we do not want to invoke the
								 * C stack function if the pid turns out to be 0 (i.e. the
								 * mutex is not held just at the moment we note it down).
								 * Not noting it down in a local variable could have a race
								 * condition where we find "cnl->in_crit" non-zero and pass
								 * it to the GET_C_STACK_FROM_SCRIPT call but before that it
								 * did become zero.
								 */
								in_crit_pid = cnl->in_crit;
								if (0 != in_crit_pid)
								{
									GET_C_STACK_FROM_SCRIPT("MUTEXLCKALERT",
										process_id, in_crit_pid, addr->crit_cycle);
									send_msg_csa(CSA_ARG(csa) VARLSTCNT(6)
										ERR_MUTEXLCKALERT, 4,
										DB_LEN_STR(csa->region), in_crit_pid,
										addr->crit_cycle);
								}
							}
						}
						if ((addr->crit_cycle == local_crit_cycle) && !TREF(disable_sigcont))
						{	/* The process might have been STOPPED (kill -SIGSTOP).
							 * Send SIGCONT and nudge the stopped process forward. However, skip
							 * this call in case of SENDTO_EPERM white-box test, because we do not
							 * want the intentionally stuck process to be awakened prematurely.
							 */
							/* For the same reasons as few lines above, note "cnl->in_crit" in local */
							uint4	in_crit_pid;

							in_crit_pid = cnl->in_crit;
							if (DEBUG_ONLY(!WBTEST_ENABLED(WBTEST_SENDTO_EPERM) &&) (0 != in_crit_pid))
								continue_proc(in_crit_pid);
						}
						break;
					default:
						assert(!status);
						DEBUG_YDB_MUTEX_ONLY(INTERLOCK_ADD(&addr->n_crit_waiters, -1));
						return cdb_sc_nolock;	/* Keep syntax checkers happy */
				}
				if (mutex_switched)
					break;
			} while (ETIMEDOUT == status);
			/* The only way we could come here is if "addr->curr_mutex_type" changed in value concurrently
			 * while we were trying to get the pthread mutex lock. So go back to the loop and retry based
			 * on the current mutex algorithm in place (note it could be have changed more than once too).
			 */
			assert(mutex_switched);
			continue;
		} else
		{
			DEBUG_ONLY(save_jnlpool = jnlpool);
			if (csa->crit_probe)
			{
				csa->probecrit_rec.p_crit_que_full = 0;
				/* run the active queue to find how many slots are left */
				csa->probecrit_rec.p_crit_que_slots = (gtm_uint64_t)addr->queslots;
					/* free = total number of slots */
				csa->probecrit_rec.p_crit_que_slots -= verify_queue_lock((que_head_ptr_t)&addr->prochead, csa);
					/* less used slots*/
				sys_get_curr_time(&atstart);	/* start time for the probecrit */
			}
			ONE_MUTEX_TRY(csa, cnl, addr, atstart, save_jnlpool, curr_mutex_type);
			/* Note: The above macro would have done a "continue" if it detected the current mutex type
			 * concurrently changed to pthread.
			 */
			if (MUTEX_LOCK_WRITE_IMMEDIATE == mutex_lock_type)
			{
				DEBUG_YDB_MUTEX_ONLY(INTERLOCK_ADD(&addr->n_crit_waiters, -1));
				return cdb_sc_nolock;
			}
			local_crit_cycle = 0;	/* this keeps us from doing a MUTEXLCKALERT on the first cycle
						 * in case the time latch is stale.
						 */
			try_recovery = jgbl.onlnrlbk;	/* salvage lock the first time if we are online rollback
							 * thereby reducing unnecessary waits.
							 */
			mutex_spin_parms = (NULL != csa->hdr)
				? (mutex_spin_parms_ptr_t)(&csa->hdr->mutex_spin_parms)
				: (mutex_spin_parms_ptr_t)((sm_uc_ptr_t)addr + JNLPOOL_CRIT_SPACE);
			n_crit_yields = cnl->gvstats_rec.n_crit_yields;
			INCR_GVSTATS_COUNTER(csa, cnl, n_crit_failed, 1);
			if (cnl->doing_epoch)
				INCR_GVSTATS_COUNTER(csa, cnl, n_crits_in_epch, 1);
			max_sleep_spin_count = mutex_spin_parms->mutex_sleep_spin_count;
			max_hard_spin_count = num_additional_processors ? mutex_spin_parms->mutex_hard_spin_count : 1;
			do
			{	/* master loop */
				if (addr->curr_mutex_type != curr_mutex_type)
				{	/* We did not yet get the ydb mutex lock but mutex type concurrently switched
					 * from ydb to pthread. Go back to trying to get the pthread mutex lock.
					 */
					mutex_switched = TRUE;
					break;
				}
				UPDATE_CRIT_COUNTER(csa, state);
				in_crit_pid = cnl->in_crit;
				MUTEX_TRACE_CNTR(mutex_trc_w_atmpts);
				for (sleep_spin_cnt = max_sleep_spin_count; 0 <= sleep_spin_cnt; sleep_spin_cnt--)
				{	/* fast grab loop for the master lock */
					hard_spin_cnt = max_hard_spin_count;
					for ( ; hard_spin_cnt; --hard_spin_cnt)
					{	/* hard spin loop for the master lock */
						if (addr->curr_mutex_type != curr_mutex_type)
						{	/* We did not yet get the ydb mutex lock but mutex type
							 * concurrently switched from ydb to pthread. Go back to
							 * trying to get the pthread mutex lock.
							 */
							mutex_switched = TRUE;
							break;
						}
						ONE_MUTEX_TRY(csa, cnl, addr, atstart, save_jnlpool, curr_mutex_type);
						/* Note: The above macro would have done a "continue" if it detected the
						 * current mutex type concurrently changed to pthread. In that case, we
						 * would do a "break" in the next iteration of this "for" loop.
						 */
						if (try_recovery)
						{
							mutex_salvage(csa);
							try_recovery = FALSE;
						}
						SPINLOCK_PAUSE;
					}
					if (mutex_switched)
						break;
					if ((cnl->gvstats_rec.n_crit_yields - n_crit_yields) >= max_sleep_spin_count)
						break;	/* A lot of sleeps have already happened across all crit waiting
							 * processes. So avoid wasting kernel time (in context switching)
							 * by going to msem wait outside this loop.
							 */
					if (0 != addr->prochead.que.fl)
						break;	/* There is already at least one process waiting in msem wait for
							 * crit. So do not waste system call sleeping potentially multiple
							 * times for this process and instead go to msem wait as well as
							 * the latter is just 1 system call.
							 */
					/* Sleep for a very short duration */
					assert(!csa->now_crit);
					INCR_GVSTATS_COUNTER(csa, cnl, n_crit_yields, 1);
					GTM_REL_QUANT(mutex_spin_parms->mutex_spin_sleep_mask);
				}
				if (mutex_switched)
					break;
				max_hard_spin_count = 1;
				max_sleep_spin_count = 0;
				try_recovery = FALSE;		/* only try recovery once per MUTEXLCKALERT */
				time(&curr_time);
				assert(MAXUINT4 > curr_time);
				curr_time_uint4 = (uint4)curr_time;
				next_alert_uint4 = addr->stuckexec.cas_time;
				assert(save_jnlpool == jnlpool);
				assert(!csa->jnlpool || (csa->jnlpool == jnlpool));
				assert((curr_time_uint4 <= next_alert_uint4) || (!csa->jnlpool || (csa->jnlpool == jnlpool)));
				if ((curr_time_uint4 > next_alert_uint4) && !IS_REPL_INST_FROZEN)
				{	/* We've waited long enough and the Instance is not frozen
					 * - might be time to send MUTEXLCKALERT.
					 */
					if (COMPSWAP_LOCK(&addr->stuckexec.time_latch, next_alert_uint4,
						(curr_time_uint4 + MUTEXLCKALERT_INTERVAL)))
					{	/* and no one else beat us to it */
						MUTEX_DPRINT3("%d: Acquired STUCKEXEC time lock, to trace %d\n",
							process_id, in_crit_pid);
						assert(process_id != in_crit_pid);
						if (in_crit_pid && (in_crit_pid == cnl->in_crit) && is_proc_alive(in_crit_pid, 0))
						{	/* and we're waiting on some living process */
							if (local_crit_cycle == addr->crit_cycle)
							{	/* and things aren't moving */
								uint4	onln_rlbk_pid;

								assert(local_crit_cycle);
								if (IS_REPL_INST_FROZEN) /* recheck to minimize spurious reports */
									continue;
								onln_rlbk_pid = cnl->onln_rlbk_pid;
								if (0 == onln_rlbk_pid)
								{	/* not rollback - send_msg after trace
									 * less likely to lose process.
									 */
									GET_C_STACK_FROM_SCRIPT("MUTEXLCKALERT",
										process_id, in_crit_pid, addr->crit_cycle);
									send_msg_csa(CSA_ARG(csa) VARLSTCNT(6)
										ERR_MUTEXLCKALERT, 4, DB_LEN_STR(csa->region),
										in_crit_pid, addr->crit_cycle);
									try_recovery = TRUE;	/* set off a salvage */
									continue;	/* make sure to act on it soon,
											 * likely this process.
											 */
								}
								/* If the holding PID belongs to online rollback which holds
								 * crit on database and journal pool for its entire duration,
								 * use a different message.
								 */
								send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_ORLBKINPROG, 3,
										onln_rlbk_pid, DB_LEN_STR(csa->region));
							}
						} else
						{	/* nobody home */
							local_crit_cycle = addr->crit_cycle;
							try_recovery = TRUE;	/* set off a salvage */
							continue;	/* make sure to act on it soon, likely this process */
						}
					} else
					{	/* didn't get resource to do the MUTEXLCKALERT and procestuckexec */
						MUTEX_DPRINT2("%d: Could not acquire STUCKEXEC time lock", process_id);
					}
				}
				/* Time to try for a slot on the mutex queue in order to wait
				 * for a wake up when someone releases crit.
				 */
				if (0 == local_crit_cycle)
					local_crit_cycle = addr->crit_cycle;	/* sync first time waiter */
				if (mu_upgrade_in_prog && IS_ONLNRLBK_ACTIVE(csa))
					RTS_ERROR_CSA_ABT(CSA_ARG(NULL) VARLSTCNT(7) ERR_REORGUPCNFLCT, 5,
							LEN_AND_LIT("REORG -UPGRADE"),
							LEN_AND_LIT("MUPIP ROLLBACK -ONLINE in progress"),
							csa->nl->onln_rlbk_pid);
				/* Try to get a slot on the queue. But first if crit is available, grab it and go */
				ONE_MUTEX_TRY(csa, cnl, addr, atstart, save_jnlpool, curr_mutex_type);
				/* Note: The above macro would have done a "continue" if it detected the
				 * current mutex type concurrently changed to pthread.
				 */
				free_slot = (mutex_que_entry_ptr_t)REMQHI((que_head_ptr_t)&addr->freehead);
				if ((mutex_que_entry_ptr_t)INTERLOCK_FAIL == free_slot)
				{
					assert(save_jnlpool == jnlpool);
					DEBUG_YDB_MUTEX_ONLY(INTERLOCK_ADD(&addr->n_crit_waiters, -1));
					return (cdb_sc_dbccerr);
				}
				if (NULL != free_slot)
				{
					free_slot->pid = process_id;
					/* this loop makes sure that the msemaphore is locked initially before the process goes to
					 * long sleep
					 */
					do
					{
						rc = sem_trywait(&free_slot->mutex_wake_msem);
						if ((-1 != rc) || (EINTR != errno))
							break;
						eintr_handling_check();
					} while (TRUE);
					HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
					assert(MUTEX_LOCK_WRITE == mutex_lock_type);
					status = INSQTI((que_ent_ptr_t)free_slot, (que_head_ptr_t)&addr->prochead);
					if (INTERLOCK_FAIL == status)
					{
						assert(save_jnlpool == jnlpool);
						DEBUG_YDB_MUTEX_ONLY(INTERLOCK_ADD(&addr->n_crit_waiters, -1));
						return (cdb_sc_dbccerr);
					}
					INTERLOCK_ADD(&addr->n_msem_waiters, 1);
					INCR_GVSTATS_COUNTER(csa, cnl, n_crit_que_slps, 1);
					MUTEX_DPRINT3("%d: Inserted %d into wait queue\n", process_id, free_slot->pid);
					status = mutex_long_sleep(addr, csa, free_slot);
					if (cdb_sc_normal != status)
					{
						assert(cdb_sc_dbccerr == status);
						assert(save_jnlpool == jnlpool);
						DEBUG_YDB_MUTEX_ONLY(INTERLOCK_ADD(&addr->n_crit_waiters, -1));
						return (cdb_sc_dbccerr);	/* Too many failures */
					}
					ONE_MUTEX_TRY(csa, cnl, addr, atstart, save_jnlpool, curr_mutex_type);
					/* Note: The above macro would have done a "continue" if it detected the
					 * current mutex type concurrently changed to pthread.
					 */
				} else
				{
					/* Record queue full event in db file header if applicable.  Take care not to do it for
					 * jnlpool which has no concept of a db cache.  In that case csa->hdr is NULL so use
					 * PROBE_BG_TRACE_PRO_ANY macro.
					 */
					PROBE_BG_TRACE_PRO_ANY(csa, mutex_queue_full);
					csa->probecrit_rec.p_crit_que_full++;
					MUTEX_DPRINT2("%d: Free Queue full\n", process_id);
					/* When I can't find a free slot in the queue repeatedly, it means that there is no
					 * progress in the system. A recovery attempt might be warranted in this scenario.
					 * The trick is to return cdb_sc_normal which in turn causes another spin-loop
					 * initiation (or recovery when implemented).  The objective of mutex_sleep is achieved
					 * (partially) in that sleep is done, though queueing isn't.
					 */
				}
				mutex_deadlock_check(addr, csa);
				SLEEP_USEC(HUNDRED_MSEC, FALSE); /* Wait until interrupted by a wake up. Then try again. */
			} while (TRUE);
		}
	}
}

enum cdb_sc mutex_unlockw(sgmnt_addrs *csa)
{
	/* Unlock write access to the mutex at addr */
	mutex_struct_ptr_t 	addr;
	boolean_t		woke_self_or_none;
	enum cdb_sc		status;

	addr = csa->critical;
	assert(csa->now_crit);
	if (IS_MUTEX_TYPE_PTHREAD(addr->curr_mutex_type))
	{
		pthread_mutex_unlock(&addr->mutex);
		status = cdb_sc_normal;
	} else
	{
		MUTEX_TEST_SIGNAL_HERE("WRTUNLCK NOW CRIT\n", FALSE);
		assert(addr->semaphore.u.parts.latch_pid == process_id);
		RELEASE_SWAPLOCK(&addr->semaphore);
		MUTEX_DPRINT2("%d: WRITE LOCK RELEASED\n", process_id);
		status = ((0 == addr->prochead.que.fl) ? cdb_sc_normal : mutex_wakeup(addr, &woke_self_or_none));
	}
	csa->now_crit = FALSE;
	return status;
}

/* Release the COMPSWAP lock AFTER setting cnl->in_crit to 0 as an assert in
 * grab_crit (checking that cnl->in_crit is 0) relies on this order.
 */
void mutex_clean_dead_owner(gd_region* reg, uint4 holder_pid)
{
	sgmnt_addrs		*csa;
	node_local		*cnl;
	sgmnt_data_ptr_t	csd;
	jnlpool_ctl_ptr_t	jpl;
	int 			index1, index2,orig_index2;
	jnl_buffer_ptr_t	jbp;
	uint4			start_freeaddr, orig_freeaddr;
	seq_num			jnl_seqno, strm_seqno;
	int			strmIndex;
	seq_num			strmSeqno60;
	jpl_phase2_in_prog_t	*lastJplCmt;
	jbuf_phase2_in_prog_t	*lastJbufCmt;

	csa = &FILE_INFO(reg)->s_addrs;
	send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_MUTEXFRCDTERM, 3, holder_pid, DB_LEN_STR(reg));
	cnl = csa->nl;
	cnl->in_crit = 0;
	/* Mutex crash repaired, want to do write cache recovery, in case previous holder of crit had set
	 * some cr->in_cw_set to a non-zero value. Not doing cache recovery could cause incorrect
	 * GTMASSERTs in PIN_CACHE_RECORD macro in t_end/tp_tend.				BYPASSOK(GTMASSERT)
	 * Take care not to do it for jnlpool (csa->hdr is NULL in that case) which has no concept of a db cache.
	 */
	csd = csa->hdr;
	assert((NULL != csd) || (NULL != jnlpool));
	assert((NULL != csd) || (csa == &FILE_INFO(jnlpool->jnlpool_dummy_reg)->s_addrs));
	if (NULL == csd)
	{
		/* This is a jnlpool. Check if a process in t_end/tp_tend was killed BEFORE
		 * it incremented jpl->jnl_seqno. If so, undo any changes done in UPDATE_JPL_RSRV_WRITE_ADDR.
		 */
		jpl = jnlpool->jnlpool_ctl;
		assert(NULL != jpl);
		index1 = jpl->phase2_commit_index1;
		index2 = jpl->phase2_commit_index2;
		orig_index2 = index2;
		assert(jpl->write_addr <= jpl->rsrv_write_addr);
		DECR_PHASE2_COMMIT_INDEX(index2, JPL_PHASE2_COMMIT_ARRAY_SIZE);
		lastJplCmt = &jpl->phase2_commit_array[index2];
		/* This process could have been killed during a commit in t_end/tp_tend.
		 *	a) In the middle of Step CMT03 (see secshr_db_clnup.c for CMTxx steps)
		 *		UPDATE_JPL_RSRV_WRITE_ADDR macro OR
		 *	b) After Step CMT03 but before Step CMT07 finished.
		 * In either case, we need to reset jpl to what it was BEFORE Step CMT03 began i.e. roll back.
		 * If the process gets killed AFTER CMT07 finishes, the transaction is rolled forward even
		 *	if it means writing JRT_NULL and/or JRT_INCTN records in jnlpool and/or jnlbuff.
		 * Note that there is still a small window of instructions after CMT07 is done but before
		 *	CMT08 is done (for the first region in case of a multi-region TP transaction) if
		 *	a process gets killed, we will roll forward the jnlpool but roll back the jnlbuff
		 *	and so there would be a seqno with no corresponding journal records in the journal
		 *	files. This is not easily handled so is left as a todo for the future.
		 */
		if ((index1 == orig_index2) || (lastJplCmt->process_id != holder_pid))
		{
			/* CMT02 < killed <= CMT03.
			 * Kill could have happened before CMT03 finished so reset things.
			 * This reset is a no-op if the kill happened even before CMT03 started.
			 * This is Case (a).
			 */
			jpl->rsrv_write_addr = lastJplCmt->start_write_addr + lastJplCmt->tot_jrec_len;
			assert(((lastJplCmt->jnl_seqno + 1) == jpl->jnl_seqno) || !lastJplCmt->jnl_seqno);
			jpl->lastwrite_len = lastJplCmt->tot_jrec_len;
		} else
		{
			assert((lastJplCmt->jnl_seqno == jpl->jnl_seqno) || ((lastJplCmt->jnl_seqno + 1) == jpl->jnl_seqno));
			if (lastJplCmt->jnl_seqno == jpl->jnl_seqno)
			{
				/* CMT03 < killed < CMT07 */
				jpl->rsrv_write_addr = lastJplCmt->start_write_addr;
				jpl->lastwrite_len = lastJplCmt->prev_jrec_len;
				; /* similar layout as UPDATE_JPL_RSRV_WRITE_ADDR */
				jpl->phase2_commit_index2 = index2; /* remove last commit entry */
			}
			/* else : CMT07 < killed and so no rollback needed */
		}
	} else
	{
		/* This is a database shm. Check if a process in t_end/tp_tend was killed BEFORE
		 * Step CMT08 (see secshr_db_clnup.c) when it would have set cnl->update_underway_tn.
		 * If so, undo any changes done in Step CMT06 (UPDATE_JBP_RSRV_FREEADDR).
		 * Effectively rolling back the aborted commit in this region.
		 * Notice that early_tn != curr_tn almost always implies a process in t_end/tp_tend
		 * but in rare cases could also mean a process in the midst of a "wcs_recover". In the
		 * latter case, we do not have any CMTxx steps to undo/redo. We identify the latter case
		 * by checking if "cnl->last_wcs_recover_tn" is the same as "csd->trans_hist.curr_tn".
		 */
		assert((csd->trans_hist.early_tn == csd->trans_hist.curr_tn)
			|| (csd->trans_hist.early_tn == (csd->trans_hist.curr_tn + 1)));
		assert((cnl->update_underway_tn <= csd->trans_hist.curr_tn)
			|| ((cnl->update_underway_tn == (csd->trans_hist.curr_tn + 1))
				&& (csd->trans_hist.early_tn == (csd->trans_hist.curr_tn + 1))
				&& (cnl->last_wcs_recover_tn == csd->trans_hist.curr_tn)));
		assert(csd->trans_hist.early_tn >= cnl->update_underway_tn);
		if (JNL_ENABLED(csd) && (csd->trans_hist.early_tn != csd->trans_hist.curr_tn)
			&& (cnl->last_wcs_recover_tn != csd->trans_hist.curr_tn))
		{
			/* i.e. Process was killed after CMT04 but before CMT12. It is represented as
			 *	CMT04 < killed < CMT12
			 */
			assert(NULL != csa->jnl);
			assert(NULL != csa->jnl->jnl_buff);
			jbp = csa->jnl->jnl_buff;
			index1 = jbp->phase2_commit_index1;
			index2 = jbp->phase2_commit_index2;
			orig_index2 = index2;
			assert(jbp->freeaddr <= jbp->rsrv_freeaddr);
			DECR_PHASE2_COMMIT_INDEX(index2, JNL_PHASE2_COMMIT_ARRAY_SIZE);
			lastJbufCmt = &jbp->phase2_commit_array[index2];
			if (cnl->update_underway_tn != csd->trans_hist.curr_tn)
			{
				/* CMT04 < killed < CMT08.
				 * ----------------------------------------------------------------
				 * Roll-back the entire transaction's effect on this database file
				 * ----------------------------------------------------------------
				 */
				start_freeaddr = lastJbufCmt->start_freeaddr;
				if ((index1 == orig_index2) || (lastJbufCmt->process_id != holder_pid)
						|| (lastJbufCmt->curr_tn != csd->trans_hist.curr_tn))
				{
					/* CMT04 < KILLED <= CMT06.
					 * Kill could have happened before CMT06 finished so reset things.
					 * This reset is a no-op if the kill happened even before CMT06 started.
					 */
					SET_JBP_RSRV_FREEADDR(jbp, start_freeaddr + lastJbufCmt->tot_jrec_len);
				} else
				{
					/* CMTO6 < killed < CMT08 */
					assert(lastJbufCmt->curr_tn == csd->trans_hist.curr_tn);
					/* CMT06 finished. So undo it as a whole */
					assert(lastJbufCmt->process_id == holder_pid);
					/* If CMT06a was in progress when the process was KILLED, then it is
					 * possible jbp->freeaddr was updated as CMT16 (which is what CMT06a
					 * executes) progressed. So undo that too. Likewise for dskaddr,
					 * fsync_dskaddr etc. And finally reset rsrv_freeaddr.
					 */
					assert(jbp->fsync_dskaddr <= jbp->dskaddr);
					orig_freeaddr = jbp->freeaddr;
					if (orig_freeaddr > start_freeaddr)
					{
						jbp->freeaddr = start_freeaddr;
						jbp->free = start_freeaddr % jbp->size;
					} else
						assert(jbp->free == (orig_freeaddr % jbp->size));

					if (jbp->dskaddr > start_freeaddr)
					{
						assert(!GLOBAL_LATCH_HELD_BY_US(&jbp->io_in_prog_latch));
						grab_latch(&jbp->io_in_prog_latch, GRAB_LATCH_INDEFINITE_WAIT, WS_35, csa);
						/* Fix jbp->dskaddr & jbp->dsk while holding io latch */
						assert(orig_freeaddr > start_freeaddr);
						jbp->dskaddr = start_freeaddr;
						jbp->dsk = start_freeaddr % jbp->size;
						/* Setting jbp->dskaddr to start_freeaddr is not enough.
						 * We also need to re-read the partial filesystem-block-size
						 * aligned block of data that precedes the new jbp->dskaddr
						 * since that part is most likely no longer in the jnl buffer
						 * (have been overwritten by the current aborted tn's jnl records).
						 * We can try and optimize this by avoiding setting
						 * jbp->re_read_dskaddr in case no overwrite happened. But it is
						 * not straightforward to detect that and the risk is journal
						 * file corruption. Given "mutex_salvage" is a rare occurrence,
						 * it is safer to re-read unconditionally.
						 */
						jbp->re_read_dskaddr = start_freeaddr;
						rel_latch(&jbp->io_in_prog_latch);
						if (jbp->fsync_dskaddr > start_freeaddr)
						{
							/* Fix jbp->fsync_dskaddr while holding fsync io latch */
							assert(!GLOBAL_LATCH_HELD_BY_US(&jbp->fsync_in_prog_latch));
							grab_latch(&jbp->fsync_in_prog_latch, GRAB_LATCH_INDEFINITE_WAIT,
								WS_36, csa);
							jbp->fsync_dskaddr = start_freeaddr;
							rel_latch(&jbp->fsync_in_prog_latch);
						}
					} else
						assert(jbp->dsk == (jbp->dskaddr % jbp->size));

					/* "jnl_write_phase2" is never called with JRT_EPOCH (see assert there
					 * at function entry of possible rectype values and EPOCH is not in that
					 * list). Therefore we are guaranteed a "jnl_write_epoch_rec" call never
					 * happened since the first call to "jnl_write_reserve" happened in this
					 * transaction. Therefore no UNDO of the effects of "jnl_write_epoch_rec"
					 * needed here (e.g. jbp->post_epoch_freeaddr).
					 */
					assert(jbp->post_epoch_freeaddr <= start_freeaddr);
					SET_JBP_RSRV_FREEADDR(jbp, start_freeaddr);; /* see corresponding
					 * SHM_READ_MEMORY_BARRIER in
					 * "jnl_phase2_cleanup".
					 */
					jbp->phase2_commit_index2 = index2; /* remove last commit entry */
				}
				csd->trans_hist.early_tn = csd->trans_hist.curr_tn; /* Undo CMT04 */
				/* CMT07 is jnlpool related, so no undo done here (in db mutex_salvage) for that */
			} else
			{
				/* CMT08 < killed < CMT12.
				 * -------------------------------------------------------------------
				 * Roll-forward the entire transaction's effect on this database file
				 * -------------------------------------------------------------------
				 * In case process got killed before CMT09 occurred, redo it.
				 * If the process got killed after CMT09, the below redo is a no-op.
				 */
				/* CMT09 redo : start */
				jnl_seqno = lastJbufCmt->jnl_seqno + 1;
				strm_seqno = lastJbufCmt->strm_seqno;
				/* If "strm_seqno" is 0, we are guaranteed this is not a supplementary
				 * instance (i.e. "supplementary" variable in t_end/tp_tend is FALSE).
				 */
				if (strm_seqno)
				{
					strmIndex = GET_STRM_INDEX(strm_seqno);
					strmSeqno60 = GET_STRM_SEQ60(strm_seqno) + 1;
				}
				SET_REG_SEQNO(csa, jnl_seqno, strm_seqno, strmIndex, strmSeqno60, SKIP_ASSERT_TRUE);
				/* CMT09 redo : end */
				csd->trans_hist.curr_tn = csd->trans_hist.early_tn; /* Redo CMT12 */
			}
		}
		/* else: Step CMT04 did not happen OR Database is not journaled.
		 *	 Nothing to undo in this db for Steps CMT01, CMT02 and CMT03.
		 */
		SET_TRACEABLE_VAR(cnl->wc_blocked, WC_BLOCK_RECOVER);
		/* This will ensure we call "wcs_recover" which
		 * will recover CMT04 and other CMTxx steps.
		 */
	}
}

/* This function is called only with ydb mutex (not with pthread mutex) */
void mutex_salvage(sgmnt_addrs *csa)
{
	node_local		*cnl;
	int			index1, index2, orig_index2, salvage_status;
	uint4			holder_pid, onln_rlbk_pid, start_freeaddr, orig_freeaddr;
	boolean_t		mutex_salvaged;
	sgmnt_data_ptr_t	csd;
	jnlpool_ctl_ptr_t	jpl;
	jpl_phase2_in_prog_t	*lastJplCmt;
	jbuf_phase2_in_prog_t	*lastJbufCmt;
	seq_num			jnl_seqno, strm_seqno, strmSeqno60;
	int			strmIndex;
	jnl_buffer_ptr_t	jbp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	cnl = csa->nl;
	if (0 != (holder_pid = csa->critical->semaphore.u.parts.latch_pid))
	{
		mutex_salvaged = FALSE;
		/* If we were trying to obtain a lock we already held, it is an out-of-design situation. We cannot safely
		 * release the lock in that case so create a core file even in pro.
		 */
		assertpro(holder_pid != process_id);
		if (!is_proc_alive(holder_pid, 0))
		{	/* Release the COMPSWAP lock AFTER setting cnl->in_crit to 0 as an assert in
			 * grab_crit (checking that cnl->in_crit is 0) relies on this order.
			 */
			mutex_clean_dead_owner(csa->region, holder_pid);
			COMPSWAP_UNLOCK(&csa->critical->semaphore, holder_pid, LOCK_AVAILABLE);
			mutex_salvaged = TRUE;
			/* Reset jbp->blocked as well if the holder_pid had it set */
			if ((NULL != csa->jnl) && (NULL != csa->jnl->jnl_buff) && (csa->jnl->jnl_buff->blocked == holder_pid))
				csa->jnl->jnl_buff->blocked = 0;
			MUTEX_DPRINT3("%d : mutex salvaged, culprit was %d\n", process_id, holder_pid);
		} else if (!TREF(disable_sigcont))
		{
			/* The process might have been STOPPED (kill -SIGSTOP). Send SIGCONT and nudge the stopped process forward.
			 * However, skip this call in case of SENDTO_EPERM white-box test, because we do not want the intentionally
			 * stuck process to be awakened prematurely. */
			DEBUG_ONLY(if (!ydb_white_box_test_case_enabled || WBTEST_SENDTO_EPERM != ydb_white_box_test_case_number))
				continue_proc(holder_pid);
		}
		/* Record salvage event in db file header if applicable.
		 * Take care not to do it for jnlpool which has no concept of a db cache.
		 * In that case csa->hdr is NULL so check accordingly.
		 */
		assert((NULL != csa->hdr) || (jnlpool && (csa == &FILE_INFO(jnlpool->jnlpool_dummy_reg)->s_addrs)));
		if (mutex_salvaged && (NULL != csa->hdr))
		{
			SET_TRACEABLE_VAR(cnl->wc_blocked, WC_BLOCK_RECOVER);
			BG_TRACE_PRO_ANY(csa, wcb_mutex_salvage); /* no need to use PROBE_BG_TRACE_PRO_ANY macro
								   * since we already checked for csa->hdr non-NULL.
								   */
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_mutex_salvage"),
				process_id, &csa->ti->curr_tn, DB_LEN_STR(csa->region));
		}
	}
}

