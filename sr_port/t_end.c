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

#include <stddef.h>
#include <signal.h>		/* for VSIG_ATOMIC_T type */

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
#include "iosp.h"
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
#include "gtmimagename.h"
#include "anticipatory_freeze.h"

#ifdef UNIX
#include "gtmrecv.h"
#include "deferred_signal_handler.h"
#include "repl_instance.h"
#include "format_targ_key.h"
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
#include "have_crit.h"
#ifdef GTM_SNAPSHOT
#include "db_snapshot.h"
#endif
#include "shmpool.h"
#include "bml_status_check.h"
#include "is_proc_alive.h"
#include "muextr.h"

GBLREF	bool			rc_locked;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	cache_rec_ptr_t		cr_array[((MAX_BT_DEPTH * 2) - 1) * 2]; /* Maximum number of blocks that can be in transaction */
GBLREF	unsigned int		cr_array_index;
GBLREF	boolean_t		block_saved;
GBLREF	uint4			update_trans;
GBLREF	cw_set_element		cw_set[];		/* create write set. */
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_namehead		*gv_target, *gv_target_list;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			dollar_tlevel;
GBLREF	trans_num		start_tn;
GBLREF	unsigned int		t_tries;
GBLREF	uint4			t_err, process_id;
GBLREF	unsigned char		cw_set_depth, cw_map_depth;
GBLREF	unsigned char		rdfail_detail;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl, temp_jnlpool_ctl;
GBLREF	boolean_t		is_updproc;
GBLREF	seq_num			seq_num_one;
GBLREF	boolean_t		mu_reorg_process;
GBLREF	boolean_t		unhandled_stale_timer_pop;
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF	sgmnt_addrs 		*kip_csa;
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
GBLREF	boolean_t		block_is_free;
GBLREF	boolean_t		gv_play_duplicate_kills;
GBLREF	boolean_t		pool_init;
GBLREF	gv_key			*gv_currkey;
#ifdef GTM_TRIGGER
GBLREF	boolean_t		skip_dbtriggers;	/* see gbldefs.c for description of this global */
#endif
#ifdef UNIX
GBLREF	recvpool_addrs		recvpool;
GBLREF	int4			strm_index;
#endif
#ifdef DEBUG
GBLREF	boolean_t		mupip_jnl_recover;
#endif

error_def(ERR_GBLOFLOW);
error_def(ERR_GVKILLFAIL);
error_def(ERR_GVPUTFAIL);
error_def(ERR_JNLFILOPN);
error_def(ERR_JNLFLUSH);
error_def(ERR_JRTNULLFAIL);
error_def(ERR_NOTREPLICATED);
error_def(ERR_TEXT);

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

/* This macro isn't enclosed in parantheses to allow for optimizations */
#define VALIDATE_CYCLE(is_mm, history)					\
if (history)								\
{									\
	for (t1 = history->h;  t1->blk_num;  t1++)			\
	{								\
		if (!is_mm && (t1->cr->cycle != t1->cycle))		\
		{	/* cache slot has been stolen */		\
			assert(!csa->now_crit || csa->hold_onto_crit);	\
			status = cdb_sc_cyclefail;			\
			goto failed_skip_revert;			\
		}							\
		n_blks_validated++;					\
	}								\
}

#define	BUSY2FREE	0x00000001
#define	RECYCLED2FREE	0x00000002
#define	FREE_DIR_DATA	0x00000004	/* denotes the block to be freed is a data block in directory tree */

#define SAVE_2FREE_IMAGE(MODE, FREE_SEEN, CSD)								\
	 (((gds_t_busy2free == MODE) && (!CSD->db_got_to_v5_once || (FREE_SEEN & FREE_DIR_DATA)))	\
	|| (gds_t_recycled2free == MODE))

trans_num t_end(srch_hist *hist1, srch_hist *hist2, trans_num ctn)
{
	srch_hist		*hist;
	bt_rec_ptr_t		bt;
	boolean_t		blk_used;
	cache_rec		cr_save;
	cache_rec_ptr_t		cr, backup_cr;
	cw_set_element		*cs, *cs_top, *cs1;
	enum cdb_sc		status;
	int			int_depth, tmpi;
	uint4			jnl_status;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp, jbbp; /* jbp is non-NULL if journaling, jbbp is non-NULL only if before-image journaling */
	sgmnt_addrs		*csa, *repl_csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	sgm_info		*dummysi = NULL;	/* needed as a dummy parameter for {mm,bg}_update */
	srch_blk_status		*t1;
	trans_num		valid_thru, oldest_hist_tn, dbtn, blktn, temp_tn, epoch_tn, old_block_tn;
	unsigned char		cw_depth, cw_bmp_depth, buff[MAX_ZWR_KEY_SZ], *end;
	jnldata_hdr_ptr_t	jnl_header;
	uint4			total_jnl_rec_size, tmp_cumul_jnl_rec_len, tmp_cw_set_depth, prev_cw_set_depth;
	DEBUG_ONLY(unsigned int	tot_jrec_size;)
	jnlpool_ctl_ptr_t	jpl, tjpl;
	boolean_t		replication = FALSE;
#	ifdef UNIX
	boolean_t		supplementary = FALSE;	/* this variable is initialized ONLY if "replication" is TRUE. */
	seq_num			strm_seqno, next_strm_seqno;
#	endif
	sm_uc_ptr_t		blk_ptr, backup_blk_ptr;
	int			blkid;
	boolean_t		is_mm;
	boolean_t		read_before_image; /* TRUE if before-image journaling or online backup in progress
						    * This is used to read before-images of blocks whose cs->mode is gds_t_create */
	boolean_t		write_inctn = FALSE;	/* set to TRUE in case writing an inctn record is necessary */
	boolean_t		decremented_currtn, retvalue, recompute_cksum, cksum_needed;
	unsigned int		free_seen; /* free_seen denotes the block is going to be set free rather than recycled */
	boolean_t		in_mu_truncate = FALSE, jnlpool_crit_acquired = FALSE;
	boolean_t		was_crit;
	blk_hdr_ptr_t		old_block;
	unsigned int		bsiz, crindex;
	jnl_tm_t		save_gbl_jrec_time;
	enum gds_t_mode		mode;
	uint4			prev_cr_array_index;
	uint4			com_csum;
#	ifdef DEBUG
	boolean_t		ready2signal_gvundef_lcl;
	enum cdb_sc		prev_status;
	GTMCRYPT_ONLY(
		blk_hdr_ptr_t	save_old_block;
	)
#	endif
	int			n_blks_validated;
	boolean_t		before_image_needed, lcl_ss_in_prog = FALSE, reorg_ss_in_prog = FALSE;
	boolean_t		ss_need_to_restart, new_bkup_started;
	gv_namehead		*gvnh;
#	ifdef GTM_TRIGGER
	uint4			cycle;
#	endif
#	ifdef GTM_SNAPSHOT
	snapshot_context_ptr_t  lcl_ss_ctx;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Currently, the only callers of t_end with NULL histories are the update process and journal recovery when they
	 * are about to process a JRT_NULL record. Assert that.
	 */
	assert((hist1 != hist2) || (ERR_JRTNULLFAIL == t_err) && (NULL == hist1)
					&& update_trans && (is_updproc || jgbl.forw_phase_recovery));
	DEBUG_ONLY(
		/* Store global variable ready2signal_gvundef in a local variable and reset the global right away to ensure that
		 * the global value does not incorrectly get carried over to the next call of "t_end".
		 */
		ready2signal_gvundef_lcl = TREF(ready2signal_gvundef);
		TREF(ready2signal_gvundef) = FALSE;
	)
	csa = cs_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	is_mm = (dba_mm == csd->acc_meth);
#	ifdef GTM_TRUNCATE
	DEBUG_ONLY(in_mu_truncate = (cnl != NULL && process_id == cnl->trunc_pid);)
#	endif
	TREF(rlbk_during_redo_root) = FALSE;
	status = cdb_sc_normal;
	/* The only cases where we set csa->hold_onto_crit to TRUE are the following :
	 * (a) jgbl.onlnrlbk
	 * (b) DSE CRIT -SEIZE (and any command that follows it), DSE CHANGE -BLOCK, DSE ALL -SEIZE (and any command that follows)
	 *	and DSE MAPS -RESTORE_ALL. Since we cannot distinguish between different DSE qualifiers, we use IS_DSE_IMAGE.
	 * (c) gvcst_redo_root_search in the final retry.
	 *
	 * Since we don't expect hold_onto_crit to be set by any other utility/function, the below assert is valid and is intended
	 * to catch cases where the field is inadvertently set to TRUE.
	 */
	assert(!csa->hold_onto_crit || IS_DSE_IMAGE UNIX_ONLY(|| jgbl.onlnrlbk || TREF(in_gvcst_redo_root_search)));
	assert(cs_data == csd);
	assert((t_tries < CDB_STAGNATE) || csa->now_crit);
	assert(!dollar_tlevel);
	/* whenever cw_set_depth is non-zero, ensure update_trans is also non-zero */
	assert(!cw_set_depth || (UPDTRNS_DB_UPDATED_MASK == update_trans));
	/* whenever cw_set_depth is zero, ensure that update_trans is FALSE except when it is a duplicate set or a duplicate kill
	 * or a NULL journal record.
	 */
	assert(cw_set_depth || !update_trans || ((ERR_GVPUTFAIL == t_err) && gvdupsetnoop)
		|| (ERR_JRTNULLFAIL == t_err) || ((ERR_GVKILLFAIL == t_err) && gv_play_duplicate_kills));
	assert(0 == cr_array_index);
	assert(!gv_cur_region->read_only || !update_trans);
	cr_array_index = 0;	/* be safe and reset it in PRO even if it is not zero */
	if (cnl->wc_blocked || (is_mm && (csa->total_blks != csa->ti->total_blks)))
	{	/* If blocked, or we have MM and file has been extended, force repair */
		status = cdb_sc_helpedout;	/* force retry with special status so philanthropy isn't punished */
		assert((CDB_STAGNATE > t_tries) || !is_mm || (csa->total_blks == csa->ti->total_blks));
		goto failed_skip_revert;
	} else
	{
		/* See if we can take a fast path for read transactions based on the following conditions :
		 * 1. If the transaction number hasn't changed since we read the blocks from the disk or cache
		 * 2. If NO concurrent online rollback is running. This is needed because we don't want read transactions
		 *    to succeed. The issue with this check is that for a rollback that was killed, the PID will be non-zero.
		 *    In that case, we might skip the fast path and go ahead and do the validation. The validation logic
		 *    gets crit anyways and so will salvage the lock and do the necessary recovery and issue DBFLCORRP if
		 *    it notices that csd->file_corrupt is TRUE.
		 */
		if (!update_trans && (start_tn == csa->ti->early_tn) UNIX_ONLY(&& (0 == csa->nl->onln_rlbk_pid)))
		{	/* read with no change to the transaction history */
			n_blks_validated = 0;
			VALIDATE_CYCLE(is_mm, hist1);	/* updates n_blks_validated */
			VALIDATE_CYCLE(is_mm, hist2);	/* updates n_blks_validated */
			assert(cdb_sc_normal == status);
#			ifdef UNIX
			if (MISMATCH_ROOT_CYCLES(csa, cnl))
			{	/* If a root block has moved, we might have started the read from the wrong root block, in which
				 * case we cannot trust the entire search. Need to redo root search.
				 */
				was_crit = csa->now_crit;
				if (!was_crit)
					grab_crit(gv_cur_region);
				status = cdb_sc_gvtrootmod2;
				if (MISMATCH_ONLN_RLBK_CYCLES(csa, cnl))
				{
					assert(!mupip_jnl_recover);
					status = ONLN_RLBK_STATUS(csa, cnl);
					SYNC_ONLN_RLBK_CYCLES;
					SYNC_ROOT_CYCLES(NULL);
				} else
					SYNC_ROOT_CYCLES(csa);
				if (!was_crit && !csa->hold_onto_crit)
					rel_crit(gv_cur_region);
				goto failed_skip_revert;
			}
#			endif
			/* Assert that if gtm_gvundef_fatal is non-zero, then we better not be about to signal a GVUNDEF */
			assert(!TREF(gtm_gvundef_fatal) || !ready2signal_gvundef_lcl);
			if (csa->now_crit && !csa->hold_onto_crit)
				rel_crit(gv_cur_region);
			if (unhandled_stale_timer_pop)
				process_deferred_stale();
			CWS_RESET;
			assert(!csa->now_crit || csa->hold_onto_crit); /* shouldn't hold crit unless asked to */
			t_tries = 0;	/* commit was successful so reset t_tries */
			INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_readonly, 1);
			INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_blkread, n_blks_validated);
			return csa->ti->curr_tn;
		}
	}
	assert((gds_t_committed < gds_t_busy2free)	&& (n_gds_t_op > gds_t_busy2free));
	assert((gds_t_committed < gds_t_recycled2free)	&& (n_gds_t_op > gds_t_recycled2free));
	assert((gds_t_committed < gds_t_write_root)	&& (n_gds_t_op > gds_t_write_root));
	free_seen = 0;
	cw_depth = cw_set_depth;
	cw_bmp_depth = cw_depth;
	if ((0 != cw_set_depth) && (gds_t_writemap == cw_set[cw_set_depth - 1].mode))
	{
		if (1 == cw_set_depth)
		{
			cw_depth = 0;
			cw_bmp_depth = 0;
		} else if (gds_t_busy2free == cw_set[0].mode)
		{
			assert(TREF(in_gvcst_bmp_mark_free));
			free_seen |= BUSY2FREE;
			if (CSE_LEVEL_DRT_LVL0_FREE == cw_set[0].level)
			{	/* the block is in fact a level-0 block in the directory tree */
				assert(MUSWP_FREE_BLK == TREF(in_mu_swap_root_state));
				free_seen |= FREE_DIR_DATA;
			}
			assert(2 == cw_set_depth);
			cw_depth = 0;
			cw_bmp_depth = 1;
		} else if (gds_t_recycled2free == cw_set[0].mode)
		{	/* in phase 1 of MUPIP REORG -TRUNCATE, and we need to free a recycled block */
			assert(in_mu_truncate);
			free_seen |= RECYCLED2FREE;
			assert(2 == cw_set_depth);
			cw_depth = 0;
			cw_bmp_depth = 1;
		}
	}
