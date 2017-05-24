/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* GT.M Mutex Control */

#include "mdef.h"

#include "gtm_time.h"	/* for time() */
#include "gtm_socket.h"
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_select.h"
#include "gtm_un.h"

#include <errno.h>
#if defined(__sparc) || defined(__hpux) || defined(__MVS__) || defined(__linux__) || defined(__CYGWIN__)
#include "gtm_limits.h"
#else
#include <sys/limits.h>
#endif

#include "aswp.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "lockconst.h"
#include "interlock.h"
#include "filestruct.h"
#include "io.h"
#include "jnl.h"
#include "gdsbgtr.h"
#include "mutex.h"
#include "relqueopi.h"
#include "eintr_wrappers.h"
#include "send_msg.h"
#include "is_proc_alive.h"
#include "compswap.h"
#include "gtmsecshr.h"
#include "gtm_rel_quant.h"
#include "add_inter.h"
#include "mutex_deadlock_check.h"
#include "gt_timer.h"
#include "gtmio.h"
#include "gtm_c_stack_trace.h"
#include "sleep.h"
#include "anticipatory_freeze.h"
#ifdef DEBUG
#include "wbox_test_init.h"
#include "repl_msg.h"			/* needed by gtmsource.h */
#include "gtmsource.h"			/* required for jnlpool GBLREF */
#endif

#ifdef MUTEX_MSEM_WAKE
#define MUTEX_MAX_WAIT        		(MUTEX_CONST_TIMEOUT_VAL * MILLISECS_IN_SEC)
#endif

/* The following PROBE_* macros invoke the corresponding * macros except in the case csa->hdr is NULL.
 * This is possible if the csa corresponds to the journal pool where there is no notion of a db hdr.
 * In that case, we skip invoking the * macros.
 */
#define	PROBE_SET_TRACEABLE_VAR(CSA, VALUE)					\
{										\
	sgmnt_data_ptr_t	lcl_csd;					\
										\
	lcl_csd = CSA->hdr;							\
	assert((NULL != lcl_csd)						\
		|| (CSA == &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs));	\
	if (NULL != lcl_csd)							\
		SET_TRACEABLE_VAR(CSA->nl->wc_blocked, TRUE);			\
}

#define	PROBE_BG_TRACE_PRO_ANY(CSA, EVENT)					\
{										\
	sgmnt_data_ptr_t	lcl_csd;					\
										\
	lcl_csd = CSA->hdr;							\
	assert((NULL != lcl_csd)						\
		|| (CSA == &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs));	\
	if (NULL != lcl_csd)							\
		BG_TRACE_PRO_ANY(CSA, EVENT);					\
}

#define	ONE_MUTEX_TRY(CSA, ADDR, CRASH_CNT, PID, LOCK_TYPE, SPINS, SPIN_CNT, YIELDS, YIELD_CNT, Q_SLPS, IN_EPOCH, ATSTART)	\
MBSTART {															\
	ABS_TIME 		ATEND;												\
	enum cdb_sc		STATUS;												\
	gtm_uint64_t		FAILED_LOCK_ATTEMPTS;										\
	node_local		*CNL;												\
																\
	CNL = (CSA)->nl;													\
	if ((CRASH_CNT) != (ADDR)->crashcnt)											\
		STATUS = cdb_sc_critreset;											\
	else if (GET_SWAPLOCK(&(ADDR)->semaphore))										\
	{															\
		(CSA)->critical->crit_cycle++;											\
		MUTEX_DPRINT3("%d: Write %sACQUIRED\n", (PID), (MUTEX_LOCK_WRITE == (LOCK_TYPE)) ? "" : "IMMEDIATE ");		\
		MUTEX_TEST_SIGNAL_HERE("WRTLCK NOW CRIT\n", FALSE);								\
		SET_CSA_NOW_CRIT_TRUE(CSA);											\
		MUTEX_TEST_SIGNAL_HERE("WRTLCK SUCCESS\n", FALSE);								\
		if (-1 != (SPIN_CNT))												\
		{														\
			assert((gtm_uint64_t)(SPIN_CNT) <= (SPINS));								\
			(SPINS) -= (gtm_uint64_t)(SPIN_CNT);			/* reduce by the number of unused */		\
		}														\
		if (-1 != (YIELD_CNT))												\
		{														\
			assert((gtm_uint64_t)(YIELD_CNT) <= (YIELDS));								\
			(YIELDS) -= (gtm_uint64_t)(YIELD_CNT);			/* reduce by the number of unused */		\
		}														\
		STATUS = cdb_sc_normal;												\
		INCR_GVSTATS_COUNTER((CSA), CNL, n_crit_success, 1);								\
	} else															\
		STATUS = cdb_sc_nolock;												\
	if ((cdb_sc_normal == STATUS) || (MUTEX_LOCK_WRITE_IMMEDIATE == (LOCK_TYPE)) || (cdb_sc_critreset == STATUS))		\
	{															\
		FAILED_LOCK_ATTEMPTS = (SPINS) + (YIELDS) + (Q_SLPS) + (cdb_sc_nolock == STATUS);				\
		if ((IN_EPOCH))													\
			INCR_GVSTATS_COUNTER((CSA), CNL, n_crits_in_epch, FAILED_LOCK_ATTEMPTS);				\
		INCR_GVSTATS_COUNTER((CSA), CNL, n_crit_failed, FAILED_LOCK_ATTEMPTS);						\
		INCR_GVSTATS_COUNTER((CSA), CNL, sq_crit_failed, FAILED_LOCK_ATTEMPTS * FAILED_LOCK_ATTEMPTS);			\
		if (YIELDS)												\
		{														\
			INCR_GVSTATS_COUNTER((CSA), CNL, n_crit_yields, (YIELDS));						\
			INCR_GVSTATS_COUNTER((CSA), CNL, sq_crit_yields, (YIELDS) * (YIELDS));					\
		}														\
		if (Q_SLPS)													\
		{														\
			INCR_GVSTATS_COUNTER((CSA), CNL, n_crit_que_slps, (Q_SLPS));						\
			INCR_GVSTATS_COUNTER((CSA), CNL, sq_crit_que_slps, (Q_SLPS) * (Q_SLPS));				\
		}														\
		if ((CSA)->crit_probe)												\
		{														\
			sys_get_curr_time(&ATEND);		/* end time for the probcrit */					\
			ATEND = sub_abs_time(&ATEND, &(ATSTART));	/* times currently use usec but might someday use ns*/	\
			(CSA)->probecrit_rec.t_get_crit =  ((gtm_uint64_t)(ATEND.at_sec * 1000000) + ATEND.at_usec) * 1000;	\
			(CSA)->probecrit_rec.p_crit_failed = (gtm_uint64_t)FAILED_LOCK_ATTEMPTS;				\
			(CSA)->probecrit_rec.p_crit_yields = (gtm_uint64_t)(YIELDS);						\
			(CSA)->probecrit_rec.p_crit_que_slps = (gtm_uint64_t)(Q_SLPS);						\
		}														\
		return STATUS;													\
	}															\
} MBEND


