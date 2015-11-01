/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* GT.M Mutex Control */

#include "mdef.h"

#include <errno.h>
#include <unistd.h>
#include "gtm_socket.h"
#include <sys/un.h>
#include <iotcp_select.h>
#include <sys/time.h>
#if defined(__sparc) || defined(__hpux) || defined(__MVS__) || defined(__linux__)
#include <limits.h>
#else
#include <sys/limits.h>
#endif
#ifdef MUTEX_MSEM_WAKE
#include <sys/mman.h>
#endif
#include "gtm_stdlib.h"

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
#include "gdsbgtr.h"
#include "mutex.h"
#include "relqueopi.h"
#include "eintr_wrappers.h"
#include "send_msg.h"
#include "is_proc_alive.h"
#include "compswap.h"
#include "gtmsecshr.h"
#include "rel_quant.h"
#include "add_inter.h"

#define QUANT_RETRY			10000
#define QUEUE_RETRY			255

#ifdef MUTEX_MSEM_WAKE
#define MUTEX_MAX_HEARTBEAT_WAIT        2 /* so that total wait for both select and msem wait will be the same */
#define MUTEX_LCKALERT_PERIOD		8
#endif

GBLREF boolean_t		mutex_salvaged, disable_sigcont;
GBLREF uint4			process_id;
GBLREF int			num_additional_processors;

#ifdef MUTEX_MSEM_WAKE
GBLREF volatile uint4           heartbeat_counter;
static msemaphore		*mutex_wake_msem_ptr = NULL;
static mutex_que_entry_ptr_t	msem_slot;
#else
GBLREF int			mutex_sock_fd;
GBLREF fd_set			mutex_wait_on_descs;
#endif

DECLARE_MUTEX_TRACE_CNTRS

DECLARE_MUTEX_TEST_SIGNAL_FLAG

static	boolean_t	woke_self;
static	boolean_t	woke_none;
static	unsigned short	next_rand[3];
static	int		optimistic_attempts;
static  mutex_lock_t	mutex_lock_type;
static	int		mutex_expected_wake_instance = 0;

static	enum cdb_sc	mutex_wakeup(mutex_struct_ptr_t addr);
void			mutex_salvage(gd_region *reg);

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
 *		enum cdb_sc mutex_lockw(reg, mutex_spin_parms, seq)
 *			Write access to mutex for region reg
 *
 *		enum cdb_sc mutex_lockwim(reg, mutex_spin_parms, seq)
 *			Write access for region reg; if cannot lock,
 *		immediately return cdb_sc_nolock
 *
 *		enum cdb_sc mutex_unlockw(reg, seq);
 *			Unlock write access for region reg
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
 *
 *		Fields may be interspersed with fillers for alignment purposes.
 */

static	void	clean_initialize(mutex_struct_ptr_t addr, int n, bool crash)
{
	mutex_que_entry_ptr_t	q_free_entry;
#ifdef MUTEX_MSEM_WAKE
	msemaphore		*status;
#endif

	error_def(ERR_TEXT);
	error_def(ERR_MUTEXERR);

	assert(n > 0);
	addr->queslots = n;
	/* Initialize the waiting process queue to be empty */
	addr->prochead.que.fl = addr->prochead.que.bl = 0;
	SET_LATCH_GLOBAL(&addr->prochead.latch, LOCK_AVAILABLE);
	/* Initialize the free queue to be empty */
	addr->freehead.que.fl = addr->freehead.que.bl = 0;
	SET_LATCH_GLOBAL(&addr->freehead.latch, LOCK_AVAILABLE);
	/* Clear the first free entry */
	q_free_entry = (mutex_que_entry_ptr_t)((sm_uc_ptr_t)&addr->freehead + sizeof(mutex_que_head));
	q_free_entry->que.fl = q_free_entry->que.bl = 0;
	q_free_entry->pid = 0;
	q_free_entry->super_crit = (void *)NULL;
	q_free_entry->mutex_wake_instance = 0;
	while (n--)
	{
#ifdef MUTEX_MSEM_WAKE
		if ((NULL == (status = msem_init(&q_free_entry->mutex_wake_msem, MSEM_LOCKED))) || ((msemaphore *)-1 == status))
			rts_error(VARLSTCNT(7) ERR_MUTEXERR, 0, ERR_TEXT, 2,
				RTS_ERROR_TEXT("Error with mutex wait memory semaphore initialization"), errno);
#endif
		if (INTERLOCK_FAIL == INSQTI((que_ent_ptr_t)q_free_entry++, (que_head_ptr_t)&addr->freehead))
			rts_error(VARLSTCNT(6) ERR_MUTEXERR, 0, ERR_TEXT, 2,
				RTS_ERROR_TEXT("Interlock instruction failure in mutex initialize"));
	}
	SET_LATCH_GLOBAL(&addr->semaphore, LOCK_AVAILABLE);
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
		    (0 != ((int)next_entry & (sizeof(mutex_que_entry) - 1))))
		{
			/*
			 * next_entry == &addr->prochead => loop is done;
			 * next_entry below queue head => queue is corrupt;
			 * next_entry above queue top => queue is corrupt;
			 * next_entry is not (sizeof queue entry)-byte
			 * aligned => queue is corrupt ...
			 * ... in all cases do a clean initialization
			 */
			clean_initialize(addr, n, crash);
			return;
		}
		/* Wake up process */
		if (next_entry->pid != process_id)
