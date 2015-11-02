/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>
#include "gtm_time.h"
#include "gtm_inet.h"

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdskill.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdsbml.h"
#include "ccp.h"
#include "error.h"
#include "interlock.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h and cws_insert.h */
#include "tp.h"
#include "gdsbgtr.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "mupipbckup.h"
#include "cache.h"
#include "gt_timer.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "cws_insert.h"
#include "min_max.h"

#ifdef UNIX
#include "gtmrecv.h"
#endif

/* Include prototypes */
#include "t_qread.h"
#include "t_retry.h"
#include "t_commit_cleanup.h"
#include "send_msg.h"
#include "bm_getfree.h"
#include "rc_cpt_ops.h"
#include "rel_quant.h"
#include "wcs_flu.h"
#include "mm_update.h"
#include "bg_update.h"
#include "wcs_get_space.h"
#include "wcs_timer_start.h"
#include "process_deferred_stale.h"
#include "t_end.h"
#include "add_inter.h"
#include "jnl_write_pblk.h"
#include "jnl_write_aimg_rec.h"
#include "memcoherency.h"
#include "jnl_get_checksum.h"
#include "wbox_test_init.h"
#include "bml_status_check.h"

#define BLOCK_FLUSHING(x) (csa->hdr->clustered && x->flushing && !CCP_SEGMENT_STATE(cs_addrs->nl,CCST_MASK_HAVE_DIRTY_BUFFERS))

#define	RESTORE_CURRTN_IF_NEEDED(csa, write_inctn, decremented_currtn)					\
{													\
	if (write_inctn && decremented_currtn)								\
	{	/* decremented curr_tn above; need to restore to original state due to the restart */	\
		assert(csa->now_crit);									\
		if (csa->now_crit)									\
		{	/* need crit to update curr_tn and early_tn */					\
			csa->ti->curr_tn++;								\
			csa->ti->early_tn++;								\
		}											\
		decremented_currtn = FALSE;								\
	}												\
}

GBLREF	bool			certify_all_blocks, rc_locked;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	cache_rec_ptr_t		cr_array[((MAX_BT_DEPTH * 2) - 1) * 2]; /* Maximum number of blocks that can be in transaction */
GBLREF	unsigned int		cr_array_index;
GBLREF	boolean_t		block_saved;
GBLREF	int4			update_trans;
GBLREF	cw_set_element		cw_set[];		/* create write set. */
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_namehead		*gv_target;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	short			dollar_tlevel;
GBLREF	trans_num		start_tn;
GBLREF	unsigned int		t_tries;
GBLREF	uint4			t_err, process_id;
GBLREF	unsigned char		cw_set_depth, cw_map_depth;
GBLREF	unsigned char		rdfail_detail;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl, temp_jnlpool_ctl;
GBLREF	bool			is_standalone;
GBLREF	boolean_t		is_updproc;
GBLREF	seq_num			seq_num_one;
GBLREF	boolean_t		mu_reorg_process;
GBLREF	boolean_t		dse_running;
GBLREF	boolean_t		unhandled_stale_timer_pop;
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF	boolean_t 		kip_incremented;
GBLREF	boolean_t		need_kip_incr;
GBLREF	boolean_t		write_after_image;
GBLREF	boolean_t		is_replicator;
GBLREF	seq_num			seq_num_zero;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	boolean_t		gvdupsetnoop; /* if TRUE, duplicate SETs update journal but not database (except for curr_tn++) */
GBLREF	boolean_t		is_dollar_incr;	/* valid only if gvcst_put is in the call-stack.
						 * is a copy of "in_gvcst_incr" just before it got reset to FALSE */
GBLREF	boolean_t		mu_reorg_upgrd_dwngrd_in_prog;	/* TRUE if MUPIP REORG UPGRADE/DOWNGRADE is in progress */
GBLREF	boolean_t		mu_reorg_nosafejnl;		/* TRUE if NOSAFEJNL explicitly specified */
GBLREF	trans_num		mu_reorg_upgrd_dwngrd_blktn;	/* tn in blkhdr of current block processed by REORG UP/DOWNGRADE */
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	inctn_detail_t		inctn_detail;			/* holds detail to fill in to inctn jnl record */

#ifdef UNIX
GBLREF	recvpool_addrs		recvpool;
#endif

/* This macro isn't enclosed in parantheses to allow for optimizations */
#define VALIDATE_CYCLE(history)						\
if (history)								\
{									\
	for (t1 = history->h;  t1->blk_num;  t1++)			\
	{								\
		if (t1->cr->cycle != t1->cycle)				\
		{		/* cache slot has been stolen */	\
			assert(FALSE == csa->now_crit);			\
			status = cdb_sc_cyclefail;			\
			goto failed_skip_revert;			\
		}							\
	}								\
}