GBLREF int			num_additional_processors;
GBLREF jnl_gbls_t		jgbl;
GBLREF jnlpool_addrs		jnlpool;
GBLREF uint4			process_id;
GBLREF uint4			mutex_per_process_init_pid;
#ifdef MUTEX_MSEM_WAKE
#  ifdef POSIX_MSEM
static sem_t			*mutex_wake_msem_ptr = NULL;
#  else
static msemaphore		*mutex_wake_msem_ptr = NULL;
#  endif
static mutex_que_entry_ptr_t	msem_slot;
#else
GBLREF fd_set			mutex_wait_on_descs;
GBLREF int			mutex_sock_fd;
#endif
#ifdef DEBUG
GBLREF boolean_t		in_mu_rndwn_file;
#endif

DECLARE_MUTEX_TRACE_CNTRS
DECLARE_MUTEX_TEST_SIGNAL_FLAG

static	boolean_t	woke_self;
static	boolean_t	woke_none;
static	unsigned short	next_rand[3];
static	int		optimistic_attempts;
static	int		mutex_expected_wake_instance = 0;

static	enum cdb_sc	mutex_wakeup(mutex_struct_ptr_t addr, mutex_spin_parms_ptr_t mutex_spin_parms);
void			mutex_salvage(gd_region *reg);

error_def(ERR_MUTEXERR);
error_def(ERR_MUTEXFRCDTERM);
error_def(ERR_MUTEXLCKALERT);
error_def(ERR_ORLBKINPROG);
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
 *		enum cdb_sc gtm_mutex_lock(reg, mutex_spin_parms, seq, mutex_lock_type)
 *			mutex for region reg
 *
 *		enum cdb_sc mutex_unlockw(reg, seq);
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
 *		|  super_crit [CCP use only]^   |
 *		---------------------------------
 *		|_ fl	second queue entry     _|
 *		|_ bl			       _|
 *		|_ pid			       _|
 *		|  super_crit [CCP use only]^   |
 *		---------------------------------
 *		:	:	:	:	:
 *		---------------------------------
 *		|_ fl	last queue entry       _|
 *		|_ bl			       _|
 *		|_ pid			       _|
 *		|  super_crit [CCP use only]^   |
 *		---------------------------------
 *
 *		^Note:  only one entry at a time (at the head of the
 *		        waiting process queue) will ever use "super_crit".
 *		        CCP is used in VMS only - 03/11/98
 *		07-31-2002 se: super-crit is not used at all anymore. Comments are left for historical purposes.
 *
 *		Fields may be interspersed with fillers for alignment purposes.
 */

static	void	clean_initialize(mutex_struct_ptr_t addr, int n, bool crash)
{
	mutex_que_entry_ptr_t	q_free_entry;
#	if defined(MUTEX_MSEM_WAKE) && !defined(POSIX_MSEM)
	msemaphore		*status;
#	endif

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
	q_free_entry->super_crit = (void *)NULL;
	q_free_entry->mutex_wake_instance = 0;
	while (n--)
	{
#		ifdef MUTEX_MSEM_WAKE
#		  ifdef POSIX_MSEM
		if (-1 == sem_init(&q_free_entry->mutex_wake_msem, TRUE, 0))  /* Shared lock with no initial resources (locked) */
#		  else
		if ((NULL == (status = msem_init(&q_free_entry->mutex_wake_msem, MSEM_LOCKED))) || ((msemaphore *)-1 == status))
#		  endif
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_MUTEXERR, 0, ERR_TEXT, 2,
				RTS_ERROR_TEXT("Error with mutex wait memory semaphore initialization"), errno);
#		endif
		/* Initialize fl,bl links to 0 before INSQTI as it (gtm_insqti in relqueopi.c) asserts this */
		DEBUG_ONLY(((que_ent_ptr_t)q_free_entry)->fl = 0;)
		DEBUG_ONLY(((que_ent_ptr_t)q_free_entry)->bl = 0;)
		if (INTERLOCK_FAIL == INSQTI((que_ent_ptr_t)q_free_entry++, (que_head_ptr_t)&addr->freehead))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUTEXERR, 0, ERR_TEXT, 2,
				RTS_ERROR_TEXT("Interlock instruction failure in mutex initialize"));
	}
	SET_LATCH_GLOBAL(&addr->semaphore, LOCK_AVAILABLE);
	SET_LATCH_GLOBAL((global_latch_t *)&addr->stuckexec, LOCK_AVAILABLE);
	if (!crash)
	{
		SET_LATCH(&addr->crashcnt, 0);
		SET_LATCH_GLOBAL(&addr->crashcnt_latch, LOCK_AVAILABLE);
	}
	return;
}

static	void	crash_initialize(mutex_struct_ptr_t addr, int n, bool crash)
{
	/*
	 * mutex_wake_proc() is not declared here because its return value
	 * is left unspecified in its definition (see mutex_wake_proc.c)
	 */
	mutex_que_entry_ptr_t	next_entry;

	INCR_CNT(&addr->crashcnt, &addr->crashcnt_latch);
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
#			ifdef MUTEX_MSEM_WAKE
			mutex_wake_proc(&next_entry->mutex_wake_msem);
#			else
			mutex_wake_proc((sm_int_ptr_t)&next_entry->pid, next_entry->mutex_wake_instance);
#			endif
	} while (TRUE);
}

