/****************************************************************
 *								*
 *	Copyright 2008, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_facility.h"
#include "gdsroot.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "sleep_cnt.h"
#include "gdsbgtr.h"
#include "memcoherency.h"

/* Include prototypes */
#include "wcs_phase2_commit_wait.h"
#include "gt_timer.h"
#include "wcs_sleep.h"
#include "rel_quant.h"
#include "send_msg.h"
#include "gtm_c_stack_trace.h"
#include "wbox_test_init.h"

error_def(ERR_COMMITWAITPID);
error_def(ERR_COMMITWAITSTUCK);

#define	SEND_COMMITWAITPID_GET_STACK_IF_NEEDED(BLOCKING_PID, STUCK_CNT, CR, CSA)						\
{																\
	GBLREF	uint4	process_id;												\
																\
	if (BLOCKING_PID)													\
	{															\
		STUCK_CNT++;													\
		GET_C_STACK_FROM_SCRIPT("COMMITWAITPID", process_id, BLOCKING_PID, STUCK_CNT);					\
		send_msg(VARLSTCNT(8) ERR_COMMITWAITPID, 6, process_id, 1, BLOCKING_PID, CR->blk, DB_LEN_STR(CSA->region));	\
	}															\
}

/* take C-stack trace of the process doing the phase2 commits at half the entire wait. We do this only while waiting
 * for a particular cache record
 */
#define GET_STACK_AT_HALF_WAIT_IF_NEEDED(BLOCKING_PID, STUCK_CNT)						\
{														\
	GBLREF	uint4	process_id;										\
														\
	if (BLOCKING_PID && (process_id != BLOCKING_PID))							\
	{													\
		STUCK_CNT++;											\
		GET_C_STACK_FROM_SCRIPT("COMMITWAITPID_HALF_WAIT", process_id, BLOCKING_PID, STUCK_CNT);	\
	}													\
}

GBLREF	uint4		process_id;
GBLREF	int		process_exiting;
#ifdef DEBUG
GBLREF	boolean_t	in_mu_rndwn_file;
#endif

#ifdef UNIX
GBLREF	volatile uint4		heartbeat_counter;
GBLREF	volatile int4		timer_stack_count;
#endif

/* if cr == NULL, wait a maximum of 1 minute for ALL processes actively in bg_update_phase2 to finish.
 * if cr != NULL, wait a maximum of 1 minute for the particular cache-record to be done with phase2 commit.
 *
 * This routine is invoked inside and outside of crit. If we hold crit, then we are guaranteed that cr->in_tend
 * cannot get reset to a non-zero value different from what we saw when we started waiting. This is not
 * guaranteed if we dont hold crit. In that case, we wait until cr->in_tend changes in value (zero or non-zero).
 *
 * Returns : TRUE if waiting event completed before timeout, FALSE otherwise
 */