#ifdef MUTEX_MSEM_WAKE
			mutex_wake_proc(&next_entry->mutex_wake_msem);
#else
			mutex_wake_proc((sm_int_ptr_t)&next_entry->pid, next_entry->mutex_wake_instance);
#endif
	} while (TRUE);
}

static	enum cdb_sc mutex_long_sleep(mutex_struct_ptr_t addr, int lock_attempts)
{
	enum cdb_sc		status;
	boolean_t		wakeup_status;
#ifdef MUTEX_MSEM_WAKE
	uint4                   bad_heartbeat;
#else
	struct timeval		timeout;
	int			timeout_threshold;
	struct sockaddr_un	mutex_woke_me_proc;
	size_t			mutex_woke_me_proc_len;
	mutex_wake_msg_t	mutex_wake_msg[2];
	int			sel_stat;
	int			nbrecvd;
	int			timeout_intr_slpcnt;
	long			timeout_val;
#endif

	error_def(ERR_MUTEXERR);
	error_def(ERR_TEXT);

	if (LOCK_AVAILABLE == addr->semaphore.latch_pid && ++optimistic_attempts <= MUTEX_MAX_OPTIMISTIC_ATTEMPTS)
	{
		MUTEX_DPRINT2("%d: Nobody in crit (II) wake procs\n", process_id);
		MUTEX_TRACE_CNTR(mutex_trc_mutex_slp_fn_noslp);
		status = mutex_wakeup(addr);
		if ((cdb_sc_normal == status) && (woke_self || woke_none))
			return (cdb_sc_normal);
		else if (cdb_sc_dbccerr == status)
			return (cdb_sc_dbccerr);
	}
	optimistic_attempts = 0;
	do
	{
#ifdef MUTEX_MSEM_WAKE
		/* My msemaphore is already used by another process.
		 * In other words, I was woken up, but missed my wakeup call.
		 * I should return immediately.
		 */
		if (msem_slot->pid != process_id)
			wakeup_status = TRUE;
		else
		{
			bad_heartbeat = 0;
			/*
			 * the check for EINTR below is valid and should not be converted to an EINTR
			 * wrapper macro, because another condition is checked for the while loop.
			 */
			while (!(wakeup_status = (0 == msem_lock(mutex_wake_msem_ptr, 0))))
			{
				if (EINTR == errno)
				{
					if (bad_heartbeat)	/* to save memory reference and calc on fast path */
					{
						if (bad_heartbeat < heartbeat_counter)
						{
							MUTEX_DPRINT3("%d: msem sleep done, heartbeat_counter = %d\n",
								     process_id, heartbeat_counter);
							break;
						}
						MUTEX_DPRINT3("%d: msem sleep continue, heartbeat_counter = %d\n",
							      process_id, heartbeat_counter);
					} else
						bad_heartbeat = heartbeat_counter + MUTEX_MAX_HEARTBEAT_WAIT - 1;
					/* -1 since we were interrupted this time */
				} else
					rts_error(VARLSTCNT(7) ERR_MUTEXERR, 0, ERR_TEXT, 2,
						RTS_ERROR_TEXT("Error with mutex wake msem"), errno);

			}
			/* wakeup_status is set to true, if I was able to lock...somebody woke me up;
			 * wakeup_status is set to false, if I timed out and should go to recovery.
			 */
		}
#else
		do
		{
			timeout.tv_sec = MUTEX_CONST_TIMEOUT_VAL;
			timeout.tv_usec = (nrand48(next_rand) & ((1U << MUTEX_NUM_WAIT_BITS) - 1)) + 1;
			timeout_val = timeout.tv_sec * ONE_MILLION + timeout.tv_usec;
			/*
			 * Can add backoff logic here to increase the timeout
			 * as the number of attempts increase
			 */
			timeout_intr_slpcnt = MUTEX_INTR_SLPCNT;
			MUTEX_DPRINT4("%d: Sleeping for %d s %d us\n", process_id, timeout.tv_sec, timeout.tv_usec);
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
					rts_error(VARLSTCNT(5) ERR_TEXT, 2,
						RTS_ERROR_TEXT("Error with mutex select. Running in degraded mode"), errno);
				timeout_val >>= 1;
				timeout.tv_sec = timeout_val / ONE_MILLION;
				timeout.tv_usec = timeout_val % ONE_MILLION;
				MUTEX_DPRINT4("%d: Interrupted select, new timeout %d s %d us\n", process_id, timeout.tv_sec,
					timeout.tv_usec);
				/* the next line deals with the case that an interrupted select has changed mutex_wait_on_descs */
				FD_SET(mutex_sock_fd, &mutex_wait_on_descs);
				MUTEX_TRACE_CNTR(mutex_trc_slp);
			}
			if (1 == sel_stat) /* Somebody woke me up */
			{
				mutex_woke_me_proc_len = sizeof(struct sockaddr_un);
				RECVFROM_SOCK(mutex_sock_fd, (void *)&mutex_wake_msg[0], sizeof(mutex_wake_msg), 0,
					(struct sockaddr *)&mutex_woke_me_proc, (sssize_t *)&mutex_woke_me_proc_len, nbrecvd);
				if (sizeof(mutex_wake_msg) == nbrecvd) /* Drained out both old and new wake messages */
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
#endif
		/*
		 * If I was woken up and am a writer, others are blocking on
		 * me. So, I shall try to get the lock NOW
		 */
		if (wakeup_status && (MUTEX_LOCK_WRITE == mutex_lock_type))
			return (cdb_sc_normal);
		status = mutex_wakeup(addr); /* Timed out or reader. In case
					      * of reader this causes
					      * accelerated wakeup of readers
					      * in the queue */
		if (cdb_sc_dbccerr == status)
			return (cdb_sc_dbccerr);
		/* else status is cdb_sc_normal */
		if (wakeup_status || woke_self || woke_none)
			return (cdb_sc_normal);
		/*
		 * There are others above me in the queue or I missed my
		 * wakeup call. In the latter case, select or msem_lock will return
		 * immediately and there won't be further sleeps.
		 */
	} while (TRUE);
}

static	enum cdb_sc mutex_sleep(sgmnt_addrs *csa, int lock_attempts)
{
	/* Insert this process at the tail of the wait queue and hibernate */
	void			rel_quant();
	mutex_struct_ptr_t	addr;
	mutex_que_entry_ptr_t	free_slot;
	int			redo_cntr;
	int			queue_retry_counter_remq,
			        quant_retry_counter_remq,
				queue_retry_counter_insq,
				quant_retry_counter_insq;
#ifdef MUTEX_MSEM_WAKE
	int			rc;
#endif