static	enum cdb_sc mutex_long_sleep(mutex_struct_ptr_t addr, sgmnt_addrs *csa,  mutex_spin_parms_ptr_t mutex_spin_parms)
{
	enum cdb_sc		status;
	boolean_t		wakeup_status;
#	ifdef MUTEX_MSEM_WAKE
	boolean_t		msem_timedout;
	int			save_errno;
#	else
	struct timeval		timeout;
	int			timeout_threshold;
	struct sockaddr_un	mutex_woke_me_proc;
	GTM_SOCKLEN_TYPE	mutex_woke_me_proc_len;
	mutex_wake_msg_t	mutex_wake_msg[2];
	int			sel_stat;
	ssize_t			nbrecvd;
	int			timeout_intr_slpcnt;
	long			timeout_val;
#	endif

#	ifdef DEBUG
	if (gtm_white_box_test_case_enabled
		&& (WBTEST_SENDTO_EPERM == gtm_white_box_test_case_number))
	{
		FPRINTF(stderr, "MUPIP BACKUP is about to start long sleep\n");
	}
#	endif
	if (LOCK_AVAILABLE == addr->semaphore.u.parts.latch_pid && --optimistic_attempts)
	{
		MUTEX_DPRINT2("%d: Nobody in crit (II) wake procs\n", process_id);
		MUTEX_TRACE_CNTR(mutex_trc_mutex_slp_fn_noslp);
		status = mutex_wakeup(addr, mutex_spin_parms);
		if ((cdb_sc_normal == status) && (woke_self || woke_none))
			return (cdb_sc_normal);
		else if (cdb_sc_dbccerr == status)
			return (cdb_sc_dbccerr);
	}
	optimistic_attempts = MUTEX_MAX_OPTIMISTIC_ATTEMPTS;
	do
	{
#		ifdef MUTEX_MSEM_WAKE
		if (msem_slot->pid != process_id)
		{	/* My msemaphore is already used by another process.
		   	 * In other words, I was woken up, but missed my wakeup call.
			 * I should return immediately.
			 */
			wakeup_status = TRUE;
		} else
		{
			TIMEOUT_INIT(msem_timedout, MUTEX_MAX_WAIT);
			/*
			 * the check for EINTR below is valid and should not be converted to an EINTR
			 * wrapper macro, because another condition is checked for the while loop.
			 */
			while (!(wakeup_status = (0 == MSEM_LOCKW(mutex_wake_msem_ptr))))
			{
				save_errno = errno;
				if (EINTR == save_errno)
				{
					if (msem_timedout)
					{
						MUTEX_DPRINT3("%d: msem sleep done, heartbeat_counter = %d\n",
							     process_id, heartbeat_counter);
						break;
					}
					MUTEX_DPRINT3("%d: msem sleep continue, heartbeat_counter = %d\n",
						      process_id, heartbeat_counter);
				} else
				{
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_MUTEXERR, 0, ERR_TEXT, 2,
						RTS_ERROR_TEXT("Error with mutex wake msem"), save_errno);
				}
			}
			TIMEOUT_DONE(msem_timedout);
			/* wakeup_status is set to true, if I was able to lock...somebody woke me up;
			 * wakeup_status is set to false, if I timed out and should go to recovery.
			 */
		}
#		else
		do
		{
			timeout.tv_sec = MUTEX_CONST_TIMEOUT_VAL;
			timeout.tv_usec = (gtm_tv_usec_t)(nrand48(next_rand) & ((1U << MUTEX_NUM_WAIT_BITS) - 1)) + 1;
			timeout_val = (timeout.tv_sec * E_6) + timeout.tv_usec;
			/*
			 * Can add backoff logic here to increase the timeout
			 * as the number of attempts increase
			 */
			timeout_intr_slpcnt = MUTEX_INTR_SLPCNT;
			MUTEX_DPRINT4("%d: Sleeping for %d s %d us\n", process_id, timeout.tv_sec, timeout.tv_usec);
			assertpro(FD_SETSIZE > mutex_sock_fd);
			FD_SET(mutex_sock_fd, &mutex_wait_on_descs);
			MUTEX_TRACE_CNTR(mutex_trc_slp);
			/*
			 * the check for EINTR below is valid and should not be converted to an EINTR
			 * wrapper macro, since it might be a timeout.
			 */
			while (-1 == (sel_stat =
				select(mutex_sock_fd + 1, &mutex_wait_on_descs, (fd_set *)NULL, (fd_set *)NULL, &timeout)))
			{
				if (EINTR == errno)
				{	/* somebody interrupted me, reduce the timeout by half and continue */
					MUTEX_TRACE_CNTR(mutex_trc_slp_intr);
					if (!(timeout_intr_slpcnt--)) /* Assume timed out */
					{
						sel_stat = 0;
						MUTEX_TRACE_CNTR(mutex_trc_intr_tmout);
						break;
					}
				} else
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_TEXT, 2,
						RTS_ERROR_TEXT("Error with mutex select. Running in degraded mode"), errno);
				timeout_val >>= 1;
				timeout.tv_sec = timeout_val / E_6;
				timeout.tv_usec = (gtm_tv_usec_t)(timeout_val % E_6);
				MUTEX_DPRINT4("%d: Interrupted select, new timeout %d s %d us\n", process_id, timeout.tv_sec,
					timeout.tv_usec);
				/* the next line deals with the case that an interrupted select has changed mutex_wait_on_descs */
				assertpro(FD_SETSIZE > mutex_sock_fd);
				FD_SET(mutex_sock_fd, &mutex_wait_on_descs);
				MUTEX_TRACE_CNTR(mutex_trc_slp);
			}
			if (1 == sel_stat) /* Somebody woke me up */
			{
				mutex_woke_me_proc_len = SIZEOF(struct sockaddr_un);
				RECVFROM_SOCK(mutex_sock_fd, (void *)&mutex_wake_msg[0], SIZEOF(mutex_wake_msg), 0,
					(struct sockaddr *)&mutex_woke_me_proc,
					(GTM_SOCKLEN_TYPE *)&mutex_woke_me_proc_len, nbrecvd);
				if (SIZEOF(mutex_wake_msg) == nbrecvd) /* Drained out both old and new wake messages */
				{
					MUTEX_TRACE_CNTR(mutex_trc_slp_wkup);
					MUTEX_TRACE_CNTR(mutex_trc_pgybckd_dlyd_wkup);
					MUTEX_DPRINT3("%d: %d woke me up, drained delayed message too\n", process_id,
						mutex_wake_msg[1].pid);
					wakeup_status = TRUE;
					break;
				}
				if (BIN_TOGGLE(mutex_expected_wake_instance) == mutex_wake_msg[0].mutex_wake_instance)
				{
					MUTEX_DPRINT3("%d: %d woke me up\n", process_id, mutex_wake_msg[0].pid);
					MUTEX_TRACE_CNTR(mutex_trc_slp_wkup);
					wakeup_status = TRUE;
					break;
				} /* else, old wake msg, ignore */
				MUTEX_DPRINT3("%d: %d sent me delayed wake msg\n", process_id, mutex_wake_msg[0].pid);
				MUTEX_TRACE_CNTR(mutex_trc_xplct_dlyd_wkup);
			} else if (0 == sel_stat) /* Timed out */
			{
				MUTEX_DPRINT2("%d: Sleep done, go wake others\n", process_id);
				MUTEX_TRACE_CNTR(mutex_trc_slp_tmout);
				wakeup_status = FALSE;
				break;
			}
		} while (TRUE);