trans_num t_end(srch_hist *hist1, srch_hist *hist2)
{
	srch_hist		*hist;
	bt_rec_ptr_t		bt;
	bool			blk_used;
	cache_rec_ptr_t		cr;
	cw_set_element		*cs, *cs_top, *cs1;
	enum cdb_sc		status;
	int			int_depth, tmpi;
	uint4			jnl_status;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp, jbbp; /* jbp is non-NULL if journaling, jbbp is non-NULL only if before-image journaling */
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sgm_info		*dummysi = NULL;	/* needed as a dummy parameter for {mm,bg}_update */
	srch_blk_status		*t1;
	trans_num		valid_thru, tnque_earliest_tn, dbtn, blktn, temp_tn, epoch_tn;
	unsigned char		cw_depth, cw_bmp_depth;
	jnldata_hdr_ptr_t	jnl_header;
	uint4			total_jnl_rec_size, tmp_cumul_jnl_rec_len, tmp_cw_set_depth, prev_cw_set_depth;
	DEBUG_ONLY(unsigned int	tot_jrec_size;)
	jnlpool_ctl_ptr_t	jpl, tjpl;
	boolean_t		replication = FALSE;
	boolean_t		is_mm, release_crit = FALSE;
	boolean_t		read_before_image; /* TRUE if before-image journaling or online backup in progress
						    * This is used to read before-images of blocks whose cs->mode is gds_t_create */
	boolean_t		write_inctn = FALSE;	/* set to TRUE in case writing an inctn record is necessary */
	boolean_t		decremented_currtn, retvalue;
	blk_hdr_ptr_t		old_block;
	unsigned int		bsiz;
	jnl_tm_t		save_gbl_jrec_time;

	error_def(ERR_GVKILLFAIL);
	error_def(ERR_GVPUTFAIL);
	error_def(ERR_NOTREPLICATED);
	error_def(ERR_JNLFILOPN);

	assert(hist1 != hist2);
	csa = cs_addrs;
	csd = csa->hdr;
	is_mm = (dba_mm == csd->acc_meth);
	status = cdb_sc_normal;
	assert(cs_data == csd);
	assert((t_tries < CDB_STAGNATE) || csa->now_crit);
	assert(0 == dollar_tlevel);
	assert(!cw_set_depth || update_trans); /* whenever cw_set_depth is non-zero, ensure that update_trans is TRUE */
	assert(cw_set_depth || !update_trans || gvdupsetnoop); /* whenever cw_set_depth is zero, ensure that update_trans
								* is FALSE except when set noop optimization is enabled */
	assert(0 == cr_array_index);
	cr_array_index = 0;	/* be safe and reset it in PRO even if it is not zero */
	if (csd->wc_blocked || (is_mm && (csa->total_blks != csa->ti->total_blks)))
	{ /* If blocked, or we have MM and file has been extended, force repair */
		status = cdb_sc_helpedout;	/* force retry with special status so philanthropy isn't punished */
		goto failed_skip_revert;
	} else
	{
		if (!update_trans && (start_tn == csa->ti->early_tn))
		{	/* read with no change to the transaction history */
			if (!is_mm)
			{
				VALIDATE_CYCLE(hist1);
				VALIDATE_CYCLE(hist2);
			}
			if (csa->now_crit)
				rel_crit(gv_cur_region);
			if (unhandled_stale_timer_pop)
				process_deferred_stale();
			CWS_RESET;
			t_tries = 0;	/* commit was successful so reset t_tries */
			return csa->ti->curr_tn;
		}
	}
	if ((0 != cw_set_depth)
			&& ((gds_t_writemap == cw_set[0].mode)
				|| ((gds_t_busy2free == cw_set[0].mode) && (gds_t_writemap == cw_set[1].mode))))
	{	/* freeing a block from gvcst_kill or reorg, or upgrading/downgrading a block by reorg */
		cw_depth = 0;
		if (gds_t_writemap == cw_set[0].mode)
			cw_bmp_depth = 0;
		else
			cw_bmp_depth = 1;
	} else
	{
		cw_depth = cw_set_depth;
		cw_bmp_depth = cw_depth;
	}
	if (0 != cw_depth)
	{	/* Caution : since csa->backup_in_prog and read_before_image are initialized below only if (cw_depth),
		 * 	these variables should be used below only within an if (cw_depth).
		 */
		assert(sizeof(bsiz) == sizeof(old_block->bsiz));
		csa->backup_in_prog = (BACKUP_NOT_IN_PROGRESS != csa->nl->nbb);
		jbbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
		read_before_image = ((NULL != jbbp) || csa->backup_in_prog);
		for (cs = &cw_set[0], cs_top = cs + cw_depth; cs < cs_top; cs++)
		{
			assert(0 == cs->jnl_freeaddr);	/* ensure haven't missed out resetting jnl_freeaddr for any cse in
							 * t_write/t_create/t_write_map/t_write_root/mu_write_map [D9B11-001991] */
			if (gds_t_create == cs->mode)
			{
				assert(0 == cs->blk_checksum);
				int_depth = (int)cw_set_depth;
				if (0 > (cs->blk = bm_getfree(cs->blk, &blk_used, cw_depth, cw_set, &int_depth)))
				{
					if (FILE_EXTENDED == cs->blk)
					{
						status = cdb_sc_helpedout;
						assert(is_mm);
					} else
					{
						GET_CDB_SC_CODE(cs->blk, status);	/* code is set in status */
					}
					goto failed_skip_revert;
				}
				/* No need to write before-image in case the block is FREE. In case the database had never
				 * been fully upgraded from V4 to V5 format (after the MUPIP UPGRADE), all RECYCLED blocks
				 * can basically be considered FREE (i.e. no need to write before-images since backward journal
				 * recovery will never be expected to take the database to a point BEFORE the mupip upgrade).
				 */
				if (!read_before_image || !blk_used || !csd->db_got_to_v5_once)
					cs->old_block = NULL;
				else
				{
					cs->old_block = t_qread(cs->blk, (sm_int_ptr_t)&cs->cycle, &cs->cr);
					old_block = (blk_hdr_ptr_t)cs->old_block;
					if (NULL == old_block)
					{
						status = (enum cdb_sc)rdfail_detail;
						goto failed_skip_revert;
					}
					if ((NULL != jbbp) && (old_block->tn < jbbp->epoch_tn))
					{	/* Compute CHECKSUM for writing PBLK record before getting crit.
						 * It is possible that we are reading a block that is actually marked free in
						 * the bitmap (due to concurrency issues at this point). Therefore we might be
						 * actually reading uninitialized block headers and in turn a bad value of
						 * "old_block->bsiz". Restart if we ever access a buffer whose size is greater
						 * than the db block size.
						 */
						bsiz = old_block->bsiz;
						if (bsiz > csd->blk_size)
						{
							assert(CDB_STAGNATE > t_tries);
							status = cdb_sc_lostbmlcr;
							goto failed_skip_revert;
						}
						JNL_GET_CHECKSUM_ACQUIRED_BLK(cs, csd, old_block, bsiz);
					}
				}
				/* assert that the block that we got from bm_getfree is less than the total blocks.
				 * if we do not have crit in this region, then it is possible that bm_getfree can return
				 * a cs->blk that is >= csa->ti->total_blks (i.e. if the bitmap buffer gets recycled).
				 * adjust assert accordingly.
				 * note that checking for crit is equivalent to checking if we are in the final retry.
				 */
				assert((CDB_STAGNATE > t_tries) || (cs->blk < csa->ti->total_blks));
				cs->mode = gds_t_acquired;
				assert(GDSVCURR == cs->ondsk_blkver);
			}
		}
	}
	if (update_trans && JNL_ENABLED(csa))
	{	/* compute the total journal record size requirements before grab_crit().
		 * there is code later that will check for state changes from now to then and if so do a recomputation
		 */
		assert(!cw_map_depth || cw_set_depth < cw_map_depth);
		tmp_cw_set_depth = cw_map_depth ? cw_map_depth : cw_set_depth;
		TOTAL_NONTPJNL_REC_SIZE(total_jnl_rec_size, non_tp_jfb_ptr, csa, tmp_cw_set_depth);
		/* For a non-tp update maximum journal space we may need is total size of
		 * 	1) space for maximum CDB_CW_SET_SIZE PBLKs, that is, MAX_JNL_REC_SIZE * CDB_CW_SET_SIZE
		 *	2) space for a logical record itself, that is, MAX_LOGI_JNL_REC_SIZE and
		 * 	3) overhead records (MIN_TOTAL_NONTPJNL_REC_SIZE + JNL_FILE_TAIL_PRESERVE)
		 * This requirement is less than the minimum autoswitchlimit size (JNL_AUTOSWITCHLIMIT_MIN) as asserted below.
		 * Therefore we do not need any check to issue JNLTRANS2BIG error like is being done in tp_tend.c
		 */
		assert((CDB_CW_SET_SIZE * MAX_JNL_REC_SIZE + MAX_LOGI_JNL_REC_SIZE +
			MIN_TOTAL_NONTPJNL_REC_SIZE + JNL_FILE_TAIL_PRESERVE) <= (JNL_AUTOSWITCHLIMIT_MIN * DISK_BLOCK_SIZE));
		DEBUG_ONLY(tot_jrec_size = MAX_REQD_JNL_FILE_SIZE(total_jnl_rec_size);)
		assert(tot_jrec_size <= csd->autoswitchlimit);
		/* The SET_GBL_JREC_TIME done below should be done before any journal writing activity
		 * on this region's journal file. This is because all the jnl record writing routines assume
		 * jgbl.gbl_jrec_time is initialized appropriately.
		 */
		assert(!jgbl.forw_phase_recovery || jgbl.dont_reset_gbl_jrec_time);
		if (!jgbl.dont_reset_gbl_jrec_time)
			SET_GBL_JREC_TIME;	/* initializes jgbl.gbl_jrec_time */
		assert(jgbl.gbl_jrec_time);
	}
	block_saved = FALSE;
	ESTABLISH_RET(t_ch, 0);
	if (!csa->now_crit)
	{
		if (update_trans)
		{	/* Get more space if needed. This is done outside crit so that
			 * any necessary IO has a chance of occurring outside crit.
			 * The available space must be double-checked inside crit. */
			if (!is_mm && (csa->nl->wc_in_free < (int4)(cw_set_depth + 1))
				   && !wcs_get_space(gv_cur_region, cw_set_depth + 1, NULL))
				assert(FALSE);	/* wcs_get_space() should have returned TRUE unconditionally in this case */
			for (;;)
			{
				grab_crit(gv_cur_region);
				if (FALSE == csd->freeze)
					break;
				rel_crit(gv_cur_region);
				while (csd->freeze)
					hiber_start(1000);
			}
		} else
			grab_crit(gv_cur_region);
	}
	/* Any retry transition where the destination state is the 3rd retry, we don't want to release crit,
	 * i.e. for 2nd to 3rd retry transition or 3rd to 3rd retry transition.
	 * Therefore we need to release crit only if (CDB_STAGNATE - 1) > t_tries
	 * But 2nd to 3rd retry transition doesn't occur if in 2nd retry we get jnlstatemod/jnlclose/backupstatemod code.
	 * Hence the variable release_crit to track the above.
	 */
	release_crit = (CDB_STAGNATE - 1) > t_tries;
	assert(!cw_depth || update_trans);
	/* If inctn_opcode has a valid value, then we better be doing an update. The only exception to this rule is if we are
	 * in MUPIP REORG UPGRADE/DOWNGRADE (mu_reorg_upgrd_dwngrd.c) where update_trans is explicitly set to FALSE in some cases.
	 */
	assert((inctn_invalid_op == inctn_opcode) || mu_reorg_upgrd_dwngrd_in_prog || update_trans);
	if (update_trans)
	{
		if (JNL_ALLOWED(csa))
		{
			if ((csa->jnl_state != csd->jnl_state) || (csa->jnl_before_image != csd->jnl_before_image))
			{ 	/* csd->jnl_state or csd->jnl_before_image changed since last time
				 * 	csa->jnl_before_image and csa->jnl_state got set */
				csa->jnl_before_image = csd->jnl_before_image;
				csa->jnl_state = csd->jnl_state;
				/* jnl_file_lost() causes a jnl_state transition from jnl_open to jnl_closed
				 * and additionally causes a repl_state transition from repl_open to repl_closed
				 * all without standalone access. This means that csa->repl_state might be repl_open
				 * while csd->repl_state might be repl_closed. update csa->repl_state in this case
				 * as otherwise the rest of the code might look at csa->repl_state and incorrectly
				 * conclude replication is on and generate sequence numbers when actually no journal
				 * records are being generated. [C9D01-002219]
				 */
				csa->repl_state = csd->repl_state;
				status = cdb_sc_jnlstatemod;
				if ((CDB_STAGNATE - 1) == t_tries)
					release_crit = TRUE;
				goto failed;
			}
		}
		if (cw_depth && (csa->backup_in_prog != (BACKUP_NOT_IN_PROGRESS != csa->nl->nbb)))
		{
			if (!csa->backup_in_prog && !(JNL_ENABLED(csa) && csa->jnl_before_image))
			{	/* If online backup is in progress now and before-image journaling is not enabled,
				 * we would not have read before-images for created blocks. Although it is possible
				 * that this transaction might not have blocks with gds_t_create at all, we expect
				 * this backup_in_prog state change to be so rare that it is ok to restart.
				 */
				status = cdb_sc_backupstatemod;
				if ((CDB_STAGNATE - 1) == t_tries)
					release_crit = TRUE;
				goto failed;
			}
			csa->backup_in_prog = !csa->backup_in_prog;	/* reset csa->backup_in_prog to current state */
			read_before_image = ((JNL_ENABLED(csa) && csa->jnl_before_image) || csa->backup_in_prog); /* recalculate */
		}
		/* in crit, ensure cache-space is available. the out-of-crit check done above might not have been enough */
		if (!is_mm && (csa->nl->wc_in_free < (int4)(cw_set_depth + 1))
			   && !wcs_get_space(gv_cur_region, cw_set_depth + 1, NULL))
		{
			assert(FALSE);
			SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
			BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_hist);
			status = cdb_sc_cacheprob;
			if ((CDB_STAGNATE - 1) == t_tries)
				release_crit = TRUE;
			goto failed;
		}
		if (inctn_invalid_op != inctn_opcode)
		{
			assert(cw_set_depth || mu_reorg_process);
			write_inctn = TRUE;	/* mupip reorg or gvcst_bmp_mark_free or extra block split in gvcstput */
			decremented_currtn = FALSE;
			if (jgbl.forw_phase_recovery && !JNL_ENABLED(csa))
			{	/* forward recovery (deduced above from the fact that journaling is not enabled) is supposed
				 * to accurately simulate GT.M runtime activity for every transaction number. The way it does
				 * this is by incrementing transaction numbers for all inctn records that GT.M wrote and not
				 * incrementing transaction number for any inctn activity that forward recovery internally needs
				 * to do. all inctn activity done outside of t_end has already been protected against incrementing
				 * transaction number in case of forward recovery. t_end is a little bit tricky since in this case
				 * a few database blocks get modified with the current transaction number and not incrementing the
				 * transaction number might result in the database transaction number being lesser than the block
				 * transaction number. we work around this problem by decrementing the database transaction number
				 * just before the commit so the database block updates for the inctn transaction get the
				 * transaction number of the previous transaction effectively merging the inctn transaction with
				 * the previous transaction.
				 */
				/* cw_set_depth is 1 for all INCTN operations except for block free operations when it can be 2 */
				assert((inctn_gvcstput_extra_blk_split == inctn_opcode)
					|| (inctn_bmp_mark_free_gtm == inctn_opcode)
					|| (inctn_bmp_mark_free_mu_reorg == inctn_opcode) || (1 == cw_set_depth));
				csa->ti->curr_tn--;
				csa->ti->early_tn--;
				decremented_currtn = TRUE;
			}
		}
	}
	assert(csd == csa->hdr);
	valid_thru = dbtn = csa->ti->curr_tn;
	if (!is_mm)
		tnque_earliest_tn = ((th_rec_ptr_t)((sm_uc_ptr_t)csa->th_base + csa->th_base->tnque.fl))->tn;
	if (update_trans)
		valid_thru++;
	for (hist = hist1;  (NULL != hist);  hist = (hist == hist1) ? hist2 : NULL)
	{
		for (t1 = hist->h;  t1->blk_num;  t1++)
		{
			if (is_mm)
			{
				if (t1->tn <= ((blk_hdr_ptr_t)(t1->buffaddr))->tn)
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_blkmod;
					goto failed;
				}
			} else
			{
				bt = bt_get(t1->blk_num);
				if (NULL == bt)
				{
					if (t1->tn <= tnque_earliest_tn)
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_losthist;
						goto failed;
					}
					cr = db_csh_get(t1->blk_num);
				} else
				{
					if (BLOCK_FLUSHING(bt))
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_blockflush;
						goto failed;
					}
					if (CR_NOTVALID == bt->cache_index)
						cr = db_csh_get(t1->blk_num);
					else
					{
						cr = (cache_rec_ptr_t)GDS_REL2ABS(bt->cache_index);
						if (cr->blk != bt->blk)
						{
							assert(FALSE);
							SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_crbtmismatch1);
							status = cdb_sc_crbtmismatch;
							goto failed;
						}
					}
					assert(bt->killtn <= bt->tn);
					if (t1->tn <= bt->tn)
					{
						assert(CDB_STAGNATE > t_tries);
						assert(!IS_DOLLAR_INCREMENT || !write_inctn);
						/* If the current operation is a $INCREMENT, then try to optimize by recomputing
						 * the update array. Do this only as long as ALL the following conditions are met.
						 *	a) the current history's block is a leaf level block we intend to modify
						 *	b) update is restricted to the data block
						 *	c) no block splits are involved.
						 *	d) this does not end up creating a new global variable tree
						 *	e) this is not a case of $INCR about to signal a GVUNDEF
						 *		cw_set_depth is 0 in that case
						 * Conveniently enough, a simple check of (1 != cw_set_depth) is enough
						 * to categorize conditions (c) to (e)
						 *
						 * Future optimization : It is possible that M-SETs (not just $INCR)
						 * that update only one data block can benefit from this optimization.
						 * But that has to be carefully thought out.
						 */
						if (!IS_DOLLAR_INCREMENT
							|| t1->level || (1 != cw_set_depth) || (t1->blk_num != cw_set[0].blk))
						{
							status = cdb_sc_blkmod;
							goto failed;
						} else
						{
							status = gvincr_recompute_upd_array(t1, &cw_set[0], cr);
							if (cdb_sc_normal != status)
							{
								status = cdb_sc_blkmod;
								goto failed;
							}
						}
					}
				}
				if ((cache_rec_ptr_t)CR_NOTVALID == cr)
				{
					SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
					BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_hist);
					status = cdb_sc_cacheprob;
					goto failed;
				}
				if ((NULL == cr) || (cr->cycle != t1->cycle) ||
				    ((sm_long_t)GDS_REL2ABS(cr->buffaddr) != (sm_long_t)t1->buffaddr))
				{
					if ((NULL != cr) && (NULL != bt) && (cr->blk != bt->blk))
					{
						assert(FALSE);
						SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_crbtmismatch2);
						status = cdb_sc_crbtmismatch;
						goto failed;
					}
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_lostcr;
					goto failed;
				}
				/* only way to have in_cw_set to be TRUE is in the case
				 * where both the histories passed contain this particular block.
				 */
				assert(FALSE == cr->in_cw_set || hist == hist2 && cr->blk == hist1->h[t1->level].blk_num);
				if (!cr->in_cw_set)
				{
					cr_array[cr_array_index++] = cr;
					cr->in_cw_set = TRUE;
					cr->refer = TRUE;
				}
				DEBUG_ONLY(
				else
				{	/* Make sure that we have already added this cache-record into the cr_array */
					for (tmpi = 0; tmpi < cr_array_index; tmpi++)
						if (cr_array[tmpi] == cr)
							break;
					assert(tmpi < cr_array_index);
				}
				)

			}
			t1->tn = valid_thru;
		}
	}
	/* check bit maps for usage */
	if (0 != cw_map_depth)
	{	/* Bit maps on end from mu_reorg (from a call to mu_swap_blk) or mu_reorg_upgrd_dwngrd */
		prev_cw_set_depth = cw_set_depth;
		cw_set_depth = cw_map_depth;
	}
	for (cs = &cw_set[cw_bmp_depth], cs_top = &cw_set[cw_set_depth]; cs < cs_top; cs++)
	{
		assert(0 == cs->jnl_freeaddr);	/* ensure haven't missed out resetting jnl_freeaddr for any cse in
						 * t_write/t_create/{t,mu}_write_map/t_write_root [D9B11-001991] */
		/* A bitmap block update will cause us to restart with "cdb_sc_bmlmod". TP transactions on the other hand
		 * try reallocating blocks using the function "reallocate_bitmap". That is not presently used here because
		 * there are cases like MUPIP REORG or MUPIP REORG UPGRADE etc. where we do not want this functionality.
		 * Also, non-TP restarts due to bitmap collisions are currently assumed to be negligible. Hence no
		 * reallocation done in the non-TP case. Reconsider if this assumption is invalidated.
		 */
		if (is_mm)
		{
			if (cs->tn <= ((blk_hdr_ptr_t)(cs->old_block))->tn)
			{
				assert(CDB_STAGNATE > t_tries);
				status = cdb_sc_bmlmod;
				goto failed;
			}
		} else
		{
			bt = bt_get(cs->blk);
			if (NULL == bt)
			{
				if (cs->tn <= tnque_earliest_tn)
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_lostbmlhist;
					goto failed;
				}
				cr = db_csh_get(cs->blk);
			} else
			{
				if (BLOCK_FLUSHING(bt))
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_blockflush;
					goto failed;
				}
				if (cs->tn <= bt->tn)
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_bmlmod;
					goto failed;
				}
				if (CR_NOTVALID == bt->cache_index)
					cr = db_csh_get(cs->blk);
				else
				{
					cr = (cache_rec_ptr_t)GDS_REL2ABS(bt->cache_index);
					if (cr->blk != bt->blk)
					{
						assert(FALSE);
						SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_crbtmismatch3);
						status = cdb_sc_crbtmismatch;
						goto failed;
					}
				}
			}
			if ((cache_rec_ptr_t)CR_NOTVALID == cr)
			{
				SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_bitmap_nullbt);
				status = cdb_sc_cacheprob;
				goto failed;
			}
			if ((NULL == cr)  || (cr->cycle != cs->cycle) ||
				((sm_long_t)GDS_REL2ABS(cr->buffaddr) != (sm_long_t)cs->old_block))
			{
				assert(CDB_STAGNATE > t_tries);
				status = cdb_sc_lostbmlcr;
				goto failed;
			}
			cr_array[cr_array_index++] = cr;
			assert(FALSE == cr->in_cw_set);
			cr->in_cw_set = TRUE;
			cr->refer = TRUE;
		}
	}
	if ((0 != cw_map_depth) && mu_reorg_upgrd_dwngrd_in_prog)
	{	/* Bit maps on end from mu_reorg_upgrd_dwngrd. Bitmap history has been validated.
		 * But we do not want bitmap cse to be considered for bg_update. Reset cw_set_depth accordingly.
		 */
		cw_set_depth = prev_cw_set_depth;
		assert(1 >= cw_set_depth);
	}
	assert(csd == csa->hdr);
	assert(!need_kip_incr || update_trans);
	if (update_trans)
	{
		if (cw_depth && read_before_image && !is_mm)
		{
			assert(!(JNL_ENABLED(csa) && csa->jnl_before_image) || jbbp == csa->jnl->jnl_buff);
			assert((JNL_ENABLED(csa) && csa->jnl_before_image) || (NULL == jbbp));
			for (cs = cw_set, cs_top = cs + cw_set_depth;  cs < cs_top;  ++cs)
			{	/* have already read old block for creates before we got crit, make sure
				 * cache record still has correct block. if not, reset "cse" fields to
				 * point to correct cache-record. this is ok to do since we only need the
				 * prior content of the block (for online backup or before-image journaling)
				 * and did not rely on it for constructing the transaction. Restart if
				 * block is not present in cache now or is being read in currently.
				 */
				if ((gds_t_acquired == cs->mode) && (NULL != cs->old_block))
				{
					assert(read_before_image == ((JNL_ENABLED(csa) && csa->jnl_before_image)
										|| csa->backup_in_prog));
					cr = db_csh_get(cs->blk);
					if ((cache_rec_ptr_t)CR_NOTVALID == cr)
					{
						SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_jnl_cwset);
						status = cdb_sc_cacheprob;
						goto failed;
					}
					if ((NULL == cr) || (0 <= cr->read_in_progress))
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_lostbefor;
						goto failed;
					}
					cr_array[cr_array_index++] = cr;
					assert(FALSE == cr->in_cw_set);
					cr->in_cw_set = TRUE;
					cr->refer = TRUE;
					cs->ondsk_blkver = cr->ondsk_blkver;
					assert((cs->cr != cr) || (cs->old_block == (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr)));
					if ((cs->cr != cr) || (cs->cycle != cr->cycle)
						|| (cs->tn <= ((blk_hdr_ptr_t)GDS_REL2ABS(cr->buffaddr))->tn))
					{	/* Global buffer containing "cs->blk" changed since we read it out of crit */
						old_block = (blk_hdr_ptr_t)GDS_REL2ABS(cr->buffaddr);
						if (NULL != jbbp)
						{	/* PBLK checksum was computed outside-of-crit when block was read but
							 * block has relocated in the cache since then. Recompute the checksum.
							 * We hold crit at this point so we are guaranteed valid bsiz field.
							 * Hence we do not need to take MIN(bsiz, csd->blk_size) like we did
							 * in the earlier call to jnl_get_checksum.
							 */
							bsiz = old_block->bsiz;
							assert(bsiz <= csd->blk_size);
							if (old_block->tn < jbbp->epoch_tn)
								cs->blk_checksum = jnl_get_checksum((uint4*)old_block, bsiz);
							else
								cs->blk_checksum = 0;
						}
						cs->cr = cr;
						cs->cycle = cr->cycle;
						cs->old_block = (sm_uc_ptr_t)old_block;
					}
					assert(cs->old_block == (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr));
					DEBUG_ONLY(old_block = (blk_hdr_ptr_t)cs->old_block;)
					assert(!cs->blk_checksum
						|| (cs->blk_checksum == jnl_get_checksum((uint4 *)old_block, old_block->bsiz)));
				}
			}
		}
		/* if we are not writing an INCTN record, we better have a non-zero cw_depth.
		 * the only two exceptions are that
		 * 	a) if we were being called from gvcst_put() for a duplicate SET
		 * 	b) if we were called from DSE MAPS
		 * in case (a), we want to write logical SET journal records and replicate them.
		 * in case (b), we do not want to replicate them. we want to assert that is_replicator is FALSE in this case.
		 * the following assert achieves that purpose.
		 */
		assert((inctn_invalid_op != inctn_opcode) || cw_depth
				|| !is_replicator				/* exception case (b) */
				|| (ERR_GVPUTFAIL == t_err) && gvdupsetnoop);	/* exception case (a) */
		if (REPL_ALLOWED(csa) && is_replicator && (inctn_invalid_op == inctn_opcode))
		{
			replication = TRUE;
			jpl = jnlpool_ctl;
			tjpl = temp_jnlpool_ctl;
			grab_lock(jnlpool.jnlpool_dummy_reg);
			QWASSIGN(tjpl->write_addr, jpl->write_addr);
			QWASSIGN(tjpl->write, jpl->write);
			QWASSIGN(tjpl->jnl_seqno, jpl->jnl_seqno);
			INT8_ONLY(assert(tjpl->write == tjpl->write_addr % tjpl->jnlpool_size);)
			assert(jgbl.cumul_jnl_rec_len);
			tmp_cumul_jnl_rec_len = (uint4)(jgbl.cumul_jnl_rec_len + sizeof(jnldata_hdr_struct));
			tjpl->write += sizeof(jnldata_hdr_struct);
			if (tjpl->write >= tjpl->jnlpool_size)
			{
				assert(tjpl->write == tjpl->jnlpool_size);
				tjpl->write = 0;
			}
			assert(QWEQ(jpl->early_write_addr, jpl->write_addr));
			QWADDDW(jpl->early_write_addr, jpl->write_addr, tmp_cumul_jnl_rec_len);
			/* Source server does not read in crit. It relies on early_write_addr, the transaction
			 * data, lastwrite_len, write_addr being updated in that order. To ensure this order,
			 * we have to force out early_write_addr to its coherency point now. If not, the source
			 * server may read data that is overwritten (or stale). This is true only on
			 * architectures and OSes that allow unordered memory access
			 */
			SHM_WRITE_MEMORY_BARRIER;
		}
		assert(cw_set_depth < CDB_CW_SET_SIZE);
		ASSERT_CURR_TN_EQUALS_EARLY_TN(csa, dbtn);
		CHECK_TN(csa, csd, dbtn);	/* can issue rts_error TNTOOLARGE */
		if (JNL_ENABLED(csa))
		{	/* Since we got the system time (jgbl.gbl_jrec_time) outside of crit, it is possible that
			 * journal records were written concurrently to this file with a timestamp that is future
			 * relative to what we recorded. In that case, adjust our recorded time to match this.
			 * This is necessary to ensure that timestamps of successive journal records for each
			 * database file are in non-decreasing order. A side-effect of this is that our recorded
			 * time might not accurately reflect the current system time but that is considered not
			 * an issue since we dont expect to be off by more than a second or two if at all.
			 * Another side effect is that even if the system time went back, we will never write
			 * out-of-order timestamped journal records in the lifetime of this database shared memory.
			 */
			jpc = csa->jnl;
			jbp = jpc->jnl_buff;
			/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order of jnl
			 * records. This needs to be done BEFORE the jnl_ensure_open as that could write journal records
			 * (if it decides to switch to a new journal file)
			 */
			ADJUST_GBL_JREC_TIME(jgbl, jbp);
			/* Note that jnl_ensure_open() can call cre_jnl_file() which
			 * in turn assumes jgbl.gbl_jrec_time is set. Also jnl_file_extend() can call
			 * jnl_write_epoch_rec() which in turn assumes jgbl.gbl_jrec_time is set.
			 * In case of forw-phase-recovery, mur_output_record() would have already set this.
			 */
			assert(jgbl.gbl_jrec_time);
			jnl_status = jnl_ensure_open();
			assert((csa->jnl_state == csd->jnl_state) &&
				(csa->jnl_before_image == csd->jnl_before_image));
			GTM_WHITE_BOX_TEST(WBTEST_T_END_JNLFILOPN, jnl_status, ERR_JNLFILOPN);
			if (jnl_status == 0)
			{	/* tmp_cw_set_depth was used to do TOTAL_NONTPJNL_REC_SIZE calculation earlier in this function.
				 * It is now though that the actual jnl record write occurs. Ensure that the current value of
				 * cw_set_depth does not entail any change in journal record size than was calculated.
				 * Same case with csa->jnl_before_images & jbp->before_images.
				 * The only exception is that in case of mu_reorg_upgrd_dwngrd_in_prog cw_set_depth will be
				 * LESSER than tmp_cw_set_depth (this is still fine as there is more size allocated than used).
				 */
				assert(cw_set_depth == tmp_cw_set_depth
					|| mu_reorg_upgrd_dwngrd_in_prog && cw_map_depth && cw_set_depth < tmp_cw_set_depth);
				assert(jbp->before_images == csa->jnl_before_image);
				if (DISK_BLOCKS_SUM(jbp->freeaddr, total_jnl_rec_size) > jbp->filesize)
				{	/* Moved as part of change to prevent journal records splitting
					 * across multiple generation journal files. */
					jnl_flush(jpc->region);
					if (jnl_file_extend(jpc, total_jnl_rec_size) == -1)
					{
						assert((!JNL_ENABLED(csd)) && (JNL_ENABLED(csa)));
						status = cdb_sc_jnlclose;
						if ((CDB_STAGNATE - 1) == t_tries)
							release_crit = TRUE;
						goto failed;
					}
				}
				assert(jgbl.gbl_jrec_time >= jbp->prev_jrec_time);
				if (0 == jpc->pini_addr)
					jnl_put_jrt_pini(csa);
				if (JNL_HAS_EPOCH(jbp))
				{
					if (jbp->next_epoch_time <= jgbl.gbl_jrec_time)
					{	/* Flush the cache. Since we are in crit, defer syncing epoch */
						if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH))
						{
							SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_jnl_wcsflu);
							status = cdb_sc_cacheprob;
							goto failed;
						}
						VMS_ONLY(
							if (csd->clustered  &&
								!CCP_SEGMENT_STATE(csa->nl, CCST_MASK_HAVE_DIRTY_BUFFERS))
							{
								CCP_FID_MSG(gv_cur_region, CCTR_FLUSHLK);
								ccp_userwait(gv_cur_region,
									CCST_MASK_HAVE_DIRTY_BUFFERS, 0, csa->nl->ccp_cycle);
							}
						)
					}
				}
			} else
				rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
		}
		blktn = dbtn;
		csa->ti->early_tn = dbtn + 1;
		if (JNL_ENABLED(csa))
		{
			DEBUG_ONLY(save_gbl_jrec_time = jgbl.gbl_jrec_time;)
			if (jbp->before_images)
			{	/* do not write PBLKs if MUPIP REORG UPGRADE/DOWNGRADE with -NOSAFEJNL */
				if (!mu_reorg_upgrd_dwngrd_in_prog || !mu_reorg_nosafejnl)
				{
					epoch_tn = jbp->epoch_tn; /* store in a local as it is used in a loop below */
					for (cs = cw_set, cs_top = cs + cw_set_depth;  cs < cs_top;  ++cs)
					{	/* write out before-update journal image records */
						if (gds_t_committed < cs->mode)
						{	/* There are two possibilities at this point.
							 * a) gds_t_write_root : In this case no need to write PBLK.
							 * b) gds_t_busy2free : This is set by gvcst_bmp_mark_free to indicate
							 *	that a block has to be freed right away instead of taking it
							 *	through the RECYCLED state. This should be done only if
							 *	csd->db_got_to_v5_once has not yet become TRUE. Once it is
							 *	TRUE, block frees will write PBLK only when the block is reused.
							 */
							assert((gds_t_write_root == cs->mode) || (gds_t_busy2free == cs->mode));
							if ((gds_t_write_root == cs->mode)
								|| (gds_t_busy2free == cs->mode) && csd->db_got_to_v5_once)
							continue;
						}
						old_block = (blk_hdr_ptr_t)cs->old_block;
						ASSERT_IS_WITHIN_SHM_BOUNDS((sm_uc_ptr_t)old_block, csa);
						DBG_ENSURE_OLD_BLOCK_IS_VALID(cs, is_mm, csa, csd);
						if ((NULL != old_block) && (old_block->tn < epoch_tn))
						{
							bsiz = old_block->bsiz;
							assert((bsiz <= csd->blk_size) || dse_running);
							assert(bsiz >= sizeof(blk_hdr) || dse_running);
							/* For acquired blocks, we should have computed checksum already.
							 * The only exception is if we found no need to compute checksum
							 * outside of crit but before we got crit, an EPOCH got written
							 * concurrently so we have to write a PBLK (and hence compute the
							 * checksum as well) when earlier we thought none was necessary.
							 * An easy way to check this is that an EPOCH was written AFTER
							 * we started this transaction.
							 */
							assert((gds_t_acquired != cs->mode) || cs->blk_checksum
								|| (epoch_tn >= start_tn));
							assert(gds_t_busy2free != cs->mode || cs->blk_checksum);
							/* It is possible that the block has a bad block-size.
							 * Before computing checksum ensure bsiz passed is safe.
							 * The checks done here for "bsiz" assignment are
							 * similar to those done in jnl_write_pblk/jnl_write_aimg.
							 */
							if (dse_running)
								bsiz = MIN(bsiz, csd->blk_size);
							assert(!cs->blk_checksum ||
								(cs->blk_checksum == jnl_get_checksum((uint4 *)old_block,
													bsiz)));
							if (!cs->blk_checksum)
								cs->blk_checksum = jnl_get_checksum((uint4 *)old_block,
													bsiz);
							jnl_write_pblk(csa, cs, old_block);
							cs->jnl_freeaddr = jbp->freeaddr;
						}
						DEBUG_ONLY(
						else
							assert(0 == cs->jnl_freeaddr);
						)
					}
				}
			}
			if (write_after_image)
			{	/* either DSE or MUPIP RECOVER playing an AIMG record */
				assert(1 == cw_set_depth); /* only one block at a time */
				assert(!replication);
				cs = &cw_set[0];
				jnl_write_aimg_rec(csa, cs);
			} else if (write_inctn)
			{
				assert(!replication);
				if ((inctn_blkupgrd == inctn_opcode) || (inctn_blkdwngrd == inctn_opcode))
				{
					assert(1 == cw_set_depth); /* upgrade/downgrade one block at a time */
					cs = &cw_set[0];
					assert(inctn_detail.blknum == cs->blk);
					assert(mu_reorg_upgrd_dwngrd_blktn < dbtn);
					if (mu_reorg_nosafejnl)
					{
						blktn = mu_reorg_upgrd_dwngrd_blktn;
						/* if NOSAFEJNL and there is going to be a block format change
						 * as a result of this update, note it down in the inctn opcode
						 * (for recovery) as there is no PBLK record for it to rely on.
						 */
						if (cs->ondsk_blkver != csd->desired_db_format)
							inctn_opcode = (inctn_opcode == inctn_blkupgrd)
									? inctn_blkupgrd_fmtchng : inctn_blkdwngrd_fmtchng;
					}
				}
				jnl_write_inctn_rec(csa);
			} else if (0 == jnl_fence_ctl.level)
			{
				if (replication)
					QWASSIGN(jnl_fence_ctl.token, tjpl->jnl_seqno);
				else
					QWASSIGN(jnl_fence_ctl.token, seq_num_zero);
				jnl_write_logical(csa, non_tp_jfb_ptr);
			} else
				jnl_write_ztp_logical(csa, non_tp_jfb_ptr);
			/* Ensure jgbl.gbl_jrec_time did not get reset by any of the jnl writing functions */
			assert(save_gbl_jrec_time == jgbl.gbl_jrec_time);
		} else if (replication)
		{	/* Case where JNL_ENABLED(csa) is FALSE but REPL_WAS_ENABLED(csa) is TRUE and therefore we need to
			 * write logical jnl records in the journal pool (no need to write in journal buffer or journal file).
			 */
			assert(!JNL_ENABLED(csa) && REPL_WAS_ENABLED(csa));
			if (0 == jnl_fence_ctl.level)
			{
				QWASSIGN(jnl_fence_ctl.token, tjpl->jnl_seqno);
				jnl_write_logical(csa, non_tp_jfb_ptr);
			} else
                                jnl_write_ztp_logical(csa, non_tp_jfb_ptr);
		}
		if (replication)
		{
			QWINCRBY(tjpl->jnl_seqno, seq_num_one);
			QWASSIGN(csa->hdr->reg_seqno, tjpl->jnl_seqno);
			if (is_updproc)
			{
				VMS_ONLY(
					QWINCRBY(jgbl.max_resync_seqno, seq_num_one);
					QWASSIGN(csa->hdr->resync_seqno, jgbl.max_resync_seqno);
				)
				UNIX_ONLY(
					assert(REPL_PROTO_VER_UNINITIALIZED !=
						recvpool.gtmrecv_local->last_valid_remote_proto_ver);
					if (REPL_PROTO_VER_DUALSITE ==
						recvpool.gtmrecv_local->last_valid_remote_proto_ver)
					{
						QWINCRBY(jgbl.max_dualsite_resync_seqno, seq_num_one);
						QWASSIGN(csa->hdr->dualsite_resync_seqno,
							jgbl.max_dualsite_resync_seqno);
					}
				)
			}
		}
		csa->prev_free_blks = csa->ti->free_blocks;
		csa->t_commit_crit = TRUE;
		if (cw_set_depth)
		{
			for (cs = cw_set, cs_top = cs + cw_set_depth;  cs < cs_top;  ++cs)
			{
				assert((gds_t_write_root != cs->mode) || ((cs - cw_set) + 1 == cw_depth));
				cs->old_mode = cs->mode;	/* note down before being reset to gds_t_committed */
				assert(gds_t_committed < gds_t_write_root);
				assert(gds_t_committed < gds_t_busy2free);
				assert((n_gds_t_op != cs->mode) && (gds_t_committed != cs->mode));
				assert((kill_t_write != cs->mode) && (kill_t_create != cs->mode));
				if (gds_t_committed > cs->mode)
				{
					DEBUG_ONLY(
						/* Check bitmap status of block we are about to modify.
						 * Two exceptions are
						 *	a) DSE which can modify bitmaps at will.
						 *	b) MUPIP RECOVER writing an AIMG. In this case it is playing
						 *		forward a DSE action so is effectively like DSE doing it.
						 */
						if (!dse_running && !write_after_image)
							bml_status_check(cs);
					)
					if (is_mm)
						status = mm_update(cs, cs_top, dbtn, blktn, dummysi);
					else
					{
						if (csd->dsid)
						{
							if (ERR_GVKILLFAIL == t_err)
							{
								if (cs == cw_set)
								{
									if ((gds_t_acquired == cs->mode) ||
									    ((cw_set_depth > 1) && (0 == cw_set[1].level)))
										rc_cpt_inval();
									else
										rc_cpt_entry(cs->blk);
								}
							} else	if (0 == cs->level)
								rc_cpt_entry(cs->blk);
						}
						status = bg_update(cs, cs_top, dbtn, blktn, dummysi);
					}
					if (cdb_sc_normal != status)
					{	/* the database is probably in trouble */
						assert(gtm_white_box_test_case_enabled);
						retvalue = t_commit_cleanup(status, 0);	/* return value of TRUE implies	that */
						assert(retvalue); 	/* secshr_db_clnup() should have completed the commit */
						status = cdb_sc_normal;	/* reset status to normal as transaction is complete */
						assert(!csa->now_crit);	/* assert that we do not hold crit any more */
						/* At this time "cr_array_index" could be non-zero and a few cache-records might
						 * have their "in_cw_set" field set to TRUE. We should not reset "in_cw_set" as we
						 * don't hold crit at this point and also because we might still need those buffers
						 * pinned until their before-images are backed up in wcs_recover (in case an
						 * online backup was running while secshr_db_clnup did its job). Reset the
						 * local variable "cr_array_index" though so we do not accidentally reset the
						 * "in_cw_set" fields ourselves before the wcs_recover.
						 */
						cr_array_index = 0;
						goto skip_cr_array;	/* hence skip until past "cr_array_index" processing */
					}
				}
				cs->mode = gds_t_committed;
			}
		}
		/* signal secshr_db_clnup/t_commit_cleanup, roll-back is no longer possible */
		update_trans = T_COMMIT_STARTED;
		csa->t_commit_crit = FALSE;
		assert(cdb_sc_normal == status);
		INCREMENT_CURR_TN(csd);
		/* If db is journaled, then db header is flushed periodically when writing the EPOCH record,
		 * otherwise do it here every HEADER_UPDATE_COUNT transactions.
		 */
		assert(!JNL_ENABLED(csa) || (jbp == csa->jnl->jnl_buff));
		if ((!JNL_ENABLED(csa) || !JNL_HAS_EPOCH(jbp)) && !(csd->trans_hist.curr_tn & (HEADER_UPDATE_COUNT - 1)))
			fileheader_sync(gv_cur_region);
		if (need_kip_incr)		/* increment kill_in_prog */
		{
			INCR_KIP(csd, csa, kip_incremented);
			need_kip_incr = FALSE;
		}
	}
	while (cr_array_index)
	{
		assert(csa->now_crit);	/* check crit held within loop as may not enter always (and its only dbg) */
		assert(cr_array[cr_array_index-1]->in_cw_set);	/* before resetting assert that in_cw_set is indeed TRUE */
		cr_array[--cr_array_index]->in_cw_set = FALSE;
	}
	rel_crit(gv_cur_region);
	assert(!replication || update_trans);
	if (replication)
	{
		assert(QWGT(jpl->early_write_addr, jpl->write_addr));
		assert(tmp_cumul_jnl_rec_len == (tjpl->write - jpl->write +
			(tjpl->write > jpl->write ? 0 : jpl->jnlpool_size)));
		/* the following statements should be atomic */
		jnl_header = (jnldata_hdr_ptr_t)(jnlpool.jnldata_base + jpl->write);
		jnl_header->jnldata_len = tmp_cumul_jnl_rec_len;
		jnl_header->prev_jnldata_len = jpl->lastwrite_len;
		/* The following assert should be an == rather than a >= (as in tp_tend) because, we have
		 * either one or no update.  If no update, we would have no cw_depth and we wouldn't enter
		 * this path.  If there is an update, then both the indices should be 1.
		 */
		INT8_ONLY(assert(jgbl.cumul_index == jgbl.cu_jnl_index);)
		jpl->lastwrite_len = jnl_header->jnldata_len;
		/* For systems with UNORDERED memory access (example, ALPHA, POWER4, PA-RISC 2.0), on a
		 * multi processor system, it is possible that the source server notices the change in
		 * write_addr before seeing the change to jnlheader->jnldata_len, leading it to read an
		 * invalid transaction length. To avoid such conditions, we should commit the order of
		 * shared memory updates before we update write_addr. This ensures that the source server
		 * sees all shared memory updates related to a transaction before the change in write_addr
		 */
		SHM_WRITE_MEMORY_BARRIER;
		jpl->write = tjpl->write;
		/* jpl->write_addr should be updated before updating jpl->jnl_seqno as secshr_db_clnup() relies on this */
		QWINCRBYDW(jpl->write_addr, jnl_header->jnldata_len);
		assert(QWEQ(jpl->early_write_addr, jpl->write_addr));
		jpl->jnl_seqno = tjpl->jnl_seqno;
		rel_lock(jnlpool.jnlpool_dummy_reg);
	}
