/****************************************************************
 *								*
 * Copyright (c) 2008-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "is_proc_alive.h"

error_def(ERR_COMMITWAITPID);
error_def(ERR_COMMITWAITSTUCK);

#define	SEND_COMMITWAITPID_GET_STACK_IF_NEEDED(BLOCKING_PID, STUCK_CNT, CR, CSA)				\
{														\
	GBLREF	uint4	process_id;										\
														\
	if (BLOCKING_PID)											\
	{													\
		STUCK_CNT++;											\
		GET_C_STACK_FROM_SCRIPT("COMMITWAITPID", process_id, BLOCKING_PID, STUCK_CNT);			\
		send_msg_csa(CSA_ARG(CSA) VARLSTCNT(8) ERR_COMMITWAITPID, 6,					\
			process_id, 1, BLOCKING_PID, CR->blk, DB_LEN_STR(CSA->region));				\
	}													\
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
#ifdef DEBUG
GBLREF	boolean_t	in_mu_rndwn_file;
#endif

#define	PROC_ALIVE_CHECK_FACTOR	32	/* Do "is_proc_alive" check 32 times during the total wait period */

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
	uint4			lcnt, lcnt_isprcalv_freq, lcnt_isprcalv_next, blocking_pid, start_in_tend;
	int4			value;
	boolean_t		is_alive, was_crit;
	boolean_t		timedout;
	block_id		blk;
	int4			index, crarray_index;
	cache_rec_ptr_t		cr_lo, cr_top, curcr;
	phase2_wait_trace_t	crarray[MAX_PHASE2_WAIT_CR_TRACE_SIZE];
#	ifdef DEBUG
	uint4			incrit_pid, phase2_commit_half_wait;
	int4			waitarray[1024];
	int4			waitarray_size;
	boolean_t		half_time = FALSE;