	addr = csa->critical;
	MUTEX_TRACE_CNTR(mutex_trc_mutex_slp_fn);
	MUTEX_DPRINT2("%d: In Mutex Sleep\n", process_id);
	if (LOCK_AVAILABLE == addr->semaphore.latch_pid) /* there is nobody in crit */
	{
		/*
		 * The above condition is an optimistic check to speed
		 * things up by not letting a process sleep.
		 * In an n-way SMP, there is a possibility that n processes
		 * (atleast one writer) might run in a lock-step manner
		 * testing the above condition almost at the same time and
		 * deciding that nobody is in crit. This might go on till
		 * atleast one of them grabs crit, or lock attempts cross a
		 * threshold (leading to recovery). This is not desired. To
		 * avoid such a scenario, we test the number of times we have
		 * run into this situation and force ourselves to sleep
		 */
		if (++optimistic_attempts < MUTEX_MAX_OPTIMISTIC_ATTEMPTS)
		{
			MUTEX_DPRINT2("%d: Nobody in crit (I) wake procs\n", process_id);
			MUTEX_TRACE_CNTR(mutex_trc_mutex_slp_fn_noslp);
			return (mutex_wakeup(addr));
		}
	}
	redo_cntr = 0;
	quant_retry_counter_remq = QUANT_RETRY;
	do
	{
		queue_retry_counter_remq = QUEUE_RETRY;
		do
		{
			free_slot = (mutex_que_entry_ptr_t)REMQHI((que_head_ptr_t)&addr->freehead);
#ifdef MUTEX_MSEM_WAKE
                        msem_slot = free_slot;
#endif
			if ((mutex_que_entry_ptr_t)NULL != free_slot &&
			    (mutex_que_entry_ptr_t)INTERLOCK_FAIL != free_slot)
			{
				free_slot->pid = process_id;
				free_slot->mutex_wake_instance = mutex_expected_wake_instance;
#ifdef MUTEX_MSEM_WAKE
				mutex_wake_msem_ptr = &free_slot->mutex_wake_msem;
				/* this line makes sure that the msemaphore is locked initially
				 * before the process goes to long sleep
				 */
				MSEM_LOCK(mutex_wake_msem_ptr, MSEM_IF_NOWAIT, rc);
#endif
				/*
				 * Significance of mutex_wake_instance field :
				 * -----------------------------------------
				 * After queueing itself, a process
				 * might go to sleep (select call in
				 * mutex_long_sleep) awaiting a wakeup message
				 * or a timeout. It is possible that a wakeup
				 * message might arrive after timeout. In this
				 * case, a later attempt at waiting for a
				 * wakeup message will falsely succeed on an
				 * old wakeup message. We use the
				 * mutex_wake_instance field (value 0 or 1)
				 * to distinguish between an old and a new
				 * wakeup message. Since at any given time
				 * there is atmost one entry in the queue for
				 * a process, the only values we need for
				 * mutex_wake_instance are 0 and 1.
				 */
				mutex_expected_wake_instance = BIN_TOGGLE(mutex_expected_wake_instance);
				quant_retry_counter_insq = QUANT_RETRY;
				do
				{
					queue_retry_counter_insq = QUEUE_RETRY;
					do
					{
						if (INTERLOCK_FAIL !=
							INSQTI((que_ent_ptr_t)free_slot, (que_head_ptr_t)&addr->prochead))
						{
							MUTEX_DPRINT3("%d: Inserted %d into wait queue\n", process_id,
									free_slot->pid);
							return (mutex_long_sleep(addr, lock_attempts));
						}
					} while (--queue_retry_counter_insq);
					if (!(--quant_retry_counter_insq))
						return (cdb_sc_dbccerr); /* Too many failures */
					rel_quant();
				} while (quant_retry_counter_insq);
			} else if ((mutex_que_entry_ptr_t)NULL == free_slot)
			{
				BG_TRACE_PRO_ANY(csa, mutex_queue_full);
				MUTEX_DPRINT2("%d: Free Queue full\n", process_id);
				/* Wait a second, then try again */
				MICROSEC_SLEEP(ONE_MILLION - 1);
				if (++redo_cntr < MUTEX_MAX_WAIT_FOR_PROGRESS_CNTR)
					break;
				/*
				 * When I can't find a free slot in the queue
				 * repeatedly, it means that there is no
				 * progress in the system. A recovery attempt
				 * might be warranted in this scenario. The
				 * trick is to return cdb_sc_normal which in
				 * turn causes another spin-loop initiation (or
				 * recovery when implemented).
				 * The objective of mutex_sleep is achieved
				 * (partially) in that sleep is done, though
				 * queueing isn't.
				 */
				return (cdb_sc_normal);
			} else
			{
				/* secondary interlock failed on an attempt to
				 * remove an entry from the free queue */
				redo_cntr = 0;
			}
		} while (--queue_retry_counter_remq);
		if (redo_cntr)
			quant_retry_counter_remq = QUANT_RETRY + 1;
		else
			rel_quant();
	} while (--quant_retry_counter_remq);

