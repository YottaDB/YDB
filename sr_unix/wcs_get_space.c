/****************************************************************
 *								*
 *	Copyright 2007, 2013 Fidelity Information Services, Inc	*
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
#include "gdsfhead.h"
#include "filestruct.h"
#include "interlock.h"
#include "jnl.h"
#include "sleep_cnt.h"
#include "gdsbgtr.h"
#include "wbox_test_init.h"

/* Include prototypes */
#include "send_msg.h"
#include "wcs_get_space.h"
#include "gtmmsg.h"
#include "gt_timer.h"
#include "wcs_sleep.h"
#include "relqop.h"
#include "error.h"		/* for gtm_fork_n_core() prototype */
#include "rel_quant.h"
#include "performcaslatchcheck.h"
#include "wcs_phase2_commit_wait.h"
#include "wcs_recover.h"
#include "gtm_c_stack_trace.h"

GBLDEF	cache_rec_ptr_t		get_space_fail_cr;	/* gbldefed to be accessible in a pro core */
GBLDEF	wcs_conflict_trace_t	*get_space_fail_array;	/* gbldefed to be accessilbe in a pro core */
GBLDEF	int4			get_space_fail_arridx;	/* gbldefed to be accessilbe in a pro core */

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_region		*gv_cur_region;	/* needed for the JNL_ENSURE_OPEN_WCS_WTSTART macro */
GBLREF	int			num_additional_processors;
GBLREF	uint4			process_id;
GBLREF	volatile int4		fast_lock_count;

error_def(ERR_DBFILERR);
error_def(ERR_WAITDSKSPACE);
error_def(ERR_GBLOFLOW);

#define	WCS_CONFLICT_TRACE_ARRAYSIZE	64
#define	LCNT_INTERVAL			DIVIDE_ROUND_UP(UNIX_GETSPACEWAIT, WCS_CONFLICT_TRACE_ARRAYSIZE)

#define WCS_GET_SPACE_RETURN_FAIL(TRACEARRAY, CR)					\
{											\
	assert(FALSE);			/* We have failed */				\
	get_space_fail_cr = CR;								\
	get_space_fail_array = TRACEARRAY;						\
	if (TREF(gtm_environment_init))							\
		gtm_fork_n_core();	/* take a snapshot in case running in-house */	\
	return FALSE;									\
}

#define GET_IO_LATCH_PID(CSA)		(CSA->jnl ? CSA->jnl->jnl_buff->io_in_prog_latch.u.parts.latch_pid : -1)
#define GET_FSYNC_LATCH_PID(CSA)	(CSA->jnl ? CSA->jnl->jnl_buff->fsync_in_prog_latch.u.parts.latch_pid : -1)

#define INVOKE_C_STACK_APPROPRIATE(CR, CSA, STUCK_CNT)										\
{																\
	int4	io_latch_pid, fsync_latch_pid;											\
																\
	if (CR->epid)														\
	{															\
		GET_C_STACK_FROM_SCRIPT("WCS_GET_SPACE_RETURN_FAIL_CR", process_id, CR->epid, STUCK_CNT);			\
	}															\
	if (0 < (io_latch_pid = GET_IO_LATCH_PID(CSA)))										\
	{															\
		GET_C_STACK_FROM_SCRIPT("WCS_GET_SPACE_RETURN_FAIL_IO_PROG", process_id, io_latch_pid, STUCK_CNT);		\
	}															\
	if (0 < (fsync_latch_pid = GET_FSYNC_LATCH_PID(CSA)))									\
	{															\
		GET_C_STACK_FROM_SCRIPT("WCS_GET_SPACE_RETURN_FAIL_FSYNC_PROG", process_id, fsync_latch_pid, STUCK_CNT);	\
	} 															\
}																\

