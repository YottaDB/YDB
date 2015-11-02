/****************************************************************
 *								*
 *	Copyright 2007 Fidelity Information Services, Inc	*
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

GBLDEF	cache_rec_ptr_t		get_space_fail_cr;	/* gbldefed to be accessible in a pro core */
GBLDEF	int4			*get_space_fail_array;	/* gbldefed to be accessilbe in a pro core */
GBLDEF	int4			get_space_fail_arridx;	/* gbldefed to be accessilbe in a pro core */

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gd_region		*gv_cur_region;
GBLREF	boolean_t		gtm_environment_init;
GBLREF	int			num_additional_processors;
GBLREF	uint4			process_id;
GBLREF	volatile int4		fast_lock_count;

/* go after a specific number of buffers or a particular buffer */
/* not called if UNTARGETED_MSYNC and MM mode */
bool	wcs_get_space(gd_region *reg, int needed, cache_rec_ptr_t cr)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t        cnl;
	cache_que_head_ptr_t	q0, base;
	int4			n, save_errno = 0, k, i, dummy_errno, max_count, count;
	int			maxspins, retries, spins;
	uint4			lcnt, size, to_wait, to_msg;
	int4			wcs_active_lvl[UNIX_GETSPACEWAIT];

	error_def(ERR_DBFILERR);
	error_def(ERR_WAITDSKSPACE);

	assert((0 != needed) || (NULL != cr));
	get_space_fail_arridx = 0;
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	if (FALSE == csa->now_crit)
	{
		assert(0 != needed);	/* if needed == 0, then we should be in crit */
		for (lcnt = DIVIDE_ROUND_UP(needed, csd->n_wrt_per_flu);  0 < lcnt;  lcnt--)
			JNL_ENSURE_OPEN_WCS_WTSTART(csa, reg, 0, dummy_errno);
					/* a macro that ensure jnl is open, invokes wcs_wtstart() and checks for errors etc. */
		return TRUE;
	}
	UNTARGETED_MSYNC_ONLY(assert(dba_mm != csd->acc_meth);)
	csd->flush_trigger = MAX(csd->flush_trigger - MAX(csd->flush_trigger / STEP_FACTOR, 1), MIN_FLUSH_TRIGGER(csd->n_bts));
	cnl = csa->nl;
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
							send_msg(VARLSTCNT(7) ERR_WAITDSKSPACE, 4,
								process_id, to_wait, DB_LEN_STR(reg), save_errno);
							gtm_putmsg(VARLSTCNT(7) ERR_WAITDSKSPACE, 4,
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
		if (dba_bg == csd->acc_meth)		/* Determine queue base to use */
			base = &csa->acc_meth.bg.cache_state->cacheq_active;
		else
			base = &csa->acc_meth.mm.mmblk_state->mmblkq_active;
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
						wcs_active_lvl[lcnt] = cnl->wcs_active_lvl;
						get_space_fail_arridx = lcnt;
						max_count = ROUND_UP(cnl->wcs_active_lvl, csd->n_wrt_per_flu);
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
						 *   sleeping we do an unconditional sleep (only if cr is dirty).
						 */
						if (!cr->dirty)
							return TRUE;
						else
							wcs_sleep(lcnt);
						BG_TRACE_PRO_ANY(csa, spcfc_buffer_flush_loop);
					}
					if (0 == cr->dirty)
						return TRUE;
					assert(FALSE);			/* We have failed */
					get_space_fail_cr = cr;
					get_space_fail_array = &wcs_active_lvl[0];
					if (gtm_environment_init)
						gtm_fork_n_core();	/* take a snapshot in case running in-house */
					return FALSE;
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
					performCASLatchCheck(&base->latch, LOOP_CNT_SEND_WAKEUP);
			}
		}
		--fast_lock_count;
		assert(0 <= fast_lock_count);
		if (0 == cr->dirty)
			return TRUE;
	}
	if (ENOSPC == save_errno)
		rts_error(VARLSTCNT(7) ERR_WAITDSKSPACE, 4, process_id, to_wait, DB_LEN_STR(reg), save_errno);
	else
		assert(FALSE);
	get_space_fail_cr = cr;
	get_space_fail_array = &wcs_active_lvl[0];
	if (gtm_environment_init)
		gtm_fork_n_core();	/* take a snapshot in case running in-house */
	return FALSE;
}