#		endif
		/*
		 * If I was woken up and am a writer, others are blocking on
		 * me. So, I shall try to get the lock NOW
		 */
		if (wakeup_status)
			return (cdb_sc_normal);
		else
			mutex_deadlock_check(addr, csa); /* Timed out: See if any deadlocks and fix if detected */
		status = mutex_wakeup(addr, mutex_spin_parms);
		if (cdb_sc_dbccerr == status)
			return (cdb_sc_dbccerr);
		/* else status is cdb_sc_normal */
		if (woke_self || woke_none)
			return (cdb_sc_normal);
		/*
		 * There are others above me in the queue or I missed my
		 * wakeup call. In the latter case, select or msem_lock will return
		 * immediately and there won't be further sleeps.
		 */
	} while (TRUE);
}

static	enum cdb_sc mutex_wakeup(mutex_struct_ptr_t addr, mutex_spin_parms_ptr_t mutex_spin_parms)
{
	mutex_que_entry_ptr_t	free_entry;
	int			queue_retry_counter_remq,
				quant_retry_counter_remq,
				queue_retry_counter_insq,
				quant_retry_counter_insq;
	uint4			wake_this_pid;
	int			wake_instance;

	woke_self = FALSE;
	woke_none = TRUE;
	quant_retry_counter_remq = queue_retry_counter_remq = 0;
	do
	{
		do
		{
			free_entry = (mutex_que_entry_ptr_t)REMQHI((que_head_ptr_t)&addr->prochead);
			if ((mutex_que_entry_ptr_t)NULL != free_entry &&
			    (mutex_que_entry_ptr_t)INTERLOCK_FAIL != free_entry)
			{
				wake_this_pid = free_entry->pid;
				wake_instance = free_entry->mutex_wake_instance;
#				ifdef MUTEX_MSEM_WAKE
				/*
				 * In case of msem wakeup, the msem has to be
				 * unlocked before returning free_entry to
				 * free queue, or else another process might
				 * use the same msem (in free_entry) for its
				 * sleep.
				 */
				if (wake_this_pid != process_id)
					mutex_wake_proc(&free_entry->mutex_wake_msem);
				else
					woke_self = TRUE;
				/* This makes this entry not belong to any process before
				 * inserting it into the free queue.
				 */
				free_entry->pid = quant_retry_counter_insq = queue_retry_counter_insq = 0;
#				endif
				do
				{
					do
					{
						if (INTERLOCK_FAIL !=
							INSQTI((que_ent_ptr_t)free_entry, (que_head_ptr_t)&addr->freehead))
						{
							MUTEX_DPRINT3("%d: Waking up %d\n", process_id, wake_this_pid);
							woke_none = FALSE;
							if (wake_this_pid != process_id)
							{
								MUTEX_TRACE_CNTR(mutex_trc_crit_wk);
#								ifndef MUTEX_MSEM_WAKE
								mutex_wake_proc((sm_int_ptr_t)&wake_this_pid, wake_instance);
#								endif
							} else
							{
								/* With
								 * msem wake,
								 * this can
								 * never
								 * happen */
								woke_self = TRUE;
							}
							return (cdb_sc_normal); /* No more wakes */
						}
						if (!queue_retry_counter_insq)		/* save memory reference on fast path */
							queue_retry_counter_insq = mutex_spin_parms->mutex_hard_spin_count;
					} while (--queue_retry_counter_insq);
					if (!quant_retry_counter_insq)			/* save memory reference on fast path */
						quant_retry_counter_insq = MAX(E_4 - mutex_spin_parms->mutex_hard_spin_count,
							mutex_spin_parms->mutex_sleep_spin_count);
					if (!(--quant_retry_counter_insq))
					{
#						ifndef MUTEX_MSEM_WAKE
						if (wake_this_pid != process_id)
							mutex_wake_proc((sm_int_ptr_t)&wake_this_pid, wake_instance);
#						endif
						/* Too many failures */
						return (cdb_sc_dbccerr);
					} else
						GTM_REL_QUANT(mutex_spin_parms->mutex_spin_sleep_mask);
				} while (quant_retry_counter_insq);	/* actually terminated by return 3 lines above */
			} else if ((mutex_que_entry_ptr_t)NULL == free_entry)
			{
				/* Empty wait queue */
				MUTEX_DPRINT2("%d: Empty wait queue\n", process_id);
				return (cdb_sc_normal);
			} /* else secondary interlock failed */
			if (!queue_retry_counter_remq)			/* save memory reference on fast path */
				quant_retry_counter_remq = mutex_spin_parms->mutex_hard_spin_count;
		} while (--queue_retry_counter_remq);
		if (!quant_retry_counter_remq)				/* save memory reference on fast path */
			quant_retry_counter_remq = MAX(E_4 - mutex_spin_parms->mutex_hard_spin_count,
							mutex_spin_parms->mutex_sleep_spin_count);
		if (!(--quant_retry_counter_remq))
			return (cdb_sc_dbccerr); /* Too many queue failures */
		else
			GTM_REL_QUANT(mutex_spin_parms->mutex_spin_sleep_mask);
	} while (quant_retry_counter_remq);
	return (cdb_sc_dbccerr); /* This will never get executed, added to make compiler happy */
}