skip_cr_array:
	assert(!csa->now_crit);
	assert(cdb_sc_normal == status);
	if (block_saved)
		backup_buffer_flush(gv_cur_region);
	if (unhandled_stale_timer_pop)
		process_deferred_stale();
	if (update_trans)
	{
		wcs_timer_start(gv_cur_region, TRUE);
		if (REPL_ALLOWED(csa) && dse_running)
		{
			temp_tn = dbtn + 1;
			send_msg(VARLSTCNT(6) ERR_NOTREPLICATED, 4, &temp_tn, LEN_AND_LIT("DSE"), process_id);
		}
	}
	REVERT;
	/* "secshr_db_clnup/t_commit_cleanup" assume an active non-TP transaction if cw_set_depth is non-zero
	 * or if update_trans is set to T_COMMIT_STARTED. Now that the transaction is complete, reset these fields.
	 */
	cw_set_depth = 0;
	update_trans = FALSE;
	CWS_RESET;
	t_tries = 0;	/* commit was successful so reset t_tries */
	return dbtn;
failed:
	assert(cdb_sc_normal != status);
	REVERT;

failed_skip_revert:
	RESTORE_CURRTN_IF_NEEDED(csa, write_inctn, decremented_currtn);
	retvalue = t_commit_cleanup(status, 0);	/* we expect to get a return value indicating update was NOT underway */
	assert(!retvalue); 			/* if it was, then we would have done a "goto skip_cr_array:" instead */
	if (NULL != gv_target)	/* gv_target can be NULL in case of DSE MAPS command etc. */
		gv_target->clue.end = 0;
	if (release_crit && (csa->now_crit))
		rel_crit(gv_cur_region);
	t_retry(status);
	/* in the retry case, we do not do a CWS_RESET as cw_stagnate is used only in the
	 * final retry in which case t_end() will succeed and do a CWS_RESET
	 */
	cw_map_depth = 0;
	return 0;
}