#	ifdef GTM_SNAPSHOT
	if (update_trans && SNAPSHOTS_IN_PROG(cnl))
	{
		/* If snapshot context is not already created, then create one now to be used by this transaction. If context
		 * creation failed (for instance, on snapshot file open fail), then SS_INIT_IF_NEEDED sets csa->snapshot_in_prog
		 * to FALSE.
		 */
		SS_INIT_IF_NEEDED(csa, cnl);
	} else
		CLEAR_SNAPSHOTS_IN_PROG(csa);
#	endif
	if (0 != cw_depth)
	{	/* Caution : since csa->backup_in_prog and read_before_image are initialized below
	 	 * only if (cw_depth), these variables should be used below only within an if (cw_depth).
		 */
#		ifdef GTM_SNAPSHOT
		lcl_ss_in_prog = SNAPSHOTS_IN_PROG(csa); /* store in local variable to avoid pointer access */
		reorg_ss_in_prog = (mu_reorg_process && lcl_ss_in_prog); /* store in local variable if both snapshots and MUPIP
									  * REORG are in progress */
#		endif
		assert(SIZEOF(bsiz) == SIZEOF(old_block->bsiz));
		assert(update_trans);
		csa->backup_in_prog = (BACKUP_NOT_IN_PROGRESS != cnl->nbb);
		jbbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
		read_before_image = ((NULL != jbbp) || csa->backup_in_prog || lcl_ss_in_prog);
		for (cs = cw_set, cs_top = cs + cw_depth; cs < cs_top; cs++)
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
#						ifdef UNIX
						if (is_mm && (cdb_sc_gbloflow == status))
						{
							assert(NULL != gv_currkey);
							if (NULL == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
								end = &buff[MAX_ZWR_KEY_SZ - 1];
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_GBLOFLOW, 0,
									ERR_GVIS, 2, end - buff, buff);
							rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_GBLOFLOW, 0,
									ERR_GVIS, 2, end - buff, buff);
						}