void	gtm_mutex_init(gd_region *reg, int n, bool crash)
{
	if (!crash)
		clean_initialize((&FILE_INFO(reg)->s_addrs)->critical, n, crash);
	else
		crash_initialize((&FILE_INFO(reg)->s_addrs)->critical, n, crash);
	return;
}

enum cdb_sc gtm_mutex_lock(gd_region *reg,
			      mutex_spin_parms_ptr_t mutex_spin_parms,
			      int crash_count,
			      mutex_lock_t mutex_lock_type)
{
	boolean_t		epoch_count, try_recovery;
	enum cdb_sc		status;
	gtm_int64_t		hard_spin_cnt, sleep_spin_cnt;
	gtm_uint64_t		queue_sleeps, spins, yields;
	int 			n_queslots, redo_cntr;
	latch_t			local_crit_cycle;
	mutex_struct_ptr_t 	addr;
	mutex_que_entry_ptr_t	free_slot;
	node_local		*cnl;
	uint4			in_crit_pid;
	sgmnt_addrs		*csa;
	time_t			curr_time;
	uint4			curr_time_uint4, next_alert_uint4;
	ABS_TIME 		atstart;
#	ifdef MUTEX_MSEM_WAKE
	int			rc;
#	endif
        DCL_THREADGBL_ACCESS;

        SETUP_THREADGBL_ACCESS;
	csa = &FILE_INFO(reg)->s_addrs;
	assert(!csa->now_crit);
	cnl = csa->nl;
	/* Check that "mutex_per_process_init" has happened before we try to grab crit and that it was done with our current
	 * pid (i.e. ensure that even in the case where parent did the mutex init with its pid and did a fork, the child process
	 * has done a reinitialization with its pid). The only exception is if we are in "mu_rndwn_file" in which case we
	 * know for sure there is no other pid accessing the database shared memory.
	 */
	assert((MUTEX_LOCK_WRITE_IMMEDIATE == mutex_lock_type) || (MUTEX_LOCK_WRITE == mutex_lock_type));
	assert((mutex_per_process_init_pid == process_id) || ((0 == mutex_per_process_init_pid) && in_mu_rndwn_file));
	MUTEX_TRACE_CNTR((MUTEX_LOCK_WRITE == mutex_lock_type) ? mutex_trc_lockw : mutex_trc_lockwim);
	optimistic_attempts = MUTEX_MAX_OPTIMISTIC_ATTEMPTS;
	queue_sleeps = csa->probecrit_rec.p_crit_que_full = 0;
	spins = yields = 0;
	local_crit_cycle = 0;	/* this keeps us from doing a MUTEXLCKALERT on the first cycle in case the time latch is stale */
	try_recovery = jgbl.onlnrlbk; /* salvage lock the first time if we are online rollback thereby reducing unnecessary waits */
	epoch_count = cnl->doing_epoch;
	addr = csa->critical;
	if (csa->crit_probe)
	{	/* run the active queue to find how many slots are left */
		csa->probecrit_rec.p_crit_que_slots = (gtm_uint64_t)addr->queslots;		/* free = total number of slots */
		csa->probecrit_rec.p_crit_que_slots -= verify_queue_lock((que_head_ptr_t)&addr->prochead); /* less used slots */
		sys_get_curr_time(&atstart);							/* start time for the probecrit */
	}
	do
	{	/* master loop */
		in_crit_pid = cnl->in_crit;
		sleep_spin_cnt = -1;
		MUTEX_TRACE_CNTR(mutex_trc_w_atmpts);
		do
		{	/* fast grab loop for the master lock */
			for (status = cdb_sc_nolock, hard_spin_cnt = -1; hard_spin_cnt; --hard_spin_cnt)
			{	/* hard spin loop for the master lock - don't admit any MUTEX_LOCK_WRITE_IMMEDIATE to try a bit '*/
				ONE_MUTEX_TRY(csa, addr, crash_count, process_id, MUTEX_LOCK_WRITE, spins, hard_spin_cnt,
					yields, sleep_spin_cnt, queue_sleeps, epoch_count, atstart);
				if (try_recovery)
				{
					mutex_salvage(reg);
					try_recovery = FALSE;
				}
				if (-1 == hard_spin_cnt)	/* save memory reference on fast path */
				{
					hard_spin_cnt = num_additional_processors ? mutex_spin_parms->mutex_hard_spin_count : 1;
					spins += hard_spin_cnt;			/* start with max */
				}
			}
				/* Sleep for a very short duration */
#			ifdef MUTEX_TRACE
			if (MUTEX_LOCK_WRITE == mutex_lock_type)
				MUTEX_TRACE_CNTR(mutex_trc_wt_short_slp);
			else
				MUTEX_TRACE_CNTR(mutex_trc_wtim_short_slp);
#			endif
			if (-1 == sleep_spin_cnt)	/* save memory reference on fast path */
			{
				sleep_spin_cnt = mutex_spin_parms->mutex_sleep_spin_count;
				if (0 == sleep_spin_cnt)
					break;
				yields += sleep_spin_cnt;				/* start with max */
			}
			assert(!csa->now_crit);
			GTM_REL_QUANT(mutex_spin_parms->mutex_spin_sleep_mask);
		} while (--sleep_spin_cnt);
		MUTEX_DPRINT4("%d: Could not acquire WRITE %sLOCK, held by %d\n", process_id,
			(MUTEX_LOCK_WRITE == mutex_lock_type) ? "" : "IMMEDIATE ", addr->semaphore.u.parts.latch_pid);
		if (MUTEX_LOCK_WRITE_IMMEDIATE == mutex_lock_type)	/* immediate gets 1 last try which returns regardless */
			ONE_MUTEX_TRY(csa, addr, crash_count, process_id, mutex_lock_type,	/* use real lock type here */
				spins, (gtm_int64_t)-1, yields, (gtm_int64_t)-1, queue_sleeps, epoch_count, atstart);
		try_recovery = FALSE;		/* only try recovery once per MUTEXLCKALERT */
		assert(cdb_sc_nolock == status);
		time(&curr_time);
		assert(MAXUINT4 > curr_time);
		curr_time_uint4 = (uint4)curr_time;
		next_alert_uint4 = csa->critical->stuckexec.cas_time;
		if ((curr_time_uint4 > next_alert_uint4) && !IS_REPL_INST_FROZEN)
		{	/* We've waited long enough and the Instance is not frozen - might be time to send MUTEXLCKALERT */
			if (COMPSWAP_LOCK(&csa->critical->stuckexec.time_latch, next_alert_uint4, 0,
				(curr_time_uint4 + MUTEXLCKALERT_INTERVAL), 0))
			{	/* and no one else beat us to it */
				MUTEX_DPRINT3("%d: Acquired STUCKEXEC time lock, to trace %d\n", process_id, in_crit_pid);
				if (process_id == in_crit_pid)
				{	/* This is just a precaution - shouldn't ever happen and has no code to maintain gvstats */
					assert(FALSE);
					SET_CSA_NOW_CRIT_TRUE(csa);
					return (cdb_sc_normal);
				}
				if (in_crit_pid && (in_crit_pid == cnl->in_crit) && is_proc_alive(in_crit_pid, 0))
				{	/* and we're waiting on some living process */
					if (local_crit_cycle == csa->critical->crit_cycle)
					{	/* and things aren't moving */
						assert(local_crit_cycle);
						if (IS_REPL_INST_FROZEN)		/* recheck to minimize spurious reports */
							continue;
						if (0 == cnl->onln_rlbk_pid)
						{	/* not rollback - send_msg after trace less likely to lose process */
							GET_C_STACK_FROM_SCRIPT("MUTEXLCKALERT", process_id, in_crit_pid,
								csa->critical->crit_cycle);
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_MUTEXLCKALERT, 4,
									DB_LEN_STR(reg), in_crit_pid, csa->critical->crit_cycle);
							try_recovery = TRUE;	/* set off a salvage */
							continue;	/* make sure to act on it soon, likely this process */
						}
						/* If the holding PID belongs to online rollback which holds crit on database and
						 * journal pool for its entire duration, use a different message
						 */
						send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_ORLBKINPROG, 3,
								cnl->onln_rlbk_pid, DB_LEN_STR(reg));
						assert(cnl->in_crit == cnl->onln_rlbk_pid);
					}
				} else
				{	/* nobody home */
					local_crit_cycle = csa->critical->crit_cycle;
					try_recovery = TRUE;	/* set off a salvage */
					continue;		/* make sure to act on it soon, likely this process */
				}
			} else
			{	/* didn't get resource to do the MUTEXLCKALERT and procestuckexec */
				MUTEX_DPRINT2("%d: Could not acquire STUCKEXEC time lock", process_id);
			}
		}
		/* time to try for a slot on the mutex queue in order to wait for a wake up when someone releases crit */
		if (0 == local_crit_cycle)
			local_crit_cycle = csa->critical->crit_cycle;	/* sync first time waiter */
		MUTEX_TRACE_CNTR(mutex_trc_mutex_slp_fn);
		MUTEX_DPRINT2("%d: looking for mutex queue slot\n", process_id);
		if (LOCK_AVAILABLE == addr->semaphore.u.parts.latch_pid) /* there is nobody in crit */
		{	/* The above condition is an optimistic check to speed hings up by not letting a process sleep.
		 	* In an n-way SMP, there is a possibility that n processes including at least one writer might run in
		 	*  lock-step,testing the above condition almost at the same time and deciding that nobody is in crit.
		 	* This might go on until one of them grabs crit, or lock-attempts cross a threshold leading to recovery.
			*/
			if (--optimistic_attempts)
			{	/* To avoid such an undesireable scenario, we test the number of times we have run into this
				 * a situation d if too many, sleep sleep as if the latch were held.
				 */
				MUTEX_DPRINT2("%d: Nobody in crit (I) wake procs\n", process_id);
				MUTEX_TRACE_CNTR(mutex_trc_mutex_slp_fn_noslp);
				if (cdb_sc_normal == mutex_wakeup(addr, mutex_spin_parms))
					continue;
				return (cdb_sc_dbccerr);
			}
		}
		for (redo_cntr = MUTEX_MAX_WAIT_FOR_PROGRESS_CNTR; redo_cntr;)
		{	/* loop on getting a slot on the queue - every time through, if crit is available, grab it and go */
			ONE_MUTEX_TRY(csa, addr, crash_count, process_id, mutex_lock_type,	/* lock type is MUTEX_LOCK_WRITE */
				spins, (gtm_int64_t)-1, yields, (gtm_int64_t)-1, queue_sleeps, epoch_count, atstart);
			free_slot = (mutex_que_entry_ptr_t)REMQHI((que_head_ptr_t)&addr->freehead);
#			ifdef MUTEX_MSEM_WAKE
			msem_slot = free_slot;
#			endif
			if ((NULL != free_slot) && (mutex_que_entry_ptr_t)INTERLOCK_FAIL != free_slot)
			{
				free_slot->pid = process_id;
				free_slot->mutex_wake_instance = mutex_expected_wake_instance;
#				ifdef MUTEX_MSEM_WAKE
				mutex_wake_msem_ptr = &free_slot->mutex_wake_msem;
				/* this loop makes sure that the msemaphore is locked initially before the process goes to
				 * long sleep
				 */
				do
				{
					rc = MSEM_LOCKNW(mutex_wake_msem_ptr);
				} while (-1 == rc && EINTR == errno);
#				endif
				/*
				 * Significance of mutex_wake_instance field : After queueing itself, a process might go to
				 * sleep -select call in mutex_long_sleep- awaiting a wakeup message or a timeout. It is
				 * possible that a wakeup message might arrive after timeout. In this case, a later attempt
				 * at waiting for a wakeup message will falsely succeed on an old wakeup message. We use the
				 * mutex_wake_instance field (value 0 or 1) to distinguish between an old and a new wakeup
				 * message. Since at any given time there is atmost one entry in the queue for a process,
				 * the only values we need for mutex_wake_instance are 0 and 1.
				 */
				mutex_expected_wake_instance = BIN_TOGGLE(mutex_expected_wake_instance);
				hard_spin_cnt = sleep_spin_cnt = -1;
				assert(MUTEX_LOCK_WRITE == mutex_lock_type);
				do
				{
					do
					{
						if (INTERLOCK_FAIL !=
							INSQTI((que_ent_ptr_t)free_slot, (que_head_ptr_t)&addr->prochead))
						{
							queue_sleeps++;
							MUTEX_DPRINT3("%d: Inserted %d into wait queue\n", process_id,
								free_slot->pid);
							if (cdb_sc_normal
								== mutex_long_sleep(addr, csa, mutex_spin_parms))
									break;
						}
						if (-1 == hard_spin_cnt)		/* save memory reference on fast path */
						{
							hard_spin_cnt = num_additional_processors
								? mutex_spin_parms->mutex_hard_spin_count : 1;
							spins += hard_spin_cnt;		/* start with max */
						}
					} while (--hard_spin_cnt);
					if (hard_spin_cnt)
						break;
					if (-1 == sleep_spin_cnt)		/* save memory reference on fast path */
					{
						sleep_spin_cnt = mutex_spin_parms->mutex_sleep_spin_count;
						if (0 == sleep_spin_cnt)
							break;
						sleep_spin_cnt = MAX(E_4 - mutex_spin_parms->mutex_hard_spin_count, sleep_spin_cnt);
						yields += sleep_spin_cnt;	/* start with max */
					}
#					ifndef MUTEX_MSEM_WAKE
					if (wake_this_pid != process_id)
						mutex_wake_proc((sm_int_ptr_t)&wake_this_pid, wake_instance);
#					endif
					if (0 != (--sleep_spin_cnt))
						return (cdb_sc_dbccerr);	/* Too many failures */
					assert(!csa->now_crit);
					GTM_REL_QUANT(mutex_spin_parms->mutex_spin_sleep_mask);
				} while (sleep_spin_cnt);		/* actually terminated by the return three lines above */
			}
			if (sleep_spin_cnt)
			{
				redo_cntr = 0;
				break;
			}
			if ((mutex_que_entry_ptr_t)NULL == free_slot)
			{
				/* Record queue full event in db file header if applicable.  Take care not to do it for
				 * jnlpool which has no concept of a db cache.  In that case csa->hdr is NULL so use
				 * PROBE_BG_TRACE_PRO_ANY macro.
				 */
				PROBE_BG_TRACE_PRO_ANY(csa, mutex_queue_full);
				csa->probecrit_rec.p_crit_que_full++;
				MUTEX_DPRINT2("%d: Free Queue full\n", process_id);
				/* When I can't find a free slot in the queue repeatedly, it means that there is no progress
				 * in the system. A recovery attempt might be warranted in this scenario. The trick is to
				 * return cdb_sc_normal which in turn causes another spin-loop initiation (or recovery when
				 * implemented).  The objective of mutex_sleep is achieved (partially) in that sleep is
				 * done, though queueing isn't.
				 */
			}
			mutex_deadlock_check(addr, csa);
			if (redo_cntr--)
			{
				yields++;
				SLEEP_USEC(HUNDRED_MSEC, FALSE);	/* Wait .1 second or until interrupted, then try again */
				continue;
			}
		} while (redo_cntr);
	} while (TRUE);
}