	return (cdb_sc_dbccerr);
}

static	enum cdb_sc mutex_wakeup(mutex_struct_ptr_t addr)
{
	void			rel_quant();
	mutex_que_entry_ptr_t	free_entry;
	int			queue_retry_counter_remq,
				quant_retry_counter_remq,
				queue_retry_counter_insq,
				quant_retry_counter_insq;
	uint4			wake_this_pid;
	int			wake_instance;

	woke_self = FALSE;
	woke_none = TRUE;
	quant_retry_counter_remq = QUANT_RETRY;
	do
	{
		queue_retry_counter_remq = QUEUE_RETRY;
		do
		{
			free_entry = (mutex_que_entry_ptr_t)REMQHI((que_head_ptr_t)&addr->prochead);
			if ((mutex_que_entry_ptr_t)NULL != free_entry &&
			    (mutex_que_entry_ptr_t)INTERLOCK_FAIL != free_entry)
			{
				quant_retry_counter_insq = QUANT_RETRY;
				wake_this_pid = free_entry->pid;
				wake_instance = free_entry->mutex_wake_instance;
#ifdef MUTEX_MSEM_WAKE
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
				 free_entry->pid = 0;
#endif
				do
				{
					queue_retry_counter_insq = QUEUE_RETRY;
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
#ifndef MUTEX_MSEM_WAKE
								mutex_wake_proc((sm_int_ptr_t)&wake_this_pid, wake_instance);
#endif
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
					} while (--queue_retry_counter_insq);
					if (!(--quant_retry_counter_insq))
					{
#ifndef MUTEX_MSEM_WAKE
						if (wake_this_pid != process_id)
							mutex_wake_proc((sm_int_ptr_t)&wake_this_pid, wake_instance);
#endif
						/* Too many failures */
						return (cdb_sc_dbccerr);
					} else
						rel_quant();
				} while (quant_retry_counter_insq);
			} else if ((mutex_que_entry_ptr_t)NULL == free_entry)
			{
				/* Empty wait queue */
				MUTEX_DPRINT2("%d: Empty wait queue\n", process_id);
				return (cdb_sc_normal);
			} /* else secondary interlock failed */
		} while (--queue_retry_counter_remq);
		if (!(--quant_retry_counter_remq))
			return (cdb_sc_dbccerr); /* Too many queue failures */
		else
			rel_quant();
	} while (quant_retry_counter_remq);
}


