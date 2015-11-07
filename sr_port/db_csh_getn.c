/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <signal.h>	/* needed for VSIG_ATOMIC_T */

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "interlock.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab.h"		/* needed for cws_insert.h */
#include "hashtab_int4.h"	/* needed for tp.h and cws_insert.h */
#include "tp.h"
#include "gdsbgtr.h"
#include "min_max.h"
#include "sleep_cnt.h"
#include "send_msg.h"
#include "relqop.h"
#include "is_proc_alive.h"
#include "cache.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "cws_insert.h"
#include "wcs_sleep.h"
#include "wcs_get_space.h"
#include "wcs_timer_start.h"
#include "add_inter.h"
#include "wbox_test_init.h"
#include "have_crit.h"
#include "memcoherency.h"
#include "gtm_c_stack_trace.h"
#include "anticipatory_freeze.h"

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF uint4		process_id;
GBLREF uint4		image_count;
GBLREF unsigned int	t_tries;
GBLREF uint4		dollar_tlevel;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF boolean_t        mu_reorg_process;
#ifdef UNIX
GBLREF uint4 		update_trans;
GBLREF jnlpool_addrs	jnlpool;
#endif

#define	TRACE_AND_SLEEP(ocnt)				\
{							\
	if (1 == ocnt)					\
	{						\
		BG_TRACE_PRO(db_csh_getn_rip_wait);	\
		first_r_epid = latest_r_epid;		\
	}						\
	wcs_sleep(ocnt);				\
}

error_def(ERR_BUFRDTIMEOUT);
error_def(ERR_INVALIDRIP);