enum cdb_sc mutex_unlockw(gd_region *reg, int crash_count)
{
	/* Unlock write access to the mutex at addr */

	uint4		already_clear;
	sgmnt_addrs	*csa;
        DCL_THREADGBL_ACCESS;

        SETUP_THREADGBL_ACCESS;
	csa = &FILE_INFO(reg)->s_addrs;
	if (crash_count != csa->critical->crashcnt)
		return (cdb_sc_critreset);
	assert(csa->now_crit);
	MUTEX_TEST_SIGNAL_HERE("WRTUNLCK NOW CRIT\n", FALSE);
	assert(csa->critical->semaphore.u.parts.latch_pid == process_id);
	RELEASE_SWAPLOCK(&csa->critical->semaphore);
	SET_CSA_NOW_CRIT_FALSE(csa);
	MUTEX_DPRINT2("%d: WRITE LOCK RELEASED\n", process_id);
	return (mutex_wakeup(csa->critical, NULL != csa->hdr
		? (mutex_spin_parms_ptr_t)(&csa->hdr->mutex_spin_parms)
		: (mutex_spin_parms_ptr_t)((sm_uc_ptr_t)csa->critical + JNLPOOL_CRIT_SPACE)));
}

void mutex_cleanup(gd_region *reg)
{
	sgmnt_addrs	*csa;

	/* mutex_cleanup is called after doing a rel_crit on the same area so if we still own the lock
	   it is because csa->now_crit was not in sync with our semaphore. At this point, if we own
	   the lock, go ahead and release it.
	*/
	csa = &FILE_INFO(reg)->s_addrs;
	if (COMPSWAP_UNLOCK(&csa->critical->semaphore, process_id, CMPVAL2, LOCK_AVAILABLE, 0))
	{
		MUTEX_DPRINT2("%d  mutex_cleanup : released lock\n", process_id);
	}
}