boolean_t	wcs_phase2_commit_wait(sgmnt_addrs *csa, cache_rec_ptr_t cr)
{
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t        cnl;
	uint4			lcnt, blocking_pid, start_in_tend, spincnt, maxspincnt, start_heartbeat, heartbeat_delta;
	int4			value;
	boolean_t		was_crit;
	boolean_t		use_heartbeat;
	block_id		blk;
#	ifdef VMS
	uint4			heartbeat_counter = 0;	/* dummy variable to make compiler happy */
#	endif
	int4			index, crarray_size, crarray_index;
	cache_rec_ptr_t		cr_lo, cr_top, curcr;
	phase2_wait_trace_t	crarray[MAX_PHASE2_WAIT_CR_TRACE_SIZE];
#	ifdef DEBUG
	uint4			incrit_pid, phase2_commit_half_wait;
	int4			waitarray[1024];
	int4			waitarray_size;
	boolean_t		half_time = FALSE;
#	endif
	static uint4		stuck_cnt = 0; /* stuck_cnt signifies the number of times the same process
						has called gtmstuckexec for the same condition*/
	DEBUG_ONLY(cr_lo = cr_top = NULL;)
	crarray_size = SIZEOF(crarray) / SIZEOF(crarray[0]);
	DEBUG_ONLY(waitarray_size = SIZEOF(waitarray) / SIZEOF(waitarray[0]);)

	assert(!in_mu_rndwn_file);
	csd = csa->hdr;
	/* To avoid unnecessary time spent waiting, we would like to do rel_quants instead of wcs_sleep. But this means
	 * we need to have some other scheme for limiting the total time slept. We use the heartbeat scheme which currently
	 * is available only in Unix. Every 8 seconds or so, the heartbeat timer increments a counter. But there are two
	 * cases where heartbeat_timer will not pop:
	 * (a) if we are in the process of exiting (through a call to cancel_timer(0) which cancels all active timers)
	 * (b) if we are are already in timer_handler. This is possible if the flush timer pops and we end up invoking
	 *     wcs_clean_dbsync->wcs_flu->wcs_phase2_commit_wait. But since the heartbeat timer cannot pop as long as
	 *     timer_in_handler is TRUE (which it will be until at least we exit this function), we cannot use the heartbeat
	 *     scheme in this case as well.
	 * Therefore, if heartbeat timer is available and currently active, then use rel_quants. If not, use wcs_sleep.
	 * We have found that doing rel_quants (instead of sleeps) causes huge CPU usage in Tru64 even if the default spincnt is
	 * set to 0 and ALL processes are only waiting for one process to finish its phase2 commit. Therefore we choose
	 * the sleep approach for Tru64. Choosing a spincnt of 0 would choose the sleep approach (versus rel_quant).
	 */
#	if (defined(UNIX) && !defined(__osf__))
	use_heartbeat = (!process_exiting && csd->wcs_phase2_commit_wait_spincnt && (1 > timer_stack_count));
#	else
	use_heartbeat = FALSE;
#	endif
	DEBUG_ONLY(phase2_commit_half_wait = use_heartbeat ? (PHASE2_COMMIT_WAIT_HTBT >> 1) : (PHASE2_COMMIT_WAIT >> 1);)
	if (use_heartbeat)
	{
		maxspincnt = csd->wcs_phase2_commit_wait_spincnt;
		assert(maxspincnt);
		if (!maxspincnt)
			maxspincnt = WCS_PHASE2_COMMIT_DEFAULT_SPINCNT;
		start_heartbeat = heartbeat_counter;
	}
	assert(dba_bg == csd->acc_meth);
	if (dba_bg != csd->acc_meth)	/* in pro, be safe and return */
		return TRUE;
	cnl = csa->nl;
	was_crit = csa->now_crit;
	assert((NULL != cr) || was_crit);
	if (NULL != cr)
	{
		start_in_tend = cr->in_tend;
		/* Normally we should never find ourselves holding the lock on the cache-record we are waiting for. There is
		 * one exception though. And that is if we had encountered an error in the middle of phase1 or phase2 of the
		 * commit and ended up invoking "secshr_db_clnup" to finish the transaction for us. It is possible that we
		 * then proceeded with the next transaction doing a "t_qread" without any process invoking "wcs_recover"
		 * (possible only if they did a "grab_crit") until then. In that case, we could have one or more cache-records
		 * with non-zero value of cr->in_tend identical to our process_id. Since we will fix these cache-records
		 * while grabbing crit (which we have to before doing validation in t_end/tp_tend), it is safe to assume
		 * this block is not being touched for now and return right away. But this exception is possible only if
		 * we dont already hold crit (i.e. called from "t_qread"). In addition, errors in the midst of commit are
		 * possible only if we have enabled white-box testing. Assert accordingly.
		 */
		/* we better not deadlock wait for ourself */
		if (!was_crit && (process_id == start_in_tend))
		{
			assert(gtm_white_box_test_case_enabled);
			return TRUE;
		}
		if (process_id == start_in_tend)
			GTMASSERT;	/* should not deadlock on our self */
		if (!start_in_tend)
			return TRUE;
	} else
	{	/* initialize the beginning and the end of cache-records to be used later (only in case of cr == NULL) */
		cr_lo = ((cache_rec_ptr_t)csa->acc_meth.bg.cache_state->cache_array) + csd->bt_buckets;
		cr_top = cr_lo + csd->n_bts;
	}
	/* Spin & sleep/yield alternately for the phase2 commit to complete */
	for (spincnt = 0, lcnt = 0; ; spincnt++)
	{
		SHM_READ_MEMORY_BARRIER; /* read memory barrier done to minimize time spent spinning waiting for value to change */
		if (NULL == cr)
		{
			value = cnl->wcs_phase2_commit_pidcnt;
			if (!value)
				return TRUE;
		} else
		{	/* If we dont hold crit and are sleep looping waiting for cr->in_tend to become 0, it is
			 * theoretically possible (though very remote) that every one of the 1000s of iterations we look
			 * at the cache-record, cr->in_tend is set to the same pid even though the block could have
			 * been updated as part of multiple transactions. But we could have stopped the wait the moment the
			 * same buffer gets updated for the next transaction (even if by the same pid). To recognize that
			 * we note down the current db tn at the start of the wait and check if the block header tn
			 * throughout the wait gets higher than this. If so, we return right away even though cr->in_tend
			 * is non-zero. But since this comparison is done outside of crit it is possible that the block
			 * header tn could be temporarily GREATER than the db tn because of concurrent updates AND because
			 * an update to the 8-byte transaction number is not necessarily atomic AND because the block's tn
			 * that we read could be a mish-mash of low-order and high-order bytes taken from BEFORE and AFTER
			 * an update. Doing less than checks with these bad values is considered risky as a false return
			 * means a GTMASSERT in "t_end" or "tp_tend" in the PIN_CACHE_RECORD macro. Since this situation is
			 * almost an impossibility in practice, we handle this by returning FALSE after timing out and
			 * requiring the caller (t_qread) to restart. Eventually we will get crit (in the final retry) where
			 * we are guaranteed not to end up in this situation.
			 */
			value = cr->in_tend;
			if (value != start_in_tend)
			{
				assert(!was_crit || !value);
				return TRUE;
			}
			if (!was_crit && cnl->wc_blocked)
			{	/* Some other process could be doing cache-recovery at this point and if it takes more than
				 * a minute, we will time out for no reason. No point proceeding with this transaction
				 * anyway as we are bound to restart. Do that right away. Caller knows to restart.
				 */
				return FALSE;
			}
		}
		if (use_heartbeat)
		{
			if (spincnt < maxspincnt)
				continue;
			assert(spincnt == maxspincnt);
			heartbeat_delta = heartbeat_counter - start_heartbeat;
		}
		spincnt = 0;
		lcnt++;
		DEBUG_ONLY(waitarray[lcnt % waitarray_size] = value;)
		if (NULL != cr)
		{
			if (was_crit)
			{
				BG_TRACE_PRO_ANY(csa, phase2_commit_wait_sleep_in_crit);
			} else
			{
				BG_TRACE_PRO_ANY(csa, phase2_commit_wait_sleep_no_crit);
			}
		} else
		{
			BG_TRACE_PRO_ANY(csa, phase2_commit_wait_pidcnt);
		}
		if (use_heartbeat)
		{
			if (PHASE2_COMMIT_WAIT_HTBT < heartbeat_delta)
				break;
			DEBUG_ONLY(half_time = (phase2_commit_half_wait == heartbeat_delta));
			rel_quant();
		} else
		{
			if (lcnt >= PHASE2_COMMIT_WAIT)
				break;
			DEBUG_ONLY(half_time = (phase2_commit_half_wait == lcnt));
			wcs_sleep(PHASE2_COMMIT_SLEEP);
		}
#		ifdef DEBUG
		if (half_time)
		{
			if (NULL != cr)
			{
				blocking_pid = cr->in_tend; /* Get a more recent value */
				GET_STACK_AT_HALF_WAIT_IF_NEEDED(blocking_pid, stuck_cnt);
			} else
			{
				assert((NULL != cr_lo) && (cr_lo < cr_top));
				for (curcr = cr_lo; curcr < cr_top; curcr++)
				{
					blocking_pid = curcr->in_tend;
					GET_STACK_AT_HALF_WAIT_IF_NEEDED(blocking_pid, stuck_cnt);
				}
			}
		}
#		endif

	}
	if (NULL == cr)
	{	/* This is the case where we wait for all the phase2 commits to complete. Note down the cache records that
		 * are still not done with the commits. Since there can be multiple cache records held by the same PID, note
		 * down one cache record for each representative PID. We don't expect the list of distinct PIDs to be large.
		 * In any case, note down only as many as we can
		 */
		crarray_index = 0;
		for (curcr = cr_lo; curcr < cr_top;  curcr++)
		{
			blocking_pid = curcr->in_tend;
			/* In rare cases, wcs_phase2_commit_wait could be invoked from bg_update_phase1 (via bt_put->wcs_get_space)
			 * when bg_update_phase1 has already pinned a few cache records (with our PID). We don't want to note down
			 * such cache records and hence the (blocking_pid != process_id) check below
			 */
			if (blocking_pid && (blocking_pid != process_id))
			{
				/* go through the book-keeping array to see if we have already noted down this PID. We don't
				 * expect many processes to be in the phase2 commit section concurrently. So, in most cases,
				 * we won't scan the array more than once
				 */
				for (index = 0; index < crarray_index; ++index)
					if (crarray[index].blocking_pid == blocking_pid)
						break;
				if (index == crarray_index)
				{	/* cache-record with distinct PID */
					assert(crarray_size >= crarray_index);
					if (crarray_size <= crarray_index)
						break;
					crarray[crarray_index].blocking_pid = blocking_pid;
					crarray[crarray_index].cr = curcr;
					crarray_index++;
				}
			}
		}
		/* Issue COMMITWAITPID and get c-stack trace (if possible) for all the distinct PID noted down above */
		for (index = 0; index < crarray_index; index++)
		{	/* It is possible that cr->in_tend changed since the time we added it to the crarray array.
			 * Account for this by rechecking.
			 */
			curcr = crarray[index].cr;
			blocking_pid = curcr->in_tend;
			SEND_COMMITWAITPID_GET_STACK_IF_NEEDED(blocking_pid, stuck_cnt, curcr, csa);
		}
	} else
	{	/* This is the case where we wait for a particular cache-record. Take the c-stack of the PID that is still
		 * holding this cr
		 */
		blocking_pid = cr->in_tend;
		SEND_COMMITWAITPID_GET_STACK_IF_NEEDED(blocking_pid, stuck_cnt, cr, csa);
	}
	DEBUG_ONLY(incrit_pid = cnl->in_crit;)
	send_msg(VARLSTCNT(7) ERR_COMMITWAITSTUCK, 5, process_id, 1, cnl->wcs_phase2_commit_pidcnt, DB_LEN_STR(csa->region));
	BG_TRACE_PRO_ANY(csa, wcb_phase2_commit_wait);
	/* If called from wcs_recover(), we dont want to assert(FALSE) as it is possible (in case of STOP/IDs) that
	 * cnl->wcs_phase2_commit_pidcnt is non-zero even though there is no process in phase2 of commit. In this case
	 * wcs_recover will call wcs_verify which will clear the flag unconditionally and proceed with normal activity.
	 * So should not assert. If the caller is wcs_recover, then we expect cnl->wc_blocked so be non-zero. Assert
	 * that. If we are called from wcs_flu via ONLINE ROLLBACK, then wc_blocked will NOT be set. Instead, wcs_flu
	 * will return with a failure status back to ROLLBACK which will invoke wcs_recover and that will take care of
	 * resetting cnl->wcs_phase2_commit_pidcnt. But, ONLINE ROLLBACK called in a crash situation is done only with
	 * whitebox test cases. So, assert accordingly.
	 */
	assert(cnl->wc_blocked || (WBTEST_CRASH_SHUTDOWN_EXPECTED == gtm_white_box_test_case_number));
	return FALSE;
}