cache_rec_ptr_t	db_csh_getn(block_id block)
{
	cache_rec_ptr_t		hdr, q0, start_cr, cr;
	bt_rec_ptr_t		bt;
	unsigned int		lcnt, ocnt;
	int			rip, max_ent, pass1, pass2, pass3;
	int4			flsh_trigger;
	uint4			first_r_epid, latest_r_epid;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	srch_blk_status		*tp_srch_status;
	ht_ent_int4		*tabent;
	boolean_t		dont_flush_buff;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = cs_addrs;
	csd = csa->hdr;
	assert(dba_mm != csd->acc_meth);
	assert(csa->now_crit);
	assert(csa == &FILE_INFO(gv_cur_region)->s_addrs);
	max_ent = csd->n_bts;
	cr = (cache_rec_ptr_t)GDS_REL2ABS(csa->nl->cur_lru_cache_rec_off);
	hdr = csa->acc_meth.bg.cache_state->cache_array + (block % csd->bt_buckets);
	start_cr = csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets;
	pass1 = max_ent;	/* skip referred or dirty or read-into cache records */
	pass2 = 2 * max_ent;	/* skip referred cache records */
	pass3 = 3 * max_ent;	/* skip nothing */
	dont_flush_buff = gv_cur_region->read_only UNIX_ONLY(||
		(!(dollar_tlevel ? sgm_info_ptr->update_trans : update_trans) && IS_REPL_INST_FROZEN)
	);
	INCR_DB_CSH_COUNTER(csa, n_db_csh_getns, 1);
	DEFER_INTERRUPTS(INTRPT_IN_DB_CSH_GETN);
	for (lcnt = 0;  ; lcnt++)
	{
		if (lcnt > pass3)
		{
			BG_TRACE_PRO(wc_blocked_db_csh_getn_loopexceed);
			assert(FALSE);
			break;
		}
		cr++;
		if (cr == start_cr + max_ent)
			cr = start_cr;
		VMS_ONLY(
			if ((lcnt == pass1) || (lcnt == pass2))
				wcs_wtfini(gv_cur_region);
		)
		if (cr->refer && (lcnt < pass2))
		{	/* in passes 1 & 2, set refer to FALSE and skip; in the third pass attempt reuse even if TRUE == refer */
			cr->refer = FALSE;
			continue;
		}
		if (cr->in_cw_set || cr->in_tend)
		{	/* some process already has this pinned for reading and/or updating. skip it. */
			cr->refer = TRUE;
			continue;
		}
		if (CDB_STAGNATE <= t_tries || mu_reorg_process)
		{
			/* Prevent stepping on self when crit for entire transaction.
			 * This is done by looking up in sgm_info_ptr->blk_in_use and cw_stagnate for presence of the block.
			 * The following two hashtable lookups are not similar, since in TP, sgm_info_ptr->blks_in_use
			 * 	is updated to the latest cw_stagnate list of blocks only in "tp_hist".
			 * Also note that the lookup in sgm_info_ptr->blks_in_use reuses blocks that don't have cse's.
			 * This is to allow big-read TP transactions which may use up more than the available global buffers.
			 * There is one issue here in that a block that has been only read till now may be stepped upon here
			 *	but may later be needed for update. It is handled by updating the block's corresponding
			 *	entry in the set of histories (sgm_info_ptr->first_tp_hist[index] structure) to hold the
			 *	"cr" and "cycle" of the t_qread done for the block when it was intended to be changed for the
			 *	first time within the transaction since otherwise the transaction would restart due to a
			 *	cdb_sc_lostcr status. Note that "tn" (read_tn of the block) in the first_tp_hist will still
			 *	remain the "tn" when the block was first read within this transaction to ensure the block
			 *	hasn't been modified since the start of the transaction. Once we intend on changing the
			 *	block i.e. srch_blk_status->cse is non-NULL, we ensure in the code below not to step on it.
			 *	["tp_hist" is the routine that updates the "cr", "cycle" and "tn" of the block].
			 * Note that usually in a transaction the first_tp_hist[] structure holds the "cr", "cycle", and "tn"
			 *	of the first t_qread of the block within that transaction. The above is the only exception.
			 * Also note that for blocks in cw_stagnate (i.e. current TP mini-action), we don't reuse any of
			 *	them even if they don't have a cse. This is to ensure that the current action doesn't
			 *	encounter a restart due to cdb_sc_lostcr in "tp_hist" even in the fourth-retry.
			 */
			tp_srch_status = NULL;
			if (dollar_tlevel && (NULL != (tabent = lookup_hashtab_int4(sgm_info_ptr->blks_in_use, (uint4 *)&cr->blk)))
					&& (tp_srch_status = (srch_blk_status *)tabent->value) && (tp_srch_status->cse))
			{	/* this process is already using the block - skip it */
				cr->refer = TRUE;
				continue;
			}
			if (NULL != lookup_hashtab_int4(&cw_stagnate, (uint4 *)&cr->blk))
			{	/* this process is already using the block for the current gvcst_search - skip it */
				cr->refer = TRUE;
				continue;
			}
			if (NULL != tp_srch_status)
			{	/* About to reuse a buffer that is part of the read-set of the current TP transaction.
				 * Reset clue as otherwise the next global reference of that global will use an outofdate clue.
				 * Even though tp_srch_status is available after the sgm_info_ptr->blks_in_use hashtable check,
				 * we dont want to reset the clue in case the cw_stagnate hashtable check causes the same cr
				 * to be skipped from reuse. Hence the placement of this reset logic AFTER the cw_stagnate check.
				 */
				tp_srch_status->blk_target->clue.end = 0;
			}
		}
		if (cr->dirty)
		{	/* Note that in Unix, it is possible that we see a stale value of cr->dirty (possible if a
			 * concurrent "wcs_wtstart" has reset dirty to 0 but that update did not reach us yet). In this
			 * case the call to "wcs_get_space" below will do the necessary memory barrier instructions
			 * (through calls to "aswp") which will allow us to see the non-stale value of cr->dirty.
			 *
			 * It is also possible that cr->dirty is non-zero but < cr->flushed_dirty_tn. In this case, wcs_get_space
			 * done below will return FALSE forcing a cache-rebuild which will fix this situation.
			 *
			 * In VMS, another process cannot be concurrently resetting cr->dirty to 0 as the resetting routine
			 * is "wcs_wtfini" which is executed in crit which another process cannot be in as we are in crit now.
			 */
			if (dont_flush_buff)
				continue;
			if (lcnt < pass1)
			{
				if (!csa->timer && (csa->nl->wcs_timers < 1))
					wcs_timer_start(gv_cur_region, FALSE);
				continue;
			}
			BG_TRACE_PRO(db_csh_getn_flush_dirty);
			if (FALSE == wcs_get_space(gv_cur_region, 0, cr))
			{	/* failed to flush it out - force a rebuild */
				BG_TRACE_PRO(wc_blocked_db_csh_getn_wcsstarvewrt);
				assert(csa->nl->wc_blocked); /* only reason we currently know why wcs_get_space could fail */
				assert(gtm_white_box_test_case_enabled);
				break;
			}
			assert(0 == cr->dirty);
		}
		UNIX_ONLY(
			/* the cache-record is not free for reuse until the write-latch value becomes LATCH_CLEAR.
			 * In VMS, resetting the write-latch value occurs in "wcs_wtfini" which is in CRIT, we are fine.
			 * In Unix, this resetting is done by "wcs_wtstart" which is out-of-crit. Therefore, we need to
			 * 	wait for this value to be LATCH_CLEAR before reusing this cache-record.
			 * Note that we are examining the write-latch-value without holding the interlock. It is ok to do
			 * 	this because the only two routines that modify the latch value are "bg_update" and
			 * 	"wcs_wtstart". The former cannot be concurrently executing because we are in crit.
			 * 	The latter will not update the latch value unless this cache-record is dirty. But in this
			 * 	case we would have most likely gone through the if (cr->dirty) check above. Most likely
			 * 	because there is one rare possibility where a concurrent "wcs_wtstart" has set cr->dirty
			 * 	to 0 but not yet cleared the latch. In that case we wait for the latch to be cleared.
			 * 	In all other cases, nobody is modifying the latch since when we got crit and therefore
			 * 	it is safe to observe the value of the latch without holding the interlock.
			 */
			if (LATCH_CLEAR != WRITE_LATCH_VAL(cr))
			{	/* possible if a concurrent "wcs_wtstart" has set cr->dirty to 0 but not yet
				 * cleared the latch. this should be very rare though.
				 */
				if (lcnt < pass2)
					continue; /* try to find some other cache-record to reuse until the 3rd pass */
				for (ocnt = 1; (MAXWRTLATCHWAIT >= ocnt) && (LATCH_CLEAR != WRITE_LATCH_VAL(cr)); ocnt++)
					wcs_sleep(SLEEP_WRTLATCHWAIT);	/* since it is a short lock, sleep the minimum */
				if (MAXWRTLATCHWAIT <= ocnt)
				{
					BG_TRACE_PRO(db_csh_getn_wrt_latch_stuck);
					assert(FALSE);
					continue;
				}
			}
		)
		/* Note that before setting up a buffer for the requested block, we should make sure the cache-record's
		 * 	read_in_progress is set. This is so that noone else in t_qread gets access to this empty buffer.
		 * By setting up a buffer, it is meant assigning cr->blk in addition to inserting the cr in the blkques
		 * 	through "shuffqth" below.
		 * Note that "t_qread" has special code to handle read_in_progress */
		LOCK_BUFF_FOR_READ(cr, rip);
		if (0 != rip)
		{
			if (lcnt < pass2)
			{	/* someone is reading into this cache record. leave it for two passes.
				 * this is because if somebody is reading it, it is most likely to be referred to very soon.
				 * if we replace this, we will definitely be causing a restart for the reader.
				 * instead of that, see if some other cache record fits in for us.
				 */
				RELEASE_BUFF_READ_LOCK(cr);
				continue;
			}
			for (ocnt = 1; 0 != rip && BUF_OWNER_STUCK >= ocnt; ocnt++)
			{
				RELEASE_BUFF_READ_LOCK(cr);
				/* The owner has been unable to complete the read - check for some things before going to sleep.
				 * Since cr->r_epid can be changing concurrently, take a local copy before using it below,
				 * particularly before calling is_proc_alive as we dont want to call it with a 0 r_epid.
				 */
				latest_r_epid = cr->r_epid;
				if (cr->read_in_progress < -1)
				{
					BG_TRACE_PRO(db_csh_getn_out_of_design);  /* outside of design; clear to known state */
					send_msg(VARLSTCNT(4) ERR_INVALIDRIP, 2, DB_LEN_STR(gv_cur_region));
					assert(cr->r_epid == 0);
					cr->r_epid = 0;
					INTERLOCK_INIT(cr);
				} else  if (0 != latest_r_epid)
				{
					if (is_proc_alive(latest_r_epid, cr->image_count))
					{
#						ifdef DEBUG
						if ((BUF_OWNER_STUCK / 2) == ocnt)
							GET_C_STACK_FROM_SCRIPT("BUFRDTIMEOUT", process_id, latest_r_epid, ONCE);
#						endif
						TRACE_AND_SLEEP(ocnt);
					} else
					{
						cr->r_epid = 0;
						INTERLOCK_INIT(cr);	/* Process gone, release that process's lock */
					}
				} else
				{
					TRACE_AND_SLEEP(ocnt);
				}
				LOCK_BUFF_FOR_READ(cr, rip);
			}
			if ((BUF_OWNER_STUCK < ocnt) && (0 != rip))
			{
				BG_TRACE_PRO(db_csh_getn_buf_owner_stuck);
				if (0 != latest_r_epid)
				{
					if (first_r_epid != latest_r_epid)
						GTMASSERT;
					GET_C_STACK_FROM_SCRIPT("BUFRDTIMEOUT", process_id, latest_r_epid,
								DEBUG_ONLY(TWICE) PRO_ONLY(ONCE));
					RELEASE_BUFF_READ_LOCK(cr);
					send_msg(VARLSTCNT(8) ERR_BUFRDTIMEOUT, 6, process_id,
						 cr->blk, cr, first_r_epid, DB_LEN_STR(gv_cur_region));
					continue;
				}
				cr->r_epid = 0;
				INTERLOCK_INIT(cr);
				LOCK_BUFF_FOR_READ(cr, rip);
				assert(0 == rip); 	/* Since holding crit, we expect to get lock */
				if (0 != rip)
					continue;
				/* We successfully obtained the lock so can fall out of this block */
			}
		}
		assert(0 == rip);
		/* no other process "owns" the block */
		if (CDB_STAGNATE <= t_tries || mu_reorg_process)
		{	/* this should probably use cr->in_cw_set with a condition handler to cleanup */
			CWS_INSERT(block);
		}
		assert(LATCH_CLEAR == WRITE_LATCH_VAL(cr));
		/* got a block - set it up */
		assert(0 == cr->epid);
		assert(0 == cr->r_epid);
		cr->r_epid = process_id;	/* establish ownership */
		cr->image_count = image_count;
		cr->blk = block;
		/* We want cr->read_in_progress to be locked BEFORE cr->cycle is incremented. t_qread relies on this order.
		 * Enforce this order with a write memory barrier. Not doing so might cause the incremented cr->cycle to be
		 * seen by another process even though it sees the unlocked state of cr->read_in_progress. This could cause
		 * t_qread to incorrectly return with an uptodate cr->cycle even though the buffer is still being read in
		 * from disk and this could cause db integ errors as validation (in t_end/tp_tend which relies on cr->cycle)
		 * will detect no problems even though there is one. Note this memory barrier is still needed even though
		 * there is a memory barrier connotation in the LOCK_BUFF_FOR_READ() macro above. LOCK_BUFF_FOR_READ() does
		 * a read type memory barrier whereas here, we need a write barrier.
		 */
		SHM_WRITE_MEMORY_BARRIER;
		cr->cycle++;
		cr->jnl_addr = 0;
		cr->refer = TRUE;
		if (cr->bt_index != 0)
		{
			bt = (bt_rec_ptr_t)GDS_REL2ABS(cr->bt_index);
			bt->cache_index = CR_NOTVALID;
			cr->bt_index = 0;
		}
		q0 = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + cr->blkque.fl);
		shuffqth((que_ent_ptr_t)q0, (que_ent_ptr_t)hdr);
		assert(0 == cr->dirty);
		csa->nl->cur_lru_cache_rec_off = GDS_ABS2REL(cr);
		if (lcnt > pass1)
			csa->nl->cache_hits = 0;
		csa->nl->cache_hits++;
		if (csa->nl->cache_hits > csd->n_bts)
		{
			flsh_trigger = csd->flush_trigger;
			csd->flush_trigger = MIN(flsh_trigger + MAX(flsh_trigger / STEP_FACTOR, 1), MAX_FLUSH_TRIGGER(csd->n_bts));
			csa->nl->cache_hits = 0;
		}
		INCR_DB_CSH_COUNTER(csa, n_db_csh_getn_lcnt, lcnt);
		ENABLE_INTERRUPTS(INTRPT_IN_DB_CSH_GETN);
		return cr;
	}
	/* force a recover */
	INCR_DB_CSH_COUNTER(csa, n_db_csh_getn_lcnt, lcnt);
	csa->nl->cur_lru_cache_rec_off = GDS_ABS2REL(cr);
	ENABLE_INTERRUPTS(INTRPT_IN_DB_CSH_GETN);
	return (cache_rec_ptr_t)CR_NOTVALID;
}