void	gtm_mutex_init(gd_region *reg, int n, bool crash)
{
	if (!crash)
		clean_initialize((&FILE_INFO(reg)->s_addrs)->critical, n, crash);
	else
		crash_initialize((&FILE_INFO(reg)->s_addrs)->critical, n, crash);
	return;
}


static enum cdb_sc write_lock_spin(gd_region *reg,
			           mutex_spin_parms_ptr_t mutex_spin_parms,
				   int crash_count,
				   int attempt_recovery)
{
	int			write_sleep_spin_count, write_hard_spin_count;
	sgmnt_addrs		*csa;
	mutex_struct_ptr_t	addr;
#ifdef MUTEX_REAL_SLEEP
	int			micro_sleep_time;
#endif

	csa = &FILE_INFO(reg)->s_addrs;
	assert(!csa->now_crit);
	addr = csa->critical;
	write_sleep_spin_count = 0;
	write_hard_spin_count = 0;
	do
	{
		do
		{
			if (crash_count != addr->crashcnt)
				return (cdb_sc_critreset);
			if (GET_SWAPLOCK(&addr->semaphore))
			{
				MUTEX_DPRINT3("%d: Write %sACQUIRED\n", process_id,
					      (MUTEX_LOCK_WRITE == mutex_lock_type) ? "" : "IMMEDIATE ");
				MUTEX_TEST_SIGNAL_HERE("WRTLCK NOW CRIT\n", FALSE);
				csa->now_crit = TRUE;
				MUTEX_TEST_SIGNAL_HERE("WRTLCK SUCCESS\n", FALSE);
				return (cdb_sc_normal);
			} else if (attempt_recovery)
			{
				mutex_salvage(reg);
				attempt_recovery = FALSE;
			}
			if (!write_hard_spin_count)	/* save memory reference on fast path */
				write_hard_spin_count = num_additional_processors ? mutex_spin_parms->mutex_hard_spin_count : 1;
		} while (--write_hard_spin_count);
		/* Sleep for a very short duration */
#ifdef MUTEX_TRACE
		if (MUTEX_LOCK_WRITE == mutex_lock_type)
			MUTEX_TRACE_CNTR(mutex_trc_wt_short_slp);
		else
			MUTEX_TRACE_CNTR(mutex_trc_wtim_short_slp);
#endif
#ifdef MUTEX_REAL_SLEEP
		micro_sleep_time = (nrand48(next_rand) & mutex_spin_parms->mutex_spin_sleep_mask) + 1;
		assert(micro_sleep_time < ONE_MILLION);
		assert(FALSE == csa->now_crit);
		MICROSEC_SLEEP(micro_sleep_time);
#else
		rel_quant();
#endif
		if (!write_sleep_spin_count)	/* save memory reference on fast path */
			write_sleep_spin_count = mutex_spin_parms->mutex_sleep_spin_count;
	} while (--write_sleep_spin_count);
	MUTEX_DPRINT4("%d: Could not acquire WRITE %sLOCK, held by %d\n", process_id,
		(MUTEX_LOCK_WRITE == mutex_lock_type) ? "" : "IMMEDIATE ", addr->semaphore.latch_pid);
	return (cdb_sc_nolock);
}