/* go after a specific number of buffers or a particular buffer */
bool	wcs_get_space(gd_region *reg, int needed, cache_rec_ptr_t cr)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t        cnl;
	cache_que_head_ptr_t	q0, base;
	int4			n, save_errno = 0, k, i, dummy_errno, max_count, count;
	int			maxspins, retries, spins;
	uint4			lcnt, size, to_wait, to_msg, this_idx;
	wcs_conflict_trace_t	wcs_conflict_trace[WCS_CONFLICT_TRACE_ARRAYSIZE];
	cache_rec		cr_contents;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((0 != needed) || (NULL != cr));
	get_space_fail_arridx = 0;
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	assert(dba_bg == csd->acc_meth);
	assert((0 == needed) || ((DB_CSH_RDPOOL_SZ <= needed) && (needed <= csd->n_bts)));
	if (FALSE == csa->now_crit)
	{
		assert(0 != needed);	/* if needed == 0, then we should be in crit */
		for (lcnt = DIVIDE_ROUND_UP(needed, csd->n_wrt_per_flu);  0 < lcnt;  lcnt--)
			JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, 0, dummy_errno);
					/* a macro that ensure jnl is open, invokes wcs_wtstart() and checks for errors etc. */
		return TRUE;
	}
	csd->flush_trigger = MAX(csd->flush_trigger - MAX(csd->flush_trigger / STEP_FACTOR, 1), MIN_FLUSH_TRIGGER(csd->n_bts));
	/* Routine actually serves two purposes:
	 *	1 - Free up required number of buffers or
	 *	2 - Free up a specific buffer
	 * Do a different kind of loop depending on which is our current calling.
	 */
	if (0 != needed)
	{
		BG_TRACE_ANY(csa, bufct_buffer_flush);
		for (lcnt = 1; (cnl->wc_in_free < needed) && (BUF_OWNER_STUCK > lcnt); ++lcnt)
		{
			JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, needed, save_errno);
			if (cnl->wc_in_free < needed)
			{
				if ((ENOSPC == save_errno) && (csa->hdr->wait_disk_space > 0))
				{
					/* not enough disk space to flush the buffers to regain them
					 * so wait for it to become available,
					 * and if it takes too long, just
					 * quit. Unfortunately, quitting would
					 * invoke the recovery logic which
					 * should be of no help to this
					 * situation. Then what?
					 */
					lcnt = BUF_OWNER_STUCK;
					to_wait = cs_data->wait_disk_space;
					to_msg = (to_wait / 8) ? (to_wait / 8) : 1; /* output error message around 8 times */
					while ((0 < to_wait) && (ENOSPC == save_errno))
					{
						if ((to_wait == cs_data->wait_disk_space)
							|| (0 == to_wait % to_msg))
						{
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_WAITDSKSPACE, 4,
								process_id, to_wait, DB_LEN_STR(reg), save_errno);
							gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_WAITDSKSPACE, 4,
								process_id, to_wait, DB_LEN_STR(reg), save_errno);
						}
						hiber_start(1000);
						to_wait--;
						JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, needed, save_errno);
						if (cnl->wc_in_free >= needed)
							break;
					}
				}
				wcs_sleep(lcnt);
			} else
				return TRUE;
			BG_TRACE_ANY(csa, bufct_buffer_flush_loop);
		}
		if (cnl->wc_in_free >= needed)
			return TRUE;
	} else
	{	/* Wait for a specific buffer to be flushed. We attempt to speed this along by shuffling the entry
		 * we want to the front of the queue before we call routines to do some writing.
		 * Formerly we used to wait for this buffer to be flushed irrespective of its position in the active queue.
		 * We keep this code commented just in case this needs to be resurrected in the future.
		 */
#		ifdef old_code
		BG_TRACE_ANY(csa, spcfc_buffer_flush);
		for (lcnt = 1; (0 != cr->dirty) && (BUF_OWNER_STUCK > lcnt); ++lcnt)
		{
			for (; 0 != cr->dirty && 0 != csa->acc_meth.bg.cache_state->cacheq_active.fl;)
				JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, 0, save_errno);
			if (0 != cr->dirty)
				wcs_sleep(lcnt);
			else
				return TRUE;
			BG_TRACE_ANY(csa, spcfc_buffer_flush_loop);
		}
		if (0 == cr->dirty)
			return TRUE;