#						endif
					}
					goto failed_skip_revert;
				}
				assert(!is_mm || (cs->blk < csa->total_blks));
				assert((CDB_STAGNATE > t_tries) || (cs->blk < csa->ti->total_blks));
				blk_used ? BIT_SET_RECYCLED_AND_CLEAR_FREE(cs->blk_prior_state)
					 : BIT_CLEAR_RECYCLED_AND_SET_FREE(cs->blk_prior_state);
				BEFORE_IMAGE_NEEDED(read_before_image, cs, csa, csd, cs->blk, before_image_needed);
				if (!before_image_needed)
					cs->old_block = NULL;
				else
				{
					block_is_free = WAS_FREE(cs->blk_prior_state);
					cs->old_block = t_qread(cs->blk, (sm_int_ptr_t)&cs->cycle, &cs->cr);
					old_block = (blk_hdr_ptr_t)cs->old_block;
					if (NULL == old_block)
					{
						status = (enum cdb_sc)rdfail_detail;
						goto failed_skip_revert;
					}
					ASSERT_IS_WITHIN_SHM_BOUNDS((sm_uc_ptr_t)cs->old_block, csa);
					if (!WAS_FREE(cs->blk_prior_state) && (NULL != jbbp) && (old_block->tn < jbbp->epoch_tn))
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
						JNL_GET_CHECKSUM_ACQUIRED_BLK(cs, csd, csa, old_block, bsiz);
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
			} else if (reorg_ss_in_prog && WAS_FREE(cs->blk_prior_state))
			{
				assert((gds_t_acquired == cs->mode) && (NULL == cs->old_block));
				/* If snapshots are in progress, we might want to read the before images of the FREE blocks also.
				 * Since mu_swap_blk mimics a small part of t_end, it sets cse->mode to gds_t_acquired and hence
				 * will not read the before images of the FREE blocks in t_end. To workaround this, set
				 * cse->blk_prior_state's free status to TRUE so that in t_end, this condition can be used to read
				 * the before images of the FREE blocks if needed.
				 */
				BEFORE_IMAGE_NEEDED(read_before_image, cs, csa, csd, cs->blk, before_image_needed);
				if (before_image_needed)
				{
					block_is_free = TRUE; /* To tell t_qread that the block it's trying to read is
							       * actually a FREE block */
					cs->old_block = t_qread(cs->blk, (sm_int_ptr_t)&cs->cycle, &cs->cr);
					if (NULL == cs->old_block)
					{
						status = (enum cdb_sc)rdfail_detail;
						goto failed_skip_revert;
					}
					ASSERT_IS_WITHIN_SHM_BOUNDS((sm_uc_ptr_t)cs->old_block, csa);
				}
			}
		}
	}
	if (update_trans && JNL_ENABLED(csa))
	{	/* compute the total journal record size requirements before grab_crit.
		 * there is code later that will check for state changes from now to then and if so do a recomputation
		 */
		assert(!cw_map_depth || cw_set_depth < cw_map_depth);
		tmp_cw_set_depth = cw_map_depth ? cw_map_depth : cw_set_depth;
		TOTAL_NONTPJNL_REC_SIZE(total_jnl_rec_size, non_tp_jfb_ptr, csa, tmp_cw_set_depth);
		/* For a non-tp update maximum journal space we may need is total size of
		 * 	1) space for maximum CDB_CW_SET_SIZE PBLKs, that is, MAX_MAX_NONTP_JNL_REC_SIZE * CDB_CW_SET_SIZE
		 *	2) space for a logical record itself, that is, MAX_LOGI_JNL_REC_SIZE and
		 * 	3) overhead records (MIN_TOTAL_NONTPJNL_REC_SIZE + JNL_FILE_TAIL_PRESERVE)
		 * This requirement is less than the minimum autoswitchlimit size (JNL_AUTOSWITCHLIMIT_MIN) as asserted below.
		 * Therefore we do not need any check to issue JNLTRANS2BIG error like is being done in tp_tend.c
		 */
		assert((CDB_CW_SET_SIZE * MAX_MAX_NONTP_JNL_REC_SIZE + MAX_LOGI_JNL_REC_SIZE +
			MIN_TOTAL_NONTPJNL_REC_SIZE + JNL_FILE_TAIL_PRESERVE) <= (JNL_AUTOSWITCHLIMIT_MIN * DISK_BLOCK_SIZE));
		DEBUG_ONLY(tot_jrec_size = MAX_REQD_JNL_FILE_SIZE(total_jnl_rec_size));
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
	ESTABLISH_NOUNWIND(t_ch);	/* avoid hefty setjmp call, which is ok since we never unwind t_ch */
	assert(!csa->hold_onto_crit || csa->now_crit);
	if (!csa->now_crit)
	{
		if (update_trans)
		{	/* Get more space if needed. This is done outside crit so that
			 * any necessary IO has a chance of occurring outside crit.
			 * The available space must be double-checked inside crit. */
			if (!is_mm && !WCS_GET_SPACE(gv_cur_region, cw_set_depth + 1, NULL))
				assert(FALSE);	/* wcs_get_space should have returned TRUE unconditionally in this case */
			for (;;)
			{
				grab_crit(gv_cur_region);
				if (FALSE == csd->freeze)
					break;
				rel_crit(gv_cur_region);
				/* We are about to wait for freeze. Assert that we are not in phase2 of a bitmap free operation
				 * (part of an M-kill or REORG operation). The freeze must have waited for the phase2 to complete.
				 */
				assert((inctn_bmp_mark_free_gtm != inctn_opcode) && (inctn_bmp_mark_free_mu_reorg != inctn_opcode));
				while (csd->freeze)
					hiber_start(1000);
			}
		} else
			grab_crit(gv_cur_region);
	} else
	{	/* We expect the process to be in its final retry as it is holding crit. The only exception is if hold_onto_crit
		 * is TRUE but in that case we dont expect csd->freeze to be TRUE so we dont care much about that case. The other
		 * exception is if this is DSE which gets crit even without being in the final retry. In that case, skip the check
		 * about whether we are about to update a frozen db. DSE is the only utility allowed to update frozen databases.
		 */
		assert((CDB_STAGNATE == t_tries) || csa->hold_onto_crit || IS_DSE_IMAGE);
		if (csd->freeze && update_trans && !IS_DSE_IMAGE)
		{	/* We are about to update a frozen database. This is possible in rare cases even though
			 * we waited for the freeze to be lifted in t_retry (see GTM-7004). Restart in this case.
			 */
			status = cdb_sc_needcrit;
			goto failed;
		}
	}
	com_csum = 0;
	/* We should never proceed to update a frozen database. Only exception is DSE */
	assert(!update_trans || !csd->freeze || IS_DSE_IMAGE);
#	ifdef UNIX
	/* We never expect to come here with file_corrupt set to TRUE (in case of an online rollback) because
	 * grab_crit done above will make sure of that. The only exception is RECOVER/ROLLBACK itself coming
	 * here in the forward phase
	 */
	assert(!csd->file_corrupt || mupip_jnl_recover);
	if (MISMATCH_ROOT_CYCLES(csa, cnl))
	{
		status = cdb_sc_gvtrootmod2;
		if (MISMATCH_ONLN_RLBK_CYCLES(csa, cnl))
		{
			assert(!mupip_jnl_recover);
			status = ONLN_RLBK_STATUS(csa, cnl);
			SYNC_ONLN_RLBK_CYCLES;
			if (!TREF(in_gvcst_bmp_mark_free) || mu_reorg_process)
				SYNC_ROOT_CYCLES(NULL);
		} else
			SYNC_ROOT_CYCLES(csa);
		goto failed;
	}
	/* We should never proceed to commit if the global variable - only_reset_clues_if_onln_rlbk - is TRUE AND if the prior
	 * retry was due to ONLINE ROLLBACK. This way, we ensure that, whoever set the global variable knows to handle ONLINE
	 * ROLLBACK and resets it before returning control to the application.
	 */
	DEBUG_ONLY(prev_status = LAST_RESTART_CODE);
	assert((cdb_sc_normal == prev_status) || ((cdb_sc_onln_rlbk1 != prev_status) && (cdb_sc_onln_rlbk2 != prev_status))
		|| (!TREF(in_gvcst_bmp_mark_free) || mu_reorg_process));
#	endif
	if (is_mm && ((csa->hdr != csd) || (csa->total_blks != csa->ti->total_blks)))
        {       /* If MM, check if wcs_mm_recover was invoked as part of the grab_crit done above OR if
                 * the file has been extended. If so, restart.
                 */
                status = cdb_sc_helpedout;      /* force retry with special status so philanthropy isn't punished */
                goto failed;
        }
	assert(!cw_depth || update_trans);
#	ifdef GTM_TRIGGER
	if (!skip_dbtriggers)
	{
		cycle = csd->db_trigger_cycle;
		if (csa->db_trigger_cycle != cycle)
		{	/* the process' view of the triggers could be potentially stale. restart to be safe. */
			/* On an originating instance, in addition to the run-time, utilities can collide with
			 * with concurrent triggers definition updates
			 * The following asserts verify that:
			 * (1) Activities on a replicating instance don't see concurrent trigger changes as update
			 * process is the only updater in the replicating instance. The only exception is if this
			 * is a supplementary root primary instance. In that case, the update process coexists with
			 * GT.M processes and hence can see restarts due to concurrent trigger changes.
			 * (2) Journal recover operates in standalone mode. So, it should NOT see any concurrent
			 * trigger changes as well
			 */
			assert(!is_updproc || jnlpool.repl_inst_filehdr->is_supplementary && !jnlpool.jnlpool_ctl->upd_disabled);
			assert(!jgbl.forw_phase_recovery);
			assert(cycle > csa->db_trigger_cycle);
			/* csa->db_trigger_cycle will be set to csd->db_trigger_cycle in t_retry */
			status = cdb_sc_triggermod;
			goto failed;
		}
	}
#	endif
	/* If inctn_opcode has a valid value, then we better be doing an update. The only exception to this rule is if we are
	 * in MUPIP REORG UPGRADE/DOWNGRADE (mu_reorg_upgrd_dwngrd.c) where update_trans is explicitly set to 0 in some cases.
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
				/* jnl_file_lost causes a jnl_state transition from jnl_open to jnl_closed
				 * and additionally causes a repl_state transition from repl_open to repl_closed
				 * all without standalone access. This means that csa->repl_state might be repl_open
				 * while csd->repl_state might be repl_closed. update csa->repl_state in this case
				 * as otherwise the rest of the code might look at csa->repl_state and incorrectly
				 * conclude replication is on and generate sequence numbers when actually no journal
				 * records are being generated. [C9D01-002219]
				 */
				csa->repl_state = csd->repl_state;
				status = cdb_sc_jnlstatemod;
				goto failed;
			}
		}
		/* Flag retry, if other mupip activities like BACKUP, INTEG or FREEZE are in progress.
		 * If in final retry, go ahead with kill. BACKUP/INTEG/FREEZE will wait for us to be done.
		 */
		if (need_kip_incr && (0 < cnl->inhibit_kills) && (CDB_STAGNATE > t_tries))
		{
			status = cdb_sc_inhibitkills;
			goto failed;
		}
		ss_need_to_restart = new_bkup_started = FALSE;
		GTM_SNAPSHOT_ONLY(
			if (update_trans)
				CHK_AND_UPDATE_SNAPSHOT_STATE_IF_NEEDED(csa, cnl, ss_need_to_restart);
		)
		if (cw_depth)
		{
			assert(update_trans);
			CHK_AND_UPDATE_BKUP_STATE_IF_NEEDED(cnl, csa, new_bkup_started);
			/* recalculate based on the new values of snapshot_in_prog and backup_in_prog. Since read_before_image used
			 * only in the context of acquired blocks, recalculation should happen only for non-zero cw_depth
			 */
			read_before_image = ((JNL_ENABLED(csa) && csa->jnl_before_image)
					     || csa->backup_in_prog
					     || SNAPSHOTS_IN_PROG(csa));
		}
		if ((cw_depth && new_bkup_started) || (update_trans && ss_need_to_restart))
		{
			if (ss_need_to_restart || (new_bkup_started && !(JNL_ENABLED(csa) && csa->jnl_before_image)))
			{
				/* If online backup is in progress now and before-image journaling is not enabled,
				 * we would not have read before-images for created blocks. Although it is possible
				 * that this transaction might not have blocks with gds_t_create at all, we expect
				 * this backup_in_prog state change to be so rare that it is ok to restart.
				 */
				status = cdb_sc_bkupss_statemod;
				goto failed;
			}
		}
		/* in crit, ensure cache-space is available. the out-of-crit check done above might not have been enough */
		if (!is_mm && !WCS_GET_SPACE(gv_cur_region, cw_set_depth + 1, NULL))
		{
			assert(cnl->wc_blocked);	/* only reason we currently know why wcs_get_space could fail */
			assert(gtm_white_box_test_case_enabled);
			SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
			BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_hist);
			SET_CACHE_FAIL_STATUS(status, csd);
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
		oldest_hist_tn = OLDEST_HIST_TN(csa);
	if (update_trans)
		valid_thru++;
	n_blks_validated = 0;
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
				t1->cse = NULL;	/* reset for next transaction */
			} else
			{
				bt = bt_get(t1->blk_num);
				if (NULL == bt)
				{
					if (t1->tn <= oldest_hist_tn)
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
							SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_crbtmismatch1);
							status = cdb_sc_crbtmismatch;
							goto failed;
						}
					}
					assert(bt->killtn <= bt->tn);
					if (t1->tn <= bt->tn)
					{
						assert((CDB_STAGNATE > t_tries) || (cdb_sc_helpedout == t_fail_hist[t_tries]));
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
							status = gvincr_recompute_upd_array(t1, cw_set, cr);
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
					SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
					BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_hist);
					SET_CACHE_FAIL_STATUS(status, csd);
					goto failed;
				}
				if ((NULL == cr) || (cr->cycle != t1->cycle)
					|| ((sm_long_t)GDS_REL2ABS(cr->buffaddr) != (sm_long_t)t1->buffaddr))
				{
					if ((NULL != cr) && (NULL != bt) && (cr->blk != bt->blk))
					{
						assert(FALSE);
						SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_crbtmismatch2);
						status = cdb_sc_crbtmismatch;
						goto failed;
					}
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_lostcr;
					goto failed;
				}
				assert(0 == cr->in_tend);
				/* Pin those buffers that we are planning on updating. Those are easily identified as the ones
				 * where the history has a non-zero cw-set-element.
				 */
				cs = t1->cse;
				if (cs)
				{
					if (n_gds_t_op > cs->mode)
					{
						assert(update_trans);
						PIN_CACHE_RECORD(cr, cr_array, cr_array_index);
						/* If cs->mode is gds_t_busy2free, then the corresponding cache-record needs
						 * to be pinned to write the before-image right away but this cse is not going
						 * to go through bg_update. So remember to unpin the cache-record before phase2
						 * as otherwise the pre-phase2 check (that we have pinned only those cache-records
						 * that we are planning to update) will fail. But to do that, we rely on the fact
						 * that the cache-record corresponding to the gds_t_busy2free cse is always the
						 * first one in the cr_array.
						 */
						assert((gds_t_committed > cs->mode)
							|| (gds_t_busy2free == cs->mode) || (gds_t_recycled2free == cs->mode));
						assert((gds_t_busy2free != cs->mode) || (1 == cr_array_index));
						assert((gds_t_recycled2free != cs->mode) || (1 == cr_array_index));
						assert((gds_t_busy2free != cs->mode) || (free_seen & BUSY2FREE));
						assert((gds_t_recycled2free != cs->mode) || (free_seen & RECYCLED2FREE));
					}
					t1->cse = NULL;	/* reset for next transaction */
				}
			}
			t1->tn = valid_thru;
			n_blks_validated++;
		}
		assert((hist != hist2) || (t1 != hist->h));
	}