static enum cdb_sc mutex_lock(gd_region *reg,
			      mutex_spin_parms_ptr_t mutex_spin_parms,
			      int crash_count,
			      int max_lock_attempts)
{
	int			lock_attempts;
	sgmnt_addrs		*csa;
	enum cdb_sc		status;
	boolean_t		alert;
#ifdef MUTEX_MSEM_WAKE
	uint4			alert_heartbeat_counter = 0;
#endif

	error_def(ERR_MUTEXLCKALERT);

	mutex_salvaged = FALSE;
	optimistic_attempts = 0;
	lock_attempts = 0;
	alert = FALSE;
	do
	{
		switch(mutex_lock_type)
		{
			case MUTEX_LOCK_WRITE :
				MUTEX_TRACE_CNTR(mutex_trc_w_atmpts);
				status = write_lock_spin(reg, mutex_spin_parms, crash_count, alert);
				break;
			case MUTEX_LOCK_WRITE_IMMEDIATE :
				return (write_lock_spin(reg, mutex_spin_parms, crash_count, FALSE));
			default :
				GTMASSERT;
		}
		if (cdb_sc_normal == status || cdb_sc_critreset == status)
			return (status);
		assert(cdb_sc_nolock == status);
#ifdef MUTEX_MSEM_WAKE
		if (0 == alert_heartbeat_counter)
			alert_heartbeat_counter = heartbeat_counter + MUTEX_LCKALERT_PERIOD;
		alert = (heartbeat_counter >= alert_heartbeat_counter);
#else
		alert = (lock_attempts >= max_lock_attempts);
#endif
		csa = &FILE_INFO(reg)->s_addrs;
		if (cdb_sc_dbccerr == mutex_sleep(csa, ++lock_attempts))
			return (cdb_sc_dbccerr);
		if (alert)
		{
			send_msg(VARLSTCNT(5) ERR_MUTEXLCKALERT, 3, DB_LEN_STR(reg),
					csa->nl->in_crit); /* Alert the admin */
			lock_attempts = 0;
#ifdef MUTEX_MSEM_WAKE
			alert_heartbeat_counter = 0;
#endif
		}
	} while (TRUE);
}