#		endif
		assert(csa->now_crit);		/* must be crit to play with queues when not the writer */
		BG_TRACE_PRO_ANY(csa, spcfc_buffer_flush);
		++fast_lock_count;			/* Disable wcs_stale for duration */
		base = &csa->acc_meth.bg.cache_state->cacheq_active;
		/* If another process is concurrently finishing up phase2 of commit, wait for that to complete first. */
		if (cr->in_tend && !wcs_phase2_commit_wait(csa, cr))
			return FALSE;	/* assumption is that caller will set wc_blocked and trigger cache recovery */
		maxspins = num_additional_processors ? MAX_LOCK_SPINS(LOCK_SPINS, num_additional_processors) : 1;
		for (retries = LOCK_TRIES - 1; retries > 0 ; retries--)
		{
			for (spins = maxspins; spins > 0 ; spins--)
			{
				if (GET_SWAPLOCK(&base->latch)) /* Lock queue to prevent interference */
				{
					if (0 != cr->state_que.fl)
					{	/* If it is still in the active queue, then insert it at the head of the queue */
						csa->wbuf_dqd++;
						q0 = (cache_que_head_ptr_t)((sm_uc_ptr_t)&cr->state_que + cr->state_que.fl);
						shuffqth((que_ent_ptr_t)q0, (que_ent_ptr_t)base);
						csa->wbuf_dqd--;
						VERIFY_QUEUE(base);
					}
					/* release the queue header lock so that the writers can proceed */
					RELEASE_SWAPLOCK(&base->latch);
					--fast_lock_count;
					assert(0 <= fast_lock_count);
					/* Fire off a writer to write it out. Another writer may grab our cache
					 * record so we have to be willing to wait for him to flush it.
					 * Flush this one buffer the first time through.
					 * If this didn't work, flush normal amount next time in the loop.
					 */
					JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, 1, save_errno);
					for (lcnt = 1; (0 != cr->dirty) && (UNIX_GETSPACEWAIT > lcnt); ++lcnt)
					{
						if (0 == (lcnt % LCNT_INTERVAL))
						{
							this_idx = (lcnt / LCNT_INTERVAL);
							assert(this_idx < WCS_CONFLICT_TRACE_ARRAYSIZE);
							wcs_conflict_trace[this_idx].wcs_active_lvl = cnl->wcs_active_lvl;
							wcs_conflict_trace[this_idx].io_in_prog_pid = GET_IO_LATCH_PID(csa);
							wcs_conflict_trace[this_idx].fsync_in_prog_pid = GET_FSYNC_LATCH_PID(csa);
						}
						get_space_fail_arridx = lcnt;
						max_count = ROUND_UP(cnl->wcs_active_lvl, csd->n_wrt_per_flu);
						/* Check if cache recovery is needed (could be set by another process in
						 * secshr_db_clnup finishing off a phase2 commit). If so, no point invoking
						 * wcs_wtstart as it will return right away. Instead return FALSE so
						 * cache-recovery can be triggered by the caller.
						 */
						if (cnl->wc_blocked)
						{
							assert(gtm_white_box_test_case_enabled);
							return FALSE;
						}
						/* loop till the active queue is exhausted */
						for (count = 0; 0 != cr->dirty && 0 != cnl->wcs_active_lvl &&
							     max_count > count; count++)
						{
							BG_TRACE_PRO_ANY(csa, spcfc_buffer_flush_retries);
							JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, 0, save_errno);
						}
						/* Usually we want to sleep only if we need to wait on someone else
						 * i.e. (i) if we are waiting for another process' fsync to complete
						 *		We have seen jnl_fsync() to take more than a minute.
						 *		Hence we wait for a max. of 2 mins (UNIX_GETSPACEWAIT).
						 *     (ii) if some concurrent writer has taken this cache-record out.
						 *    (iii) if someone else is holding the io_in_prog lock.
						 * Right now we know of only one case where there is no point in waiting
						 *   which is if the cache-record is out of the active queue and is dirty.
						 * But since that is quite rare and we don't lose much in that case by
						 *   sleeping we do an unconditional sleep (only if cr is dirty).	{BYPASSOK}
						 */
						if (!cr->dirty)
							return TRUE;
						else
						{
							DEBUG_ONLY(cr_contents = *cr;)
							/* Assert that if the cache-record is dirty, it better be in the
							 * active queue or be in the process of getting flushed by a concurrent
							 * writer or phase2 of the commit is in progress. If none of this is
							 * true, it should have become non-dirty by now even though we found it
							 * dirty a few lines above. Note that the cache-record could be in the
							 * process of being released by a concurrent writer; This is done by
							 * resetting 3 fields cr->epid, cr->dirty, cr->interlock; Since the write
							 * interlock is the last field to be released, check that BEFORE dirty.
							 */
							assert(cr_contents.state_que.fl || cr_contents.epid || cnl->in_wtstart
								|| cr_contents.in_tend
								|| (LATCH_CLEAR != WRITE_LATCH_VAL(&cr_contents))
								|| !cr_contents.dirty);
							wcs_sleep(lcnt);
						}
						BG_TRACE_PRO_ANY(csa, spcfc_buffer_flush_loop);
					}
					if (0 == cr->dirty)
						return TRUE;
					INVOKE_C_STACK_APPROPRIATE(cr, csa, 1);
					WCS_GET_SPACE_RETURN_FAIL(wcs_conflict_trace, cr);
				} else
				{	/* buffer was locked */
					if (0 == cr->dirty)
					{
						BG_TRACE_ANY(csa, spcfc_buffer_flushed_during_lockwait);
						--fast_lock_count;
						assert(0 <= fast_lock_count);
						return TRUE;
					}
				}
			}
			if (retries & 0x3)	/* On all but every 4th pass, do a simple rel_quant */
				rel_quant();	/* Release processor to holder of lock (hopefully) */
			else
			{	/* On every 4th pass, we bide for awhile */
				wcs_sleep(LOCK_SLEEP);
				/* If near end of loop, see if target is dead and/or wake it up */
				if (RETRY_CASLATCH_CUTOFF == retries)
					performCASLatchCheck(&base->latch, TRUE);
			}
		}
		--fast_lock_count;
		assert(0 <= fast_lock_count);
		if (0 == cr->dirty)
			return TRUE;
	}
	if (ENOSPC == save_errno)
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_WAITDSKSPACE, 4, process_id, to_wait, DB_LEN_STR(reg), save_errno);
	else
		assert(FALSE);
	INVOKE_C_STACK_APPROPRIATE(cr, csa, 2);
	WCS_GET_SPACE_RETURN_FAIL(wcs_conflict_trace, cr);
}