#	ifdef DEBUG
	/* If clue is non-zero, validate it (BEFORE this could be used in a future transaction). The only exception is reorg
	 * where we could have an invalid clue (e.g. last_rec < first_rec etc.). This is because reorg shuffles records around
	 * heavily and therefore it is hard to maintain an uptodate clue. reorg therefore handles this situation by actually
	 * resetting the clue just before doing the next gvcst_search. The mu_reorg* routines already take care of this reset
	 * (in fact, this is asserted in gvcst_search too). So we can allow invalid clues here in that special case.
	 */
	if (!mu_reorg_process && (NULL != gv_target) && gv_target->clue.end)
		/* gv_target can be NULL in case of DSE MAPS etc. */
		DEBUG_GVT_CLUE_VALIDATE(gv_target);	/* Validate that gvt has valid first_rec, clue & last_rec fields */
#	endif
	/* Assert that if gtm_gvundef_fatal is non-zero, then we better not be about to signal a GVUNDEF */
	assert(!TREF(gtm_gvundef_fatal) || !ready2signal_gvundef_lcl);
	/* check bit maps for usage */
	if (0 != cw_map_depth)
	{	/* Bit maps on end from mu_reorg (from a call to mu_swap_blk) or mu_reorg_upgrd_dwngrd */
		prev_cw_set_depth = cw_set_depth;
		prev_cr_array_index = cr_array_index;	/* note down current depth of pinned cache-records */
		cw_set_depth = cw_map_depth;
	}
	for (cs = &cw_set[cw_bmp_depth], cs_top = &cw_set[cw_set_depth]; cs < cs_top; cs++)
	{
		assert(0 == cs->jnl_freeaddr);	/* ensure haven't missed out resetting jnl_freeaddr for any cse in
						 * t_write/t_create/{t,mu}_write_map/t_write_root [D9B11-001991]
						 */
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
				if (cs->tn <= oldest_hist_tn)
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
						SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_crbtmismatch3);
						status = cdb_sc_crbtmismatch;
						goto failed;
					}
				}
			}
			if ((cache_rec_ptr_t)CR_NOTVALID == cr)
			{
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_bitmap_nullbt);
				SET_CACHE_FAIL_STATUS(status, csd);
				goto failed;
			}
			if ((NULL == cr)  || (cr->cycle != cs->cycle) ||
				((sm_long_t)GDS_REL2ABS(cr->buffaddr) != (sm_long_t)cs->old_block))
			{
				assert(CDB_STAGNATE > t_tries);
				status = cdb_sc_lostbmlcr;
				goto failed;
			}
			PIN_CACHE_RECORD(cr, cr_array, cr_array_index);
		}
	}
	if ((0 != cw_map_depth) && mu_reorg_upgrd_dwngrd_in_prog)
	{	/* Bit maps on end from mu_reorg_upgrd_dwngrd. Bitmap history has been validated.
		 * But we do not want bitmap cse to be considered for bg_update. Reset cw_set_depth accordingly.
		 */
		cw_set_depth = prev_cw_set_depth;
		assert(1 >= cw_set_depth);
		assert(2 >= cw_map_depth);
		/* UNPIN the bitmap cache record we no longer need */
		assert(prev_cr_array_index <= cr_array_index);
		if (prev_cr_array_index < cr_array_index)
		{
			cr_array_index--;
			assert(prev_cr_array_index == cr_array_index);
			assert(process_id == cr_array[cr_array_index]->in_cw_set);
			UNPIN_CACHE_RECORD(cr_array[cr_array_index]);
		}
	}
	assert(csd == csa->hdr);
	assert(!need_kip_incr || update_trans UNIX_ONLY(|| TREF(in_gvcst_redo_root_search)));
	/* At this point if the transaction has no updates, we are done with validation (all possibilities of "goto failed")
	 * and so we need to assert that donot_commit better be set to FALSE at this point. For transaction with updates,
	 * there are a few more "goto failed" usages below so we will do this check separately after those usages.
	 */
	assert(update_trans || !TREF(donot_commit));	/* We should never commit a transaction that was determined restartable */
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
				 * prior content of the block (for online backup or before-image journaling
				 * or online integ) and did not rely on it for constructing the transaction.
				 * Restart if block is not present in cache now or is being read in currently.
				 */
				assert((gds_t_busy2free != cs->mode) && (gds_t_recycled2free != cs->mode));
				if ((gds_t_acquired == cs->mode) && (NULL != cs->old_block))
				{
					assert(read_before_image == ((JNL_ENABLED(csa) && csa->jnl_before_image)
								     || csa->backup_in_prog
								     || SNAPSHOTS_IN_PROG(csa)));
					cr = db_csh_get(cs->blk);
					if ((cache_rec_ptr_t)CR_NOTVALID == cr)
					{
						SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_jnl_cwset);
						SET_CACHE_FAIL_STATUS(status, csd);
						goto failed;
					}
					/* It is possible that cr->in_cw_set is non-zero in case a concurrent MUPIP REORG
					 * UPGRADE/DOWNGRADE is in PHASE2 touching this very same block. In that case,
					 * we cannot reuse this block so we restart. We could try finding a different block
					 * to acquire instead and avoid a restart (tracked as part of C9E11-002651).
					 * Note that in_cw_set is set to 0 ahead of in_tend in bg_update_phase2. Therefore
					 * it is possible that we see in_cw_set 0 but in_tend is still non-zero. In that case,
					 * we cannot proceed with pinning this cache-record as the cr is still locked by
					 * the other process. We can choose to wait here but instead decide to restart.
					 */
					if ((NULL == cr) || (0 <= cr->read_in_progress)
						|| (0 != cr->in_cw_set) || (0 != cr->in_tend))
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_lostbefor;
						goto failed;
					}
					PIN_CACHE_RECORD(cr, cr_array, cr_array_index);
					cs->ondsk_blkver = cr->ondsk_blkver;
					old_block = (blk_hdr_ptr_t)GDS_REL2ABS(cr->buffaddr);
					assert((cs->cr != cr) || (cs->old_block == (sm_uc_ptr_t)old_block));
					old_block_tn = old_block->tn;
					/* Need checksums if before imaging and if a PBLK record is going to be written. However,
					 * while doing the bm_getfree, if we got a free block, then no need to compute checksum
					 * as we would NOT be writing before images of free blocks to journal files
					 */
					cksum_needed = (!WAS_FREE(cs->blk_prior_state) && (NULL != jbbp)
								&& (old_block_tn < jbbp->epoch_tn));
					if ((cs->cr != cr) || (cs->cycle != cr->cycle))
					{	/* Block has relocated in the cache. Adjust pointers to new location. */
						cs->cr = cr;
						cs->cycle = cr->cycle;
						cs->old_block = (sm_uc_ptr_t)old_block;
						/* PBLK checksum was computed outside-of-crit when block was read but
						 * block has relocated in the cache since then so recompute the checksum
						 * if this block needs a checksum in the first place (cksum_needed is TRUE).
						 */
						recompute_cksum = cksum_needed;
					} else if (cksum_needed)
					{	/* We have determined that a checksum is needed for this block. If we have not
						 * previously computed one outside crit OR if the block contents have changed
						 * since the checksum was previously computed, we need to recompute it.
						 * Otherwise, the out-of-crit computed value can be safely used.
						 * Note that cs->tn is valid only if a checksum was computed outside of crit.
						 * So make sure it is used only if checksum is non-zero. There is a rare chance
						 * that the computed checksum could be zero in which case we will recompute
						 * unnecessarily. Since that is expected to be very rare, it is considered ok.
						 */
						recompute_cksum = (!cs->blk_checksum || (cs->tn <= old_block_tn));
					}
					if (!cksum_needed)
						cs->blk_checksum = 0;	/* zero any out-of-crit computed checksum */
					else if (recompute_cksum)
					{	/* We hold crit at this point so we are guaranteed valid bsiz field.
						 * Hence we do not need to take MIN(bsiz, csd->blk_size) like we did
						 * in the earlier call to jnl_get_checksum.
						 */
						assert(NULL != jbbp);
						assert(SIZEOF(bsiz) == SIZEOF(old_block->bsiz));
						bsiz = old_block->bsiz;
						assert(bsiz <= csd->blk_size);
						cs->blk_checksum = jnl_get_checksum((uint4*)old_block, csa, bsiz);
					}
					DEBUG_ONLY(
					else
						assert(cs->blk_checksum == jnl_get_checksum((uint4 *)old_block,
												csa, old_block->bsiz));
					)
					assert(cs->cr->blk == cs->blk);
				}
			}
		}
		/* if we are not writing an INCTN record, we better have a non-zero cw_depth.
		 * the only known exceptions are
		 * 	a) if we were being called from gvcst_put for a duplicate SET
		 * 	b) if we were being called from gvcst_kill for a duplicate KILL
		 * 	c) if we were called from DSE MAPS
		 * 	d) if we were being called from gvcst_jrt_null.
		 * in case (a) and (b), we want to write logical SET or KILL journal records and replicate them.
		 * in case (c), we do not want to replicate them. we want to assert that is_replicator is FALSE in this case.
		 * the following assert achieves that purpose.
		 */
		assert((inctn_invalid_op != inctn_opcode) || cw_depth
				|| !is_replicator						/* exception case (c) */
				|| (ERR_GVPUTFAIL == t_err) && gvdupsetnoop			/* exception case (a) */
				|| (ERR_JRTNULLFAIL == t_err)					/* exception case (d) */
				|| (ERR_GVKILLFAIL == t_err) && gv_play_duplicate_kills);	/* exception case (b) */
		if (REPL_ALLOWED(csa) && (NULL != jnlpool_ctl))
		{
			repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
			if (!repl_csa->hold_onto_crit)
				grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
			assert(repl_csa->now_crit);
			jnlpool_crit_acquired = TRUE;
#			ifdef UNIX
			/* With jnlpool lock held, check instance freeze, and retry if set. */
			if (jnlpool.jnlpool_ctl->freeze)
			{
				status = cdb_sc_instancefreeze;
				goto failed;
			}
#			endif
			if (is_replicator && (inctn_invalid_op == inctn_opcode))
			{
				jpl = jnlpool_ctl;
				tjpl = temp_jnlpool_ctl;
				replication = TRUE;
				tjpl->write_addr = jpl->write_addr;
				tjpl->write = jpl->write;
				tjpl->jnl_seqno = jpl->jnl_seqno;
#				ifdef UNIX
				if (INVALID_SUPPL_STRM != strm_index)
				{	/* Need to also update supplementary stream seqno */
					supplementary = TRUE;
					assert(0 <= strm_index);
					/* assert(strm_index < ARRAYSIZE(tjpl->strm_seqno)); */
					strm_seqno = jpl->strm_seqno[strm_index];
					ASSERT_INST_FILE_HDR_HAS_HISTREC_FOR_STRM(strm_index);
				} else
					supplementary = FALSE;
#				endif
				INT8_ONLY(assert(tjpl->write == tjpl->write_addr % tjpl->jnlpool_size));
				assert(jgbl.cumul_jnl_rec_len);
				tmp_cumul_jnl_rec_len = (uint4)(jgbl.cumul_jnl_rec_len + SIZEOF(jnldata_hdr_struct));
				tjpl->write += SIZEOF(jnldata_hdr_struct);
				if (tjpl->write >= tjpl->jnlpool_size)
				{
					assert(tjpl->write == tjpl->jnlpool_size);
					tjpl->write = 0;
				}
				assert(jpl->early_write_addr == jpl->write_addr);
				jpl->early_write_addr = jpl->write_addr + tmp_cumul_jnl_rec_len;
				/* Source server does not read in crit. It relies on early_write_addr, the transaction
				 * data, lastwrite_len, write_addr being updated in that order. To ensure this order,
				 * we have to force out early_write_addr to its coherency point now. If not, the source
				 * server may read data that is overwritten (or stale). This is true only on
				 * architectures and OSes that allow unordered memory access
				 */
				SHM_WRITE_MEMORY_BARRIER;
			}
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
			if (replication)
			{	/* Make sure timestamp of this seqno is >= timestamp of previous seqno. Note: The below macro
				 * invocation should be done AFTER the ADJUST_GBL_JREC_TIME call as the below resets
				 * jpl->prev_jnlseqno_time. Doing it the other way around would mean the reset will happen
				 * with a potentially lower value than the final adjusted time written in the jnl record.
				 */
				ADJUST_GBL_JREC_TIME_JNLPOOL(jgbl, jpl);
			}
			/* Note that jnl_ensure_open can call cre_jnl_file which
			 * in turn assumes jgbl.gbl_jrec_time is set. Also jnl_file_extend can call
			 * jnl_write_epoch_rec which in turn assumes jgbl.gbl_jrec_time is set.
			 * In case of forw-phase-recovery, mur_output_record would have already set this.
			 */
			assert(jgbl.gbl_jrec_time);
			jnl_status = jnl_ensure_open();
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
				assert((csa->jnl_state == csd->jnl_state) && (csa->jnl_before_image == csd->jnl_before_image));
				if (DISK_BLOCKS_SUM(jbp->freeaddr, total_jnl_rec_size) > jbp->filesize)
				{	/* Moved as part of change to prevent journal records splitting
					 * across multiple generation journal files. */
					if (SS_NORMAL != (jnl_status = jnl_flush(jpc->region)))
					{
						send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFLUSH, 2, JNL_LEN_STR(csd),
							ERR_TEXT, 2, RTS_ERROR_TEXT("Error with journal flush during t_end"),
							jnl_status);
						assert((!JNL_ENABLED(csd)) && (JNL_ENABLED(csa)));
						status = cdb_sc_jnlclose;
						goto failed;
					} else if (EXIT_ERR == jnl_file_extend(jpc, total_jnl_rec_size))
					{
						assert(csd == csa->hdr);	/* jnl_file_extend() shouldn't reset csd in MM */
						assert((!JNL_ENABLED(csd)) && (JNL_ENABLED(csa)));
						status = cdb_sc_jnlclose;
						goto failed;
					}
					assert(csd == csa->hdr);	/* If MM, csd shouldn't have been reset */
				}
				assert(jgbl.gbl_jrec_time >= jbp->prev_jrec_time);
				if (0 == jpc->pini_addr)
					jnl_put_jrt_pini(csa);
				if (JNL_HAS_EPOCH(jbp))
				{
					if ((jbp->next_epoch_time <= jgbl.gbl_jrec_time) UNCONDITIONAL_EPOCH_ONLY(|| TRUE))
					{	/* Flush the cache. Since we are in crit, defer syncing epoch */
						if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_IN_COMMIT
												| WCSFLU_SPEEDUP_NOBEFORE))
						{
							SET_WCS_FLU_FAIL_STATUS(status, csd);
							SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_jnl_wcsflu);
							goto failed;
						}
						assert(csd == csa->hdr);
						VMS_ONLY(
							if (csd->clustered  &&
								!CCP_SEGMENT_STATE(cnl, CCST_MASK_HAVE_DIRTY_BUFFERS))
							{
								CCP_FID_MSG(gv_cur_region, CCTR_FLUSHLK);
								ccp_userwait(gv_cur_region,
									CCST_MASK_HAVE_DIRTY_BUFFERS, 0, cnl->ccp_cycle);
							}
						)
					}
				}
			} else
			{
				if (SS_NORMAL != jpc->status)
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) jnl_status, 4, JNL_LEN_STR(csd),
							DB_LEN_STR(gv_cur_region), jpc->status);
				else
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd),
							DB_LEN_STR(gv_cur_region));
			}
		}
		assert(!TREF(donot_commit));	/* We should never commit a transaction that was determined restartable */
		assert(TN_NOT_SPECIFIED > MAX_TN_V6); /* Ensure TN_NOT_SPECIFIED isn't a valid TN number */
		blktn = (TN_NOT_SPECIFIED == ctn) ? dbtn : ctn;
		csa->ti->early_tn = dbtn + 1;
		if (JNL_ENABLED(csa))
		{
			/* At this point we know tn,pini_addr and jrec_time; so calculate the checksum for the transaction once
			   reuse it for all the updates
			*/
			if(!com_csum)
			{
				ADJUST_CHECKSUM_TN(INIT_CHECKSUM_SEED, &dbtn, com_csum);
				ADJUST_CHECKSUM(com_csum, csa->jnl->pini_addr, com_csum);
				ADJUST_CHECKSUM(com_csum, jgbl.gbl_jrec_time, com_csum);
			}
			DEBUG_ONLY(save_gbl_jrec_time = jgbl.gbl_jrec_time;)
			if (jbp->before_images)
			{	/* do not write PBLKs if MUPIP REORG UPGRADE/DOWNGRADE with -NOSAFEJNL */
				if (!mu_reorg_upgrd_dwngrd_in_prog || !mu_reorg_nosafejnl)
				{
					epoch_tn = jbp->epoch_tn; /* store in a local as it is used in a loop below */
					for (cs = cw_set, cs_top = cs + cw_set_depth;  cs < cs_top;  ++cs)
					{
						/* PBLK computations for FREE blocks are not needed */
						if (WAS_FREE(cs->blk_prior_state))
							continue;
						/* write out before-update journal image records */
						mode = cs->mode;
						if (gds_t_committed < mode)
						{	/* There are three possibilities at this point.
							 * a) gds_t_write_root : In this case no need to write PBLK.
							 * b) gds_t_busy2free : This is set by gvcst_bmp_mark_free to indicate
							 *	that a block has to be freed right away instead of taking it
							 *	through the RECYCLED state. This should be done only if
							 *	csd->db_got_to_v5_once has not yet become TRUE. Once it is
							 *	TRUE, block frees will write PBLK only when the block is reused.
							 *      An exception is when the block is a level-0 block in directory
							 * 	tree, we always write PBLK immediately.
							 * c) gds_t_recycled2free: Need to write PBLK
							 */
							assert((gds_t_write_root == mode) || (gds_t_busy2free == mode)
								|| (gds_t_recycled2free == mode));
							if (!SAVE_2FREE_IMAGE(mode, free_seen, csd))
								continue;
						}
						old_block = (blk_hdr_ptr_t)cs->old_block;
						ASSERT_IS_WITHIN_SHM_BOUNDS((sm_uc_ptr_t)old_block, csa);
						DBG_ENSURE_OLD_BLOCK_IS_VALID(cs, is_mm, csa, csd);
						if ((NULL != old_block) && (old_block->tn < epoch_tn))
						{
							bsiz = old_block->bsiz;
							assert((bsiz <= csd->blk_size) || IS_DSE_IMAGE);
							assert(bsiz >= SIZEOF(blk_hdr) || IS_DSE_IMAGE);
							/* For acquired or gds_t_busy2free blocks, we should have computed
							 * checksum already. The only exception is if we found no need to
							 * compute checksum outside of crit but before we got crit, an
							 * EPOCH got written concurrently so we have to write a PBLK (and
							 * hence compute the checksum as well) when earlier we thought none
							 * was necessary. An easy way to check this is that an EPOCH was
							 * written AFTER we started this transaction.
							 */
							assert((gds_t_acquired != cs->mode) || (gds_t_busy2free != cs->mode)
								|| cs->blk_checksum || (epoch_tn >= start_tn));
							/* It is possible that the block has a bad block-size.
							 * Before computing checksum ensure bsiz passed is safe.
							 * The checks done here for "bsiz" assignment are
							 * similar to those done in jnl_write_pblk/jnl_write_aimg.
							 */
							if (IS_DSE_IMAGE)
								bsiz = MIN(bsiz, csd->blk_size);
							assert(!cs->blk_checksum ||
								(cs->blk_checksum == jnl_get_checksum((uint4 *)old_block,
													csa,
													bsiz)));
							if (!cs->blk_checksum)
								cs->blk_checksum = jnl_get_checksum((uint4 *)old_block,
													csa,
													bsiz);
#							ifdef GTM_CRYPT
							if (csd->is_encrypted)
							{
								DBG_ENSURE_PTR_IS_VALID_GLOBUFF(csa, csd, (sm_uc_ptr_t)old_block);
								DEBUG_ONLY(save_old_block = old_block;)
								old_block = (blk_hdr_ptr_t)GDS_ANY_ENCRYPTGLOBUF(old_block,
														      csa);
								/* Ensure that the unencrypted block and it's twin counterpart are
								 * in sync.
								 */
								assert(save_old_block->tn == old_block->tn);
								assert(save_old_block->bsiz == old_block->bsiz);
								assert(save_old_block->levl == old_block->levl);
								DBG_ENSURE_PTR_IS_VALID_ENCTWINGLOBUFF(csa,
												       csd,
												       (sm_uc_ptr_t)old_block);
							}
#							endif
							jnl_write_pblk(csa, cs, old_block, com_csum);
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
				cs = cw_set;
				jnl_write_aimg_rec(csa, cs, com_csum);
			} else if (write_inctn)
			{
				assert(!replication);
				if ((inctn_blkupgrd == inctn_opcode) || (inctn_blkdwngrd == inctn_opcode))
				{
					assert(1 == cw_set_depth); /* upgrade/downgrade one block at a time */
					cs = cw_set;
					assert(inctn_detail.blknum_struct.blknum == cs->blk);
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
				assert(!replication || !jgbl.forw_phase_recovery);
				if (replication)
				{
					jnl_fence_ctl.token = tjpl->jnl_seqno;
					UNIX_ONLY(
						if (supplementary)
							jnl_fence_ctl.strm_seqno = SET_STRM_INDEX(strm_seqno, strm_index);
					)
				} else if (!jgbl.forw_phase_recovery)
					jnl_fence_ctl.token = seq_num_zero;
				/* In case of forw-phase of recovery, token would have been set by mur_output_record */
				jnl_write_logical(csa, non_tp_jfb_ptr, com_csum);
			} else
				jnl_write_ztp_logical(csa, non_tp_jfb_ptr, com_csum);
			/* Ensure jgbl.gbl_jrec_time did not get reset by any of the jnl writing functions */
			assert(save_gbl_jrec_time == jgbl.gbl_jrec_time);
		} else if (replication)
		{	/* Case where JNL_ENABLED(csa) is FALSE but REPL_WAS_ENABLED(csa) is TRUE and therefore we need to
			 * write logical jnl records in the journal pool (no need to write in journal buffer or journal file).
			 */
			assert(!JNL_ENABLED(csa) && REPL_WAS_ENABLED(csa));
			if (0 == jnl_fence_ctl.level)
			{
				jnl_fence_ctl.token = tjpl->jnl_seqno;
				UNIX_ONLY(
					if (supplementary)
						jnl_fence_ctl.strm_seqno = SET_STRM_INDEX(strm_seqno, strm_index);
				)
				jnl_write_logical(csa, non_tp_jfb_ptr, com_csum);
			} else
				jnl_write_ztp_logical(csa, non_tp_jfb_ptr, com_csum);
		}
		if (free_seen)
		{	/* Write to snapshot and backup file for busy2free and recycled2free mode. These modes only appear in
			 * mupip reorg -truncate or v4-v5 upgrade, neither of which can occur with MM.
			 */
			assert(!is_mm);
			cs = &cw_set[0];
			if (SAVE_2FREE_IMAGE(cs->mode, free_seen, csd))
			{
				blkid = cs->blk;
				assert(!IS_BITMAP_BLK(blkid) && (blkid == cr_array[0]->blk));
				csa->backup_in_prog = (BACKUP_NOT_IN_PROGRESS != cnl->nbb);
				cr = cr_array[0];
				backup_cr = cr;
				blk_ptr = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
				backup_blk_ptr = blk_ptr;
				BG_BACKUP_BLOCK(csa, csd, cnl, cr, cs, blkid, backup_cr, backup_blk_ptr, block_saved,
						 dummysi->backup_block_saved);
#				ifdef GTM_SNAPSHOT
				if (SNAPSHOTS_IN_PROG(csa))
				{	/* we write the before-image to snapshot file only for FAST_INTEG and not for
					 * regular integ because the block is going to be marked free at this point
					 * and in case of a regular integ a before image will be written to the snapshot
					 * file eventually when the free block gets reused. So the before-image writing
					 * effectively gets deferred but does happen.
					 */
					lcl_ss_ctx = SS_CTX_CAST(cs_addrs->ss_ctx);
					if (lcl_ss_ctx && FASTINTEG_IN_PROG(lcl_ss_ctx))
						WRITE_SNAPSHOT_BLOCK(cs_addrs, cr, NULL, blkid, lcl_ss_ctx);
				}
#				endif
			}
		}
		if (replication)
		{
			tjpl->jnl_seqno++;
			assert(csa->hdr->reg_seqno < tjpl->jnl_seqno);
			csa->hdr->reg_seqno = tjpl->jnl_seqno;
			UNIX_ONLY(
				if (supplementary)
				{
					next_strm_seqno = strm_seqno + 1;
					/* tjpl->strm_seqno[strm_index] = next_strm_seqno; */
					csa->hdr->strm_reg_seqno[strm_index] = next_strm_seqno;
				}
			)
			VMS_ONLY(
				if (is_updproc)
				{
					jgbl.max_resync_seqno++;
					csa->hdr->resync_seqno = jgbl.max_resync_seqno;
				}
			)
		}
		csa->prev_free_blks = csa->ti->free_blocks;
		csa->t_commit_crit = T_COMMIT_CRIT_PHASE1;
		if (cw_set_depth)
		{
			if (!is_mm)	/* increment counter of # of processes that are actively doing two-phase commit */
				INCR_WCS_PHASE2_COMMIT_PIDCNT(csa, cnl);
#			ifdef DEBUG
			/* Assert that cs->old_mode if uninitialized, never contains a negative value (relied by secshr_db_clnup) */
			for (cs = cw_set, cs_top = cs + cw_set_depth;  cs < cs_top;  ++cs)
				assert(0 <= cs->old_mode);
#			endif
			for (cs = cw_set, cs_top = cs + cw_set_depth;  cs < cs_top;  ++cs)
			{
				mode = cs->mode;
				assert((gds_t_write_root != mode) || ((cs - cw_set) + 1 == cw_depth));
				assert((gds_t_committed > mode) ||
					(gds_t_busy2free == mode) || (gds_t_recycled2free == mode) || (gds_t_write_root == mode));
				cs->old_mode = (int4)mode;	/* note down before being reset to gds_t_committed */
				if (gds_t_committed > mode)
				{
					DEBUG_ONLY(
						/* Check bitmap status of block we are about to modify.
						 * Two exceptions are
						 *	a) DSE which can modify bitmaps at will.
						 *	b) MUPIP RECOVER writing an AIMG. In this case it is playing
						 *		forward a DSE action so is effectively like DSE doing it.
						 */
						if (!IS_DSE_IMAGE && !write_after_image)
							bml_status_check(cs);
					)
					if (is_mm)
						status = mm_update(cs, dbtn, blktn, dummysi);
					else
					{
						if (csd->dsid)
						{
							if (ERR_GVKILLFAIL == t_err)
							{
								if (cs == cw_set)
								{
									if ((gds_t_acquired == mode) ||
									    ((cw_set_depth > 1) && (0 == cw_set[1].level)))
										rc_cpt_inval();
									else
										rc_cpt_entry(cs->blk);
								}
							} else	if (0 == cs->level)
								rc_cpt_entry(cs->blk);
						}
						/* Do phase1 of bg_update while holding crit on the database.
						 * This will lock the buffers that need to be changed.
						 * Once crit is released, invoke phase2 which will update those locked buffers.
						 * The exception is if it is a bitmap block. In that case we also do phase2
						 * while holding crit so the next process to use this bitmap will see a
						 * consistent copy of this bitmap when it gets crit for commit. This avoids
						 * the reallocate_bitmap routine from restarting or having to wait for a
						 * concurrent phase2 construction to finish. When the change request C9E11-002651
						 * (to reduce restarts due to bitmap collisions) is addressed, we can reexamine
						 * whether it makes sense to move bitmap block builds back to phase2.
						 */
						status = bg_update_phase1(cs, dbtn, dummysi);
						if ((cdb_sc_normal == status) && (gds_t_writemap == mode))
						{
							status = bg_update_phase2(cs, dbtn, blktn, dummysi);
							if (cdb_sc_normal == status)
								cs->mode = gds_t_committed;
						}
					}
					if (cdb_sc_normal != status)
					{	/* the database is probably in trouble */
						INVOKE_T_COMMIT_CLEANUP(status, csa);
						assert(cdb_sc_normal == status);
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
			}
		}
		/* signal secshr_db_clnup/t_commit_cleanup, roll-back is no longer possible */
		update_trans |= UPDTRNS_TCOMMIT_STARTED_MASK;
		assert(cdb_sc_normal == status);
		/* should never increment curr_tn on a frozen database except if DSE */
		assert(!(csd->freeze UNIX_ONLY(|| (replication && jnlpool.jnlpool_ctl->freeze))) || IS_DSE_IMAGE);
		INCREMENT_CURR_TN(csd);
		csa->t_commit_crit = T_COMMIT_CRIT_PHASE2;	/* set this BEFORE releasing crit but AFTER incrementing curr_tn */
		/* If db is journaled, then db header is flushed periodically when writing the EPOCH record,
		 * otherwise do it here every HEADER_UPDATE_COUNT transactions.
		 */
		assert(!JNL_ENABLED(csa) || (jbp == csa->jnl->jnl_buff));
		if ((!JNL_ENABLED(csa) || !JNL_HAS_EPOCH(jbp)) && !(csd->trans_hist.curr_tn & (HEADER_UPDATE_COUNT - 1)))
			fileheader_sync(gv_cur_region);
		UNIX_ONLY(assert((MUSWP_INCR_ROOT_CYCLE != TREF(in_mu_swap_root_state)) || need_kip_incr));
		if (need_kip_incr)		/* increment kill_in_prog */
		{
			INCR_KIP(csd, csa, kip_csa);
			need_kip_incr = FALSE;
#			ifdef UNIX
			if (MUSWP_INCR_ROOT_CYCLE == TREF(in_mu_swap_root_state))
			{	/* Increment root_search_cycle to let other processes know that they should redo_root_search. */
				assert((0 != cw_map_depth) && !TREF(in_gvcst_redo_root_search));
				csa->nl->root_search_cycle++;
			}
#			endif
		}
		start_tn = dbtn; /* start_tn temporarily used to store currtn (for bg_update_phase2) before releasing crit */
		if (free_seen)
		{	/* need to do below BEFORE releasing crit as we have no other lock on this buffer */
			VMS_ONLY(assert((BUSY2FREE == free_seen) && (2 <= cr_array_index) && (cr_array_index <= 3)));
			UNIX_ONLY(assert(2 == cr_array_index));	/* Unlike VMS, no chance to pin a twin for bitmap update */
			assert((2 == cw_set_depth) && (process_id == cr_array[0]->in_cw_set));
			UNPIN_CACHE_RECORD(cr_array[0]);
		}
	}
	if (!csa->hold_onto_crit)
		rel_crit(gv_cur_region);
	assert(!replication || update_trans);
	if (replication)
	{
		assert(jpl->early_write_addr > jpl->write_addr);
		assert(tmp_cumul_jnl_rec_len == (tjpl->write - jpl->write + (tjpl->write > jpl->write ? 0 : jpl->jnlpool_size)));
		/* the following statements should be atomic */
		jnl_header = (jnldata_hdr_ptr_t)(jnlpool.jnldata_base + jpl->write);
		jnl_header->jnldata_len = tmp_cumul_jnl_rec_len;
		jnl_header->prev_jnldata_len = jpl->lastwrite_len;
		/* The following assert should be an == rather than a >= (as in tp_tend) because, we have
		 * either one or no update.  If no update, we would have no cw_depth and we wouldn't enter
		 * this path.  If there is an update, then both the indices should be 1.
		 */
		INT8_ONLY(assert(jgbl.cumul_index == jgbl.cu_jnl_index));
		UNIX_ONLY(
			if (supplementary)
				jpl->strm_seqno[strm_index] = next_strm_seqno;
		)
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
		/* jpl->write_addr should be updated before updating jpl->jnl_seqno as secshr_db_clnup relies on this */
		jpl->write_addr += jnl_header->jnldata_len;
		assert(jpl->early_write_addr == jpl->write_addr);
		jpl->jnl_seqno = tjpl->jnl_seqno;
	}
	if (jnlpool_crit_acquired)
	{
		assert((NULL != jnlpool_ctl) && repl_csa->now_crit && REPL_ALLOWED(csa));
		rel_lock(jnlpool.jnlpool_dummy_reg);
	}
	/* If BG, check that we have not pinned any more buffers than we are updating */
	DBG_CHECK_PINNED_CR_ARRAY_CONTENTS(is_mm, cr_array, cr_array_index, csd->bplmap);
	if (cw_set_depth)
	{	/* Finish 2nd phase of commit for BG (updating the buffers in phase1) now that CRIT has been released.
		 * For MM, only thing needed is to set cs->mode to gds_t_committed.
		 */
		for (cs = cw_set, cs_top = cs + cw_set_depth;  cs < cs_top;  ++cs)
		{
			mode = cs->mode;
			assert((gds_t_write_root != mode) || ((cs - cw_set) + 1 == cw_depth));
			assert((kill_t_write != mode) && (kill_t_create != mode));
			if (gds_t_committed > mode)
			{
				if (!is_mm)
				{	/* Validate old_mode noted down in first phase is the same as the current mode.
					 * Note that cs->old_mode is negated by bg_update_phase1 (to help secshr_db_clnup).
					 */
					assert(-cs->old_mode == mode);
					status = bg_update_phase2(cs, dbtn, blktn, dummysi);
					if (cdb_sc_normal != status)
					{	/* the database is probably in trouble */
						INVOKE_T_COMMIT_CLEANUP(status, csa);
						assert(cdb_sc_normal == status);
						/* At this time "cr_array_index" could be non-zero and a few cache-records might
						 * have their "in_cw_set" field set to TRUE. We should not reset "in_cw_set" as we
						 * don't hold crit at this point and also because we might still need those buffers
						 * pinned until their before-images are backed up in wcs_recover (in case an
						 * online backup was running while secshr_db_clnup did its job). Reset the
						 * local variable "cr_array_index" though so we do not accidentally reset the
						 * "in_cw_set" fields ourselves before the wcs_recover.
						 */
						cr_array_index = 0;
						/* Note that seshr_db_clnup (invoked by t_commit_cleanup above) would have
						 * done a lot of cleanup for us including decrementing the wcs_phase2_commit_pidcnt
						 * so it is ok to skip all that processing below and go directly to skip_cr_array.
						 */
						goto skip_cr_array;	/* hence skip until past "cr_array_index" processing */
					}
				}
			}
			cs->mode = gds_t_committed;
		}
		if (!is_mm)	/* now that two-phase commit is done, decrement counter */
			DECR_WCS_PHASE2_COMMIT_PIDCNT(csa, cnl);
	}
	UNPIN_CR_ARRAY_ON_COMMIT(cr_array, cr_array_index);
	assert(!cr_array_index);
	csa->t_commit_crit = FALSE;
	/* Phase 2 commits are completed. See if we had done a snapshot init (csa->snapshot_in_prog == TRUE). If so,
	 * try releasing the resources obtained while snapshot init.
	 */
	if (SNAPSHOTS_IN_PROG(csa))
	{
		assert(update_trans);
		SS_RELEASE_IF_NEEDED(csa, cnl);
	}

skip_cr_array:
	assert(!csa->now_crit || csa->hold_onto_crit);
	assert(cdb_sc_normal == status);
	REVERT;	/* no need for t_ch to be invoked if any errors occur after this point */
	DEFERRED_EXIT_HANDLING_CHECK; /* now that all crits are released, check if deferred signal/exit handling needs to be done */
	if (block_saved)
		backup_buffer_flush(gv_cur_region);
	if (unhandled_stale_timer_pop)
		process_deferred_stale();
	if (update_trans)
	{
		wcs_timer_start(gv_cur_region, TRUE);
		if (REPL_ALLOWED(csa) && IS_DSE_IMAGE)
		{
			temp_tn = dbtn + 1;
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_NOTREPLICATED, 4, &temp_tn, LEN_AND_LIT("DSE"), process_id);
		}
		INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_readwrite, 1);
		INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_blkread, n_blks_validated);
		INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_blkwrite, cw_set_depth);
		GVSTATS_SET_CSA_STATISTIC(csa, db_curr_tn, dbtn);
	} else
	{
		INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_readonly, 1);
		INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_blkread, n_blks_validated);
	}
	/* "secshr_db_clnup/t_commit_cleanup" assume an active non-TP transaction if cw_set_depth is non-zero
	 * or if update_trans is set to T_COMMIT_STARTED. Now that the transaction is complete, reset these fields.
	 */
	DEBUG_ONLY(jgbl.cumul_index = jgbl.cu_jnl_index = 0;) 	/* symmetrical with TP and makes op_tstart checks happy */
	cw_set_depth = 0;
	update_trans = 0;
	CWS_RESET;
	/* although we have the same assert at the beginning of "skip_cr_array" label, the below assert ensures that we did not grab
	 * crit in between (in process_deferred_stale() or backup_buffer_flush() or wcs_timer_start()). The assert at the beginning
	 * of the loop is needed as well to ensure that if we are done with the commit, we should have released crit
	 */
	assert(!csa->now_crit || csa->hold_onto_crit);
	t_tries = 0;	/* commit was successful so reset t_tries */
	assert(0 == cr_array_index);
	return dbtn;