enum cdb_sc mutex_lockw(gd_region *reg, mutex_spin_parms_ptr_t mutex_spin_parms, int crash_count)
{
	MUTEX_TRACE_CNTR(mutex_trc_lockw);
	mutex_lock_type = MUTEX_LOCK_WRITE;
	return (mutex_lock(reg, mutex_spin_parms, crash_count, MUTEX_MAX_WRITE_LOCK_ATTEMPTS));
}

enum cdb_sc mutex_lockwim(gd_region *reg, mutex_spin_parms_ptr_t mutex_spin_parms, int crash_count)
{
	MUTEX_TRACE_CNTR(mutex_trc_lockwim);
	mutex_lock_type = MUTEX_LOCK_WRITE_IMMEDIATE;
	return (mutex_lock(reg, mutex_spin_parms, crash_count, 0)); /* Don't care for last argument */
}

enum cdb_sc mutex_unlockw(gd_region *reg, int crash_count)
{
	/* Unlock write access to the mutex at addr */

	uint4		already_clear;
	sgmnt_addrs	*csa;

	csa = &FILE_INFO(reg)->s_addrs;
	if (crash_count != csa->critical->crashcnt)
		return (cdb_sc_critreset);
	assert(csa->now_crit);
	MUTEX_TEST_SIGNAL_HERE("WRTUNLCK NOW CRIT\n", FALSE);
	csa->now_crit = FALSE;
	assert(csa->critical->semaphore.latch_pid == process_id);
	RELEASE_SWAPLOCK(&csa->critical->semaphore);
	MUTEX_DPRINT2("%d: WRITE LOCK RELEASED\n", process_id);
	return (mutex_wakeup(csa->critical));
}

void mutex_cleanup(gd_region *reg)
{
	sgmnt_addrs	*csa;

	/* mutex_cleanup is called after doing a rel_crit on the same area so if we still own the lock
	   it is because csa->now_crit was not in sync with our semaphore. At this point, if we own
	   the lock, go ahead and release it.
	*/
	csa = &FILE_INFO(reg)->s_addrs;
	if (compswap(&csa->critical->semaphore, process_id, LOCK_AVAILABLE))
	{
		MUTEX_DPRINT2("%d  mutex_cleanup : released lock\n", process_id);
	}
}

void mutex_seed_init(void)
{
	time_t mutex_seed;

	mutex_seed = time(NULL) * process_id;
	next_rand[0] = (unsigned short)(mutex_seed & ((1U << (sizeof(unsigned short) * 8)) - 1));
	mutex_seed >>= (sizeof(unsigned short) * 8);
	next_rand[1] = (unsigned short)(mutex_seed & ((1U << (sizeof(unsigned short) * 8)) - 1));
	mutex_seed >>= (sizeof(unsigned short) * 8);
	next_rand[2] = (unsigned short)(mutex_seed & ((1U << (sizeof(unsigned short) * 8)) - 1));
}

void mutex_salvage(gd_region *reg)
{
	sgmnt_addrs	*csa;
	int		salvage_status;
	uint4		holder;

	error_def(ERR_MUTEXFRCDTERM);

	csa = &FILE_INFO(reg)->s_addrs;
	if (0 != (holder = csa->critical->semaphore.latch_pid))
	{
		if (holder == process_id)
		{	/* We were trying to obtain a lock we already held -- very odd */
			RELEASE_SWAPLOCK(&csa->critical->semaphore);
			csa->nl->in_crit = 0;
			mutex_salvaged = TRUE;
			MUTEX_DPRINT2("%d : mutex salvaged, culprit was our own process\n", process_id);
		} else if (!is_proc_alive(holder, 0))
		{
			compswap(&csa->critical->semaphore, holder, LOCK_AVAILABLE);
			csa->nl->in_crit = 0;
			mutex_salvaged = TRUE;
			send_msg(VARLSTCNT(5) ERR_MUTEXFRCDTERM, 3, holder, REG_LEN_STR(reg));
			MUTEX_DPRINT3("%d : mutex salvaged, culprit was %d\n", process_id, holder);
		} else if (FALSE == disable_sigcont)
		{
			/* The process might have been STOPPED (kill -SIGSTOP). Send SIGCONT and nudge the stopped
			 * process forward */
			continue_proc(holder);
		}
	}
}
