/****************************************************************
 *								*
 *	Copyright 2008 Fidelity Information Services, Inc	*
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

GBLREF	uint4		process_id;
GBLREF	boolean_t	mu_rndwn_file_dbjnl_flush;
GBLREF	boolean_t	gtm_white_box_test_case_enabled;
GBLREF	int		process_exiting;

#ifdef UNIX
GBLREF	volatile uint4	heartbeat_counter;
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
	cache_rec_ptr_t		crarray[512], cr_lo, cr_top, curcr;
#	ifdef DEBUG
	uint4			incrit_pid;
	int4			waitarray[1024];
	int4			waitarray_size;
#	endif

	error_def(ERR_COMMITWAITPID);
	error_def(ERR_COMMITWAITSTUCK);

	crarray_size = sizeof(crarray) / sizeof(crarray[0]);
	DEBUG_ONLY(waitarray_size = sizeof(waitarray) / sizeof(waitarray[0]);)

	assert(!mu_rndwn_file_dbjnl_flush);	/* caller should have avoided calling us if it was mupip rundown */
	csd = csa->hdr;
	/* To avoid unnecessary time spent waiting, we would like to do rel_quants instead of wcs_sleep. But this means
	 * we need to have some other scheme for limiting the total time slept. We use the heartbeat scheme which currently
	 * is available only in Unix. Every 8 seconds or so, the heartbeat timer increments a counter. But this timer
	 * could have been cancelled if we are in the process of exiting (through a call to cancel_timer(0)).
	 * Therefore, if heartbeat timer is available and currently active, then use rel_quants. If not, use wcs_sleep.
	 * We have found that doing rel_quants (instead of sleeps) causes huge CPU usage in Tru64 even if the default spincnt is
	 * set to 0 and ALL processes are only waiting for one process to finish its phase2 commit. Therefore we choose
	 * the sleep approach for Tru64. Choosing a spincnt of 0 would choose the sleep approach (versus rel_quant).
	 */
#	if (defined(UNIX) && !defined(__osf__))
	use_heartbeat = !process_exiting && csd->wcs_phase2_commit_wait_spincnt;
#	else
	use_heartbeat = FALSE;
#	endif
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
			if (!was_crit && csd->wc_blocked)
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
			rel_quant();
		} else
		{
			if (lcnt >= PHASE2_COMMIT_WAIT)
				break;
			wcs_sleep(PHASE2_COMMIT_SLEEP);
		}
	}
	/* Note down those cache-records that are still not done with commits. Note only as many as we can. */
	cr_lo = ((cache_rec_ptr_t)csa->acc_meth.bg.cache_state->cache_array) + csd->bt_buckets;
	cr_top = cr_lo + csd->n_bts;
	crarray_index = 0;
	for (curcr = cr_lo; curcr < cr_top;  curcr++)
	{
		blocking_pid = curcr->in_tend;
		if (blocking_pid)
		{
			assert(blocking_pid != process_id);
			crarray[crarray_index++] = curcr;
		}
		assert(crarray_size >= crarray_index);
		if (crarray_size <= crarray_index)
			break;
	}
	for (index = 0; index < crarray_index; index++)
	{	/* It is possible that cr->in_tend changed since the time we added it to the crarray array.
		 * Account for this by rechecking.
		 */
		if (crarray[index] == cr)
		{
			blk = crarray[index]->blk;
			blocking_pid = crarray[index]->in_tend;
		} else
			blocking_pid = 0;
		if (blocking_pid)
			send_msg(VARLSTCNT(8) ERR_COMMITWAITPID, 6, process_id, 1, blocking_pid, blk, DB_LEN_STR(csa->region));
	}
	DEBUG_ONLY(incrit_pid = cnl->in_crit;)
	send_msg(VARLSTCNT(7) ERR_COMMITWAITSTUCK, 5, process_id, 1, cnl->wcs_phase2_commit_pidcnt, DB_LEN_STR(csa->region));
	BG_TRACE_PRO_ANY(csa, wcb_phase2_commit_wait);
	/* If called from wcs_recover(), we dont want to assert(FALSE) as it is possible (in case of STOP/IDs) that
	 * cnl->wcs_phase2_commit_pidcnt is non-zero even though there is no process in phase2 of commit. In this case
	 * wcs_recover will call wcs_verify which will clear the flag unconditionally and proceed with normal activity.
	 * So should not assert. If the caller is wcs_recover, then we expect csd->wc_blocked so be non-zero. Assert that.
	 */
	assert(csd->wc_blocked);
	return FALSE;
}