void mutex_seed_init(void)
{
	time_t mutex_seed;

	mutex_seed = time(NULL) * process_id;
	next_rand[0] = (unsigned short)(mutex_seed & ((1U << (SIZEOF(unsigned short) * 8)) - 1));
	mutex_seed >>= (SIZEOF(unsigned short) * 8);
	next_rand[1] = (unsigned short)(mutex_seed & ((1U << (SIZEOF(unsigned short) * 8)) - 1));
	mutex_seed >>= (SIZEOF(unsigned short) * 8);
	next_rand[2] = (unsigned short)(mutex_seed & ((1U << (SIZEOF(unsigned short) * 8)) - 1));
}

void mutex_salvage(gd_region *reg)
{
	sgmnt_addrs	*csa;
	int		salvage_status;
	uint4		holder_pid, onln_rlbk_pid;
	boolean_t	mutex_salvaged;
	VMS_ONLY(uint4	holder_imgcnt;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = &FILE_INFO(reg)->s_addrs;
	if (0 != (holder_pid = csa->critical->semaphore.u.parts.latch_pid))
	{
		mutex_salvaged = FALSE;
		if (holder_pid == process_id)
		{	/* We were trying to obtain a lock we already held -- very odd */
			RELEASE_SWAPLOCK(&csa->critical->semaphore);
			csa->nl->in_crit = 0;
			/* Mutex crash repaired, want to do write cache recovery, just in case.
			 * Take care not to do it for jnlpool which has no concept of a db cache.
			 * In that case csa->hdr is NULL so use PROBE_SET_TRACEABLE_VAR macro.
			 */
			PROBE_SET_TRACEABLE_VAR(csa, TRUE);
			mutex_salvaged = TRUE;
			MUTEX_DPRINT2("%d : mutex salvaged, culprit was our own process\n", process_id);
		} else if (!is_proc_alive(holder_pid, UNIX_ONLY(0) VMS_ONLY(holder_imgcnt)))
		{	/* Release the COMPSWAP lock AFTER setting csa->nl->in_crit to 0 as an assert in
			 * grab_crit (checking that csa->nl->in_crit is 0) relies on this order.
			 */
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_MUTEXFRCDTERM, 3, holder_pid, REG_LEN_STR(reg));
			csa->nl->in_crit = 0;
			/* Mutex crash repaired, want to do write cache recovery, in case previous holder of crit had set
			 * some cr->in_cw_set to a non-zero value. Not doing cache recovery could cause incorrect
			 * GTMASSERTs in PIN_CACHE_RECORD macro in t_end/tp_tend.				BYPASSOK(GTMASSERT)
			 * Take care not to do it for jnlpool which has no concept of a db cache.
			 * In that case csa->hdr is NULL so use PROBE_SET_TRACEABLE_VAR macro.
			 */
			PROBE_SET_TRACEABLE_VAR(csa, TRUE);
			COMPSWAP_UNLOCK(&csa->critical->semaphore, holder_pid, holder_imgcnt, LOCK_AVAILABLE, 0);
			mutex_salvaged = TRUE;
			/* Reset jb->blocked as well if the holder_pid had it set */
			if ((NULL != csa->jnl) && (NULL != csa->jnl->jnl_buff) && (csa->jnl->jnl_buff->blocked == holder_pid))
				csa->jnl->jnl_buff->blocked = 0;
			MUTEX_DPRINT3("%d : mutex salvaged, culprit was %d\n", process_id, holder_pid);
		} else if (!TREF(disable_sigcont))
		{
			/* The process might have been STOPPED (kill -SIGSTOP). Send SIGCONT and nudge the stopped process forward.
			 * However, skip this call in case of SENDTO_EPERM white-box test, because we do not want the intentionally
			 * stuck process to be awakened prematurely. */
			DEBUG_ONLY(if (!gtm_white_box_test_case_enabled || WBTEST_SENDTO_EPERM != gtm_white_box_test_case_number))
				continue_proc(holder_pid);
		}
		/* Record salvage event in db file header if applicable.
		 * Take care not to do it for jnlpool which has no concept of a db cache.
		 * In that case csa->hdr is NULL so check accordingly.
		 */
		assert((NULL != csa->hdr) || (csa == &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs));
		if (mutex_salvaged && (NULL != csa->hdr))
		{
			SET_TRACEABLE_VAR(csa->nl->wc_blocked, TRUE);
			BG_TRACE_PRO_ANY(csa, wcb_mutex_salvage); /* no need to use PROBE_BG_TRACE_PRO_ANY macro
								   * since we already checked for csa->hdr non-NULL.
								   */
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_mutex_salvage"),
				process_id, &csa->ti->curr_tn, DB_LEN_STR(reg));
		}
	}
}

/* Do the per process initialization of mutex stuff. This function should be invoked only once per process. The only
 * exception is the receiver server which could invoke this twice. Once through the receiver server startup command when
 * it does "jnlpool_init" and the second through the child receiver server process initialization. The second initialization
 * is needed to set the mutex structures up to correspond to the child process id (and not the parent pid). The function below
 * has to be coded to ensure that the second call nullifies any effects of the first call.
 */
void	mutex_per_process_init(void)
{
	int4	status;

	assert(process_id != mutex_per_process_init_pid);
	mutex_seed_init();
#	ifndef MUTEX_MSEM_WAKE
	if (mutex_per_process_init_pid)
	{	/* Close socket opened by the first call. But dont delete the socket file as the parent process will do that. */
		assert(FD_INVALID != mutex_sock_fd);
		if (FD_INVALID != mutex_sock_fd)
			CLOSEFILE_RESET(mutex_sock_fd, status);	/* resets "mutex_sock_fd" to FD_INVALID */
	}
	assert(FD_INVALID == mutex_sock_fd);
	mutex_sock_init();
	assert(FD_INVALID != mutex_sock_fd);
#	endif
	mutex_per_process_init_pid = process_id;
}