#	endif
	static uint4		stuck_cnt = 0;	/* stuck_cnt signifies the number of times the same process
						 * has called gtmstuckexec for the same condition.
						 */

	DEBUG_ONLY(cr_lo = cr_top = NULL;)
	DEBUG_ONLY(waitarray_size = SIZEOF(waitarray) / SIZEOF(waitarray[0]);)

	assert(!in_mu_rndwn_file);
	csd = csa->hdr;
	DEBUG_ONLY(phase2_commit_half_wait = (PHASE2_COMMIT_WAIT / 2));
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
		assertpro(process_id != start_in_tend);	/* should not deadlock on our self */
		if (!start_in_tend)
			return TRUE;
	} else
	{	/* initialize the beginning and the end of cache-records to be used later (only in case of cr == NULL) */
		cr_lo = ((cache_rec_ptr_t)csa->acc_meth.bg.cache_state->cache_array) + csd->bt_buckets;
		cr_top = cr_lo + csd->n_bts;
	}
	/* Check/Sleep alternately for the phase2 commit to complete */
	lcnt_isprcalv_freq = PHASE2_COMMIT_WAIT / PROC_ALIVE_CHECK_FACTOR;
	lcnt_isprcalv_next = lcnt_isprcalv_freq - 1;
	for (lcnt = 0; ; )
	{
		SHM_READ_MEMORY_BARRIER; /* read memory barrier done to minimize time spent spinning waiting for value to change */
		if (NULL == cr)
		{
			value = cnl->wcs_phase2_commit_pidcnt;
			if (!value)
				return TRUE;
			if (lcnt == lcnt_isprcalv_next)
			{	/* Do "is_proc_alive" check. This section is very similar to the "NULL == cr" section
				 * at the end of this module in terms of book-keeping array maintenance.
				 */
				crarray_index = 0;
				for (curcr = cr_lo; curcr < cr_top;  curcr++)
				{
					blocking_pid = curcr->in_tend;
					if (!blocking_pid || (blocking_pid == process_id))
						continue;
					/* If we do not hold crit, the existence of one dead pid is enough for us to know we
					 * cannot return TRUE (because we are waiting for all phase2 commits to finish and one
					 * dead pid means all commits will never complete on its own) so return FALSE right away
					 * that way caller can invoke "wcs_recover" and try to fix the situation.
					 * If we hold crit though, we cannot return FALSE right away in this situation. Only if
					 * we examine all non-zero "cr->in_tend" entries and confirm all of them are dead can
					 * we return FALSE. If at least one process is still alive, we have to wait for the
					 * timeout period (1-minute or so) before returning FALSE.
					 * We use "crarray" to hold the list of alive pids in the !was_crit case and to hold
					 * the list of dead pids in the was_crit case.
					 */
					for (index = 0; index < crarray_index; ++index)
						if (crarray[index].blocking_pid == blocking_pid)
							break;
					if (index == crarray_index)
					{	/* cache-record with PID different from what we have seen till now */
						is_alive = is_proc_alive(blocking_pid, 0);
						if (!is_alive && !was_crit)
							return FALSE;	/* Process is not alive. We can return
									 * right away with failure.
									 */
						if (is_alive && was_crit)
						{	/* We found one pid that is still alive and has phase2 commit in
							 * progress. Stop the search of the cache-array to find if all
							 * phase2 commit pids are dead. We will anyways have to continue
							 * waiting (for this alive pid to finish its phase2 commit).
							 */
							break;
						}
						/* Process is alive (if "!was_crit") or dead (if "was_crit"). Add it to array to
						 * avoid "is_proc_alive" check on other "cr"s which point to this same pid.
						 */
						assert(ARRAYSIZE(crarray) >= crarray_index);
						if (ARRAYSIZE(crarray) > crarray_index)
						{
							crarray[crarray_index].blocking_pid = blocking_pid;
							crarray[crarray_index].cr = curcr;
							crarray_index++;
						}
					}
				}
				if (was_crit && crarray_index && (curcr == cr_top))
				{	/* We hold crit and found at least one dead pid and found no alive pids in phase2 commit.
					 * No need to wait any more. Return FALSE right away. Caller will invoke "wcs_recover"
					 * to fix the situation.
					 */
					return FALSE;
				}
				lcnt_isprcalv_next += lcnt_isprcalv_freq;
			}
		} else
		{
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
			if (lcnt == lcnt_isprcalv_next)
			{	/* Do "is_proc_alive" check */
				if (!is_proc_alive(value, 0))
					return FALSE;	/* Process is not alive. We can return right away with failure. */
				lcnt_isprcalv_next += lcnt_isprcalv_freq;
			}
		}
		lcnt++;
		DEBUG_ONLY(waitarray[lcnt % waitarray_size] = value;)
		if (NULL != cr)
		{
			if (was_crit)
			{
				BG_TRACE_PRO_ANY(csa, phase2_commit_wait_sleep_in_crit);
			} else
				BG_TRACE_PRO_ANY(csa, phase2_commit_wait_sleep_no_crit);
		} else
			BG_TRACE_PRO_ANY(csa, phase2_commit_wait_pidcnt);
		if (lcnt >= PHASE2_COMMIT_WAIT)
			break;
		DEBUG_ONLY(half_time = (phase2_commit_half_wait == lcnt));
		wcs_sleep(lcnt);
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
		 * In any case, note down only as many as we have space allocated.
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
					assert(ARRAYSIZE(crarray) >= crarray_index);
					if (ARRAYSIZE(crarray) <= crarray_index)
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
	{	/* This is the case where we wait for a particular cache-record.
		 * Take the c-stack of the PID that is still holding this cr.
		 */
		blocking_pid = cr->in_tend;
		SEND_COMMITWAITPID_GET_STACK_IF_NEEDED(blocking_pid, stuck_cnt, cr, csa);
	}
	DEBUG_ONLY(incrit_pid = cnl->in_crit;)
	send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_COMMITWAITSTUCK, 5, process_id,
		1, cnl->wcs_phase2_commit_pidcnt, DB_LEN_STR(csa->region));
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