failed:
	assert(cdb_sc_normal != status);
	REVERT;
failed_skip_revert:
	RESTORE_CURRTN_IF_NEEDED(csa, write_inctn, decremented_currtn);
	retvalue = t_commit_cleanup(status, 0);	/* we expect to get a return value indicating update was NOT underway */
	assert(!retvalue); 			/* if it was, then we would have done a "goto skip_cr_array:" instead */
	if ((NULL != hist1) && (NULL != (gvnh = hist1->h[0].blk_num ? hist1->h[0].blk_target : NULL)))
		gvnh->clue.end = 0;
	if ((NULL != hist2) && (NULL != (gvnh = hist2->h[0].blk_num ? hist2->h[0].blk_target : NULL)))
		gvnh->clue.end = 0;
#	ifdef DEBUG
	/* Ensure we dont have t1->cse set for any gv_targets that also have their clue non-zero.
	 * As this can cause following transactions to rely on out-of-date information and do wrong things.
	 * (e.g. in t_end of the following transaction, we will see t1->cse non-NULL and conclude the buffer
	 * needs to be pinned when actually it is not necessary).
	 */
	for (gvnh = gv_target_list; NULL != gvnh; gvnh = gvnh->next_gvnh)
	{
		if (gvnh->clue.end)
		{
			for (t1 = &gvnh->hist.h[0]; t1->blk_num; t1++)
				assert(NULL == t1->cse);
		}
	}
#	endif
	/* t_commit_cleanup releases crit as long as the transition is from 2nd to 3rd retry or 3rd to 3rd retry. The only exception
	 * is if hold_onto_crit is set to TRUE in which case t_commit_cleanup honors it. Assert accordingly.
	 */
	assert(!csa->now_crit || !NEED_TO_RELEASE_CRIT(t_tries, status) || csa->hold_onto_crit);
	DEFERRED_EXIT_HANDLING_CHECK; /* now that all crits are released, check if deferred signal/exit handling needs to be done */
	t_retry(status);
	/* in the retry case, we do not do a CWS_RESET as cw_stagnate is used only in the
	 * final retry in which case t_end will succeed and do a CWS_RESET
	 */
	cw_map_depth = 0;
	assert(0 == cr_array_index);
	return 0;
}
