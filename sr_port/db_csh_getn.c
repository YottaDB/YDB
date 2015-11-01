/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

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
#include "hashtab.h"		/* needed for tp.h, cws_insert.h */
#include "buddy_list.h"		/* needed for tp.h */
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

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF uint4		process_id;
GBLREF uint4		image_count;
GBLREF unsigned int	t_tries;
GBLREF hashtab		*cw_stagnate;
GBLREF short		dollar_tlevel;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF boolean_t        mu_reorg_process;

cache_rec_ptr_t	db_csh_getn(block_id block)
{
	cache_rec_ptr_t		hdr, q0, start_cr, cr;
	bt_rec_ptr_t		bt;
	unsigned int		lcnt, ocnt;
	int			rip, max_ent, pass1, pass2, pass3;
	int4			flsh_trigger;
	uint4			r_epid, dummy;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	srch_blk_status		*tp_srch_status;

	error_def(ERR_BUFRDTIMEOUT);
	error_def(ERR_INVALIDRIP);

	csa = cs_addrs;
	csd = csa->hdr;
	assert(csa->now_crit);
	assert(csa == &FILE_INFO(gv_cur_region)->s_addrs);
	max_ent = csd->n_bts;
	cr = (cache_rec_ptr_t)GDS_REL2ABS(csa->nl->cur_lru_cache_rec_off);
	hdr = csa->acc_meth.bg.cache_state->cache_array + (block % csd->bt_buckets);
	start_cr = csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets;
	pass1 = max_ent;	/* skip referred or dirty or read-into cache records */
	pass2 = 2 * max_ent;	/* skip referred cache records */
	pass3 = 3 * max_ent;	/* skip nothing */
	INCR_DB_CSH_COUNTER(csa, n_db_csh_getns, 1);
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
		if (TRUE == cr->refer && lcnt < pass2)
		{	/* in passes 1 & 2, set refer to FALSE and skip; in the third pass attempt reuse even if TRUE == refer */
			cr->refer = FALSE;
			continue;
		}
		if (TRUE == cr->in_cw_set)
		{	/* this process already owns it - skip it */
			cr->refer = TRUE;
			continue;
		}
		if (CDB_STAGNATE <= t_tries || mu_reorg_process)
		{
			/* Prevent stepping on self when crit for entire transaction.
			 * This is done by looking up in sgm_info_ptr->blk_in_use and cw_stagnate for presence of the block.
			 * The following two hashtable lookups are not similar, since in TP, sgm_info_ptr->blks_in_use
			 * 	is updated to the latest cw_stagnate list of blocks only in tp_hist().
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
			 *	block i.e. srch_blk_status->ptr is non-NULL, we ensure in the code below not to step on it.
			 *	[tp_hist() is the routine that updates the "cr", "cycle" and "tn" of the block].
			 * Note that usually in a transaction the first_tp_hist[] structure holds the "cr", "cycle", and "tn"
			 *	of the first t_qread of the block within that transaction. The above is the only exception.
			 * Also note that for blocks in cw_stagnate (i.e. current TP mini-action), we don't reuse any of
			 *	them even if they don't have a cse. This is to ensure that the current action doesn't
			 *	encounter a restart due to cdb_sc_lostcr in tp_hist() even in the fourth-retry.
			 */
			if (dollar_tlevel
				&& (tp_srch_status =
					(srch_blk_status *)lookup_hashtab_ent(sgm_info_ptr->blks_in_use, (void *)cr->blk, &dummy))
				&& tp_srch_status->ptr)
			{	/* this process is already using the block - skip it */
				cr->refer = TRUE;
				continue;
			}
			if (NULL != lookup_hashtab_ent(cw_stagnate, (void *)cr->blk, &dummy))
			{
				cr->refer = TRUE;
				continue;
			}
		}
		if (0 != cr->dirty)
		{
			if (cr->dirty >= cr->flushed_dirty_tn) /* the block needs to be flushed */
			{	/* cr->dirty == cr->flushed_dirty_tn can happen in a very small window wherein the block has
				 * has actually been flushed by wcs_wtstart, but it hasn't yet cleared the dirty and epid field.
				 * In this case, it is actually better to go ahead and reuse this cache-record but that would
				 * add another if check to the most frequent codepath which we want to avoid.
				 */
				if (gv_cur_region->read_only)
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
					assert(FALSE);
					break;
				}
			} else
			{	/* cr->dirty should have become 0 immediately after the if (0 != cr->dirty) check but before
				 * the if (cr->dirty >= flushed_dirty_tn) check.
				 */
				assert(0 == cr->dirty);
				cr->dirty = 0;	/* something dropped the bits; fix it and proceed */
			}
		}
		UNIX_ONLY(
		/* the cache-record is not free for reuse until the write-latch value becomes LATCH_CLEAR.
		 * In VMS, since resetting the write-latch value occurs in wcs_wtfini() which is in CRIT, we are fine.
		 * In Unix, this resetting is done by wcs_wtstart() which is out-of-crit. Therefore, we need to wait
		 * 	for this value to be LATCH_CLEAR before reusing this cache-record.
		 */
		else if (LATCH_CLEAR != WRITE_LATCH_VAL(cr))
		{	/* possible if a concurrent wcs_wtstart() has set cr->dirty to 0 but not yet cleared the latch.
			 * this should be very rare though.
			 */
			if (lcnt < pass2)
				continue;	/* try to find some other cache-record to reuse until the 3rd pass */
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
		 * 	through shuffqth() below.
		 * Note that t_qread() has special code to handle read_in_progress */
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
				/* the owner has been unable to complete the read -
					check for some things before going to sleep */
				if (cr->read_in_progress < -1)
				{
					BG_TRACE_PRO(db_csh_getn_out_of_design);  /* outside of design; clear to known state */
 					send_msg(VARLSTCNT(4) ERR_INVALIDRIP, 2, DB_LEN_STR(gv_cur_region));
					INTERLOCK_INIT(cr);
					assert(cr->r_epid == 0);
					cr->r_epid = 0;
				} else  if ((0 != cr->r_epid) && (FALSE == is_proc_alive(cr->r_epid, cr->image_count)))
				{
					INTERLOCK_INIT(cr);			/* Process gone, release that process's lock */
					cr->r_epid = 0;
				} else
				{
					if (1 == ocnt)
					{
						BG_TRACE_PRO(db_csh_getn_rip_wait);
						r_epid = cr->r_epid;
					}
					wcs_sleep(ocnt);
				}
				LOCK_BUFF_FOR_READ(cr, rip);
			}
			if ((BUF_OWNER_STUCK < ocnt) && (0 != rip))
			{
				BG_TRACE_PRO(db_csh_getn_buf_owner_stuck);
				if (0 != cr->r_epid)
				{
					if (r_epid != cr->r_epid)
						GTMASSERT;
					RELEASE_BUFF_READ_LOCK(cr);
					send_msg(VARLSTCNT(8) ERR_BUFRDTIMEOUT, 6, process_id,
								cr->blk, cr, r_epid, DB_LEN_STR(gv_cur_region));
					continue;
				} else
					INTERLOCK_INIT(cr);
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
		return cr;
	}
	/* force a recover */
	INCR_DB_CSH_COUNTER(csa, n_db_csh_getn_lcnt, lcnt);
	csa->nl->cur_lru_cache_rec_off = GDS_ABS2REL(cr);
	return (cache_rec_ptr_t)CR_NOTVALID;
}
