/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_time.h"
#include "gtm_inet.h"
#include "gtm_signal.h"	/* for VSIG_ATOMIC_T type */

#include <stddef.h>

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

#include "gtmrecv.h"
#include "deferred_signal_handler.h"
#include "repl_instance.h"
#include "format_targ_key.h"

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
#include "db_snapshot.h"
#include "shmpool.h"
#include "bml_status_check.h"
#include "is_proc_alive.h"
#include "muextr.h"

GBLREF	bool			rc_locked;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	cache_rec_ptr_t		cr_array[]; /* Maximum number of blocks that can be in transaction */
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
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
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
GBLREF	trans_num		mu_reorg_upgrd_dwngrd_blktn;	/* tn in blkhdr of current block processed by
								 * REORG UPGRADE/DOWNGRADE */
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	inctn_detail_t		inctn_detail;			/* holds detail to fill in to inctn jnl record */
GBLREF	boolean_t		block_is_free;
GBLREF	boolean_t		gv_play_duplicate_kills;
GBLREF	int			pool_init;
GBLREF	gv_key			*gv_currkey;
GBLREF	recvpool_addrs		recvpool;
GBLREF	int4			strm_index;
GBLREF	uint4			mu_reorg_encrypt_in_prog;	/* non-zero if MUPIP REORG ENCRYPT is in progress */
GBLREF	sgmnt_addrs		*reorg_encrypt_restart_csa;
GBLREF	trans_num		local_tn;	/* transaction number for THIS PROCESS */
#ifdef GTM_TRIGGER
GBLREF	boolean_t		skip_dbtriggers;	/* see gbldefs.c for description of this global */
#endif
#ifdef DEBUG
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	uint4			bml_save_dollar_tlevel;
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

#define	RESTORE_CURRTN_IF_NEEDED(csa, cti, write_inctn, decremented_currtn)				\
{													\
	if (write_inctn && decremented_currtn)								\
	{	/* decremented curr_tn above; need to restore to original state due to the restart */	\
		assert(csa->now_crit);									\
		if (csa->now_crit)									\
		{	/* need crit to update curr_tn and early_tn */					\
			cti->curr_tn++;									\
			cti->early_tn++;								\
		}											\
		decremented_currtn = FALSE;								\
	}												\
}

#define EACH_HIST(HIST, HIST1, HIST2)	(HIST = HIST1;  (NULL != HIST);  HIST = (HIST == HIST1) ? HIST2 : NULL)

#define	BUSY2FREE	0x00000001
#define	RECYCLED2FREE	0x00000002
#define	FREE_DIR_DATA	0x00000004	/* denotes the block to be freed is a data block in directory tree */

#define SAVE_2FREE_IMAGE(MODE, FREE_SEEN, CSD)								\
	 (((gds_t_busy2free == MODE) && (!CSD->db_got_to_v5_once || (FREE_SEEN & FREE_DIR_DATA)))	\
	|| (gds_t_recycled2free == MODE))

trans_num t_end(srch_hist *hist1, srch_hist *hist2, trans_num ctn)
{
	srch_hist		*hist, tmp_hist;
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
	DEBUG_ONLY(sgmnt_addrs	*jnlpool_csa = NULL;)
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	sgm_info		*dummysi = NULL;	/* needed as a dummy parameter for {mm,bg}_update */
	srch_blk_status		*t1;
	trans_num		valid_thru, oldest_hist_tn, dbtn, blktn, temp_tn, epoch_tn, old_block_tn;
	unsigned char		cw_depth, cw_bmp_depth, buff[MAX_ZWR_KEY_SZ], *end;
	jnldata_hdr_ptr_t	jnl_header;
	uint4			total_jnl_rec_size, tmp_cw_set_depth, prev_cw_set_depth;
	DEBUG_ONLY(unsigned int	tot_jrec_size;)
	jnlpool_ctl_ptr_t	jpl;
	jnlpool_addrs_ptr_t	save_jnlpool, tmp_jnlpool;
	boolean_t		replication = FALSE;
	boolean_t		supplementary = FALSE;	/* this variable is initialized ONLY if "replication" is TRUE. */
	seq_num			strm_seqno, next_strm_seqno;
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
	seq_num			temp_jnl_seqno;
#	ifdef DEBUG
	boolean_t		ready2signal_gvundef_lcl;
	enum cdb_sc		prev_status;
#	endif
	int			n_blks_validated;
	boolean_t		before_image_needed, lcl_ss_in_prog = FALSE, reorg_ss_in_prog = FALSE;
	boolean_t		ss_need_to_restart, new_bkup_started;
	boolean_t		same_db_state;
	gv_namehead		*gvnh;
	gd_region		*reg;
#	ifdef GTM_TRIGGER
	uint4			cycle;
#	endif
	snapshot_context_ptr_t  lcl_ss_ctx;
	th_index_ptr_t     	cti;
	jbuf_rsrv_struct_t	*jrs;
	jrec_rsrv_elem_t	*first_jre, *jre, *jre_top;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Currently, the only callers of t_end with NULL histories are the update process and journal recovery when they
	 * are about to process a JRT_NULL record. Assert that.
	 */
	assert((hist1 != hist2) || (ERR_JRTNULLFAIL == t_err) && (NULL == hist1)
					&& update_trans && (is_updproc || jgbl.forw_phase_recovery));
#	ifdef DEBUG
	/* Store global variable ready2signal_gvundef in a local variable and reset the global right away to ensure that
	 * the global value does not incorrectly get carried over to the next call of "t_end".
	 */
	ready2signal_gvundef_lcl = TREF(ready2signal_gvundef);
	TREF(ready2signal_gvundef) = FALSE;
#	endif
	reg = gv_cur_region;
	csa = cs_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	is_mm = (dba_mm == csd->acc_meth);
	save_jnlpool = jnlpool;
	if (csa->jnlpool && (jnlpool != csa->jnlpool))
	{
		DEBUG_ONLY(jnlpool_csa = csa);
		jnlpool = csa->jnlpool;
	}
	tmp_jnlpool = jnlpool;
	DEBUG_ONLY(in_mu_truncate = (cnl != NULL && process_id == cnl->trunc_pid);)
	TREF(rlbk_during_redo_root) = FALSE;
	status = cdb_sc_normal;
	/* The only cases where we set csa->hold_onto_crit to TRUE are the following :
	 * (a) jgbl.onlnrlbk
	 * (b) DSE CRIT -SEIZE (and any command that follows it), DSE CHANGE -BLOCK, DSE ALL -SEIZE (and any command that follows)
	 *	and DSE MAPS -RESTORE_ALL. Since we cannot distinguish between different DSE qualifiers, we use IS_DSE_IMAGE.
	 * (c) gvcst_redo_root_search in the final retry.
	 * (d) MUPIP TRIGGER -UPGRADE which is a TP transaction but it could do non-TP as part of gvcst_bmp_mark_free at the end.
	 *
	 * Since we don't expect hold_onto_crit to be set by any other utility/function, the below assert is valid and is intended
	 * to catch cases where the field is inadvertently set to TRUE.
	 */
	assert(!csa->hold_onto_crit || IS_DSE_IMAGE
		|| jgbl.onlnrlbk || TREF(in_gvcst_redo_root_search) || TREF(in_trigger_upgrade));
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
	assert(!reg->read_only || !update_trans);
	cr_array_index = 0;	/* be safe and reset it in PRO even if it is not zero */
	/* If inctn_opcode has a valid value, then we better be doing an update. The only exception to this rule is if we are
	 * in MUPIP REORG UPGRADE/DOWNGRADE/ENCRYPT (mu_reorg_upgrd_dwngrd.c or mupip_reorg_encrypt.c), where update_trans is
	 * explicitly set to 0 in some cases.
	 */
	assert((inctn_invalid_op == inctn_opcode) || mu_reorg_upgrd_dwngrd_in_prog || mu_reorg_encrypt_in_prog || update_trans);
	assert(!need_kip_incr || update_trans || TREF(in_gvcst_redo_root_search));
	cti = csa->ti;
	if (cnl->wc_blocked || (is_mm && (csa->total_blks != cti->total_blks)))
	{	/* If blocked, or we have MM and file has been extended, force repair */
		status = cdb_sc_helpedout;	/* force retry with special status so philanthropy isn't punished */
		assert((CDB_STAGNATE > t_tries) || !is_mm || (csa->total_blks == cti->total_blks));
		goto failed_skip_revert;
	}
	if (!update_trans)
	{	/* Take a fast path for read transactions. We do not need crit; we simply need to verify our blocks have not been
		 * modified since we read them. The read is valid if neither the cycles nor the tns have changed. Otherwise restart.
		 * Note: if, as with updates, we validated via bt_get/db_csh_get, we would restart under the same conditions.
		 */
		SHM_READ_MEMORY_BARRIER;
		same_db_state = (start_tn == cti->early_tn);
		n_blks_validated = 0;
		if (cw_map_depth)
		{	/* Bit maps from "mupip_reorg_encrypt" or "mu_reorg_upgrd_dwngrd" that need history validation.
			 * There would be just ONE cse entry and "hist2" is guaranteed to be NULL. Assert that.
			 * Morph the one cse into a history that hist2 points to so we can use the below EACH_HIST for loop.
			 */
			assert(mu_reorg_upgrd_dwngrd_in_prog || mu_reorg_encrypt_in_prog);
			assert(1 == cw_map_depth);
			assert(0 == cw_set_depth);
			assert(NULL == hist2);
			hist2 = &tmp_hist;
			tmp_hist.h[1].blk_num = 0;			/* needed to indicate only one block in history array */
			tmp_hist.h[0].cr = cw_set[0].cr;		/* used by TP_IS_CDB_SC_BLKMOD and cycle check below */
			tmp_hist.h[0].buffaddr = cw_set[0].old_block;	/* used by TP_IS_CDB_SC_BLKMOD below */
			assert(IS_BITMAP_BLK(cw_set[0].blk));
			/* The for loop terminator for history validation below expects a non-zero block number
			 * (it is coded to handle non-bitmap blocks) whereas we might pass the bitmap block 0 here
			 * so add 1 to the block number so it descends down through the history validation but remember
			 * to undo the +1 before using the NONTP_TRACE_HIST_MOD macro which expects the correct block number.
			 */
			tmp_hist.h[0].blk_num = cw_set[0].blk + 1;	/* used by for terminator and NONTP_TRACE_HIST_MOD below */
			tmp_hist.h[0].tn = cw_set[0].tn;		/* used by TP_IS_CDB_SC_BLKMOD below */
			tmp_hist.h[0].cycle = cw_set[0].cycle;		/* used by cycle check below */
			assert(LCL_MAP_LEVL == cw_set[0].level);
			tmp_hist.h[0].level = cw_set[0].level;		/* used by NONTP_TRACE_HIST_MOD below */
			tmp_hist.h[0].blk_target = NULL;		/* used after "failed_skip_revert" in case of restart */
			/* "prev_rec", "curr_rec", "cse" and "first_tp_srch_status" are unused by below validation
			 * or "failed_skip_revert" code so no need to initialize them.
			 */
		}
		for EACH_HIST(hist, hist1, hist2)
		{
			for (t1 = hist->h;  t1->blk_num;  t1++)
			{	/* Validate block tn */
				if (!same_db_state && TP_IS_CDB_SC_BLKMOD(t1->cr, t1))
				{	/* block has been modified */
					status = cdb_sc_blkmod;
					if (cw_map_depth && (hist == hist2))
						t1->blk_num--;	/* Undo the + 1 done above (to ensure the for loop is executed) */
					NONTP_TRACE_HIST_MOD(t1, t_blkmod_t_end1);
					goto failed_skip_revert;
				}
				/* Validate buffer cycle */
				if (!is_mm && (t1->cr->cycle != t1->cycle))
				{	/* cache slot has been stolen */
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_cyclefail;
					goto failed_skip_revert;
				}
				n_blks_validated++; /* update n_blks_validated */
			}
		}
		assert(cdb_sc_normal == status);
		if ((0 != csa->nl->onln_rlbk_pid) && (process_id != csa->nl->onln_rlbk_pid))
		{	/* We don't want read transactions to succeed if a concurrent online rollback is running. Following
			 * grab_crit, we know none is running because grab_crit either waits for online rollback to complete, or
			 * salvages crit from a KILL -9'd online rollback. In the former case, we restart because of
			 * MISMATCH_ONLN_RLBK_CYCLES. In the latter case, grab_crit does the necessary recovery and issues
			 * DBFLCORRP if it notices that csd->file_corrupt is TRUE. Otherwise online rollback did not take the
			 * database to a state back in time, and we can complete the read.
			 */
			assert(!csa->now_crit);
			DEBUG_ONLY(tmp_jnlpool = jnlpool;)
			grab_crit(reg);
			rel_crit(reg);
			assert(tmp_jnlpool == jnlpool);
		}
		if (MISMATCH_ROOT_CYCLES(csa, cnl))
		{	/* If a root block has moved, we might have started the read from the wrong root block, in which
			 * case we cannot trust the entire search. Need to redo root search.
			 * If an online rollback concurrently finished, we will come into this "if" block and restart.
			 */
			DEBUG_ONLY(tmp_jnlpool = jnlpool;)
			was_crit = csa->now_crit;
			if (!was_crit)
				grab_crit(reg);
			status = cdb_sc_gvtrootmod2;
			if (MISMATCH_ONLN_RLBK_CYCLES(csa, cnl))
			{
				assert(!mupip_jnl_recover);
				assert(!IS_STATSDB_CSA(csa));
				status = ONLN_RLBK_STATUS(csa, cnl);
				SYNC_ONLN_RLBK_CYCLES;
				SYNC_ROOT_CYCLES(NULL);
			} else
				SYNC_ROOT_CYCLES(csa);
			if (!was_crit)
				rel_crit(reg);
			assert(tmp_jnlpool == jnlpool);
			goto failed_skip_revert;
		}
		if (start_tn <= cnl->last_wcs_recover_tn)
		{
			status = cdb_sc_wcs_recover;
			assert(CDB_STAGNATE > t_tries);
			goto failed_skip_revert;
		}
		/* Assert that if gtm_gvundef_fatal is non-zero, then we better not be about to signal a GVUNDEF */
		assert(!TREF(gtm_gvundef_fatal) || !ready2signal_gvundef_lcl);
		assert(!TREF(donot_commit));    /* We should never commit a transaction that was determined restartable */
		DEBUG_ONLY(tmp_jnlpool = jnlpool;)
		if (csa->now_crit && !csa->hold_onto_crit)
			rel_crit(reg);
		if (unhandled_stale_timer_pop)
			process_deferred_stale();
		assert(tmp_jnlpool == jnlpool);
		CWS_RESET;
		assert(!csa->now_crit || csa->hold_onto_crit); /* shouldn't hold crit unless asked to */
		t_tries = 0;	/* commit was successful so reset t_tries */
		INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_readonly, 1);
		INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_blkread, n_blks_validated);
		assert(tmp_jnlpool == jnlpool);
		if (save_jnlpool != jnlpool)
		{
			assert(!jnlpool_csa || (jnlpool_csa == csa));
			jnlpool = save_jnlpool;
		}
		return cti->curr_tn;
	}
	assert(update_trans);
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
	if (SNAPSHOTS_IN_PROG(cnl))
	{
		/* If snapshot context is not already created, then create one now to be used by this transaction. If context
		 * creation failed (for instance, on snapshot file open fail), then SS_INIT_IF_NEEDED sets csa->snapshot_in_prog
		 * to FALSE.
		 */
		SS_INIT_IF_NEEDED(csa, cnl);
	} else
		CLEAR_SNAPSHOTS_IN_PROG(csa);
	if (0 != cw_depth)
	{	/* Caution : since csa->backup_in_prog and read_before_image are initialized below
	 	 * only if (cw_depth), these variables should be used below only within an if (cw_depth).
		 */
		DEBUG_ONLY(tmp_jnlpool = jnlpool;)
		lcl_ss_in_prog = SNAPSHOTS_IN_PROG(csa); /* store in local variable to avoid pointer access */
		reorg_ss_in_prog = (mu_reorg_process && lcl_ss_in_prog); /* store in local variable if both snapshots and MUPIP
									  * REORG are in progress */
		assert(SIZEOF(bsiz) == SIZEOF(old_block->bsiz));
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
						if (is_mm && (cdb_sc_gbloflow == status))
						{
							assert(NULL != gv_currkey);
							if (NULL == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
								end = &buff[MAX_ZWR_KEY_SZ - 1];
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_GBLOFLOW, 0,
									ERR_GVIS, 2, end - buff, buff);
							if (save_jnlpool != jnlpool)
							{
								assert(!jnlpool_csa || (jnlpool_csa == csa));
								jnlpool = save_jnlpool;
							}
							rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_GBLOFLOW, 0,
									ERR_GVIS, 2, end - buff, buff);
						}
					}
					goto failed_skip_revert;
				}
				assert((CDB_STAGNATE > t_tries) || (cs->blk < cti->total_blks));
				blk_used ? BIT_SET_RECYCLED_AND_CLEAR_FREE(cs->blk_prior_state)
					 : BIT_CLEAR_RECYCLED_AND_SET_FREE(cs->blk_prior_state);
				BEFORE_IMAGE_NEEDED(read_before_image, cs, csa, csd, cs->blk, before_image_needed);
				if (!before_image_needed)
					cs->old_block = NULL;
				else
				{
					block_is_free = WAS_FREE(cs->blk_prior_state);
					DEBUG_ONLY(tmp_jnlpool = jnlpool;)
					cs->old_block = t_qread(cs->blk, (sm_int_ptr_t)&cs->cycle, &cs->cr);
					assert(tmp_jnlpool == jnlpool);
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
				 * a cs->blk that is >= cti->total_blks (i.e. if the bitmap buffer gets recycled).
				 * adjust assert accordingly.
				 * note that checking for crit is equivalent to checking if we are in the final retry.
				 */
				assert((CDB_STAGNATE > t_tries) || (cs->blk < cti->total_blks));
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
					DEBUG_ONLY(tmp_jnlpool = jnlpool;)
					cs->old_block = t_qread(cs->blk, (sm_int_ptr_t)&cs->cycle, &cs->cr);
					assert(tmp_jnlpool == jnlpool);
					if (NULL == cs->old_block)
					{
						status = (enum cdb_sc)rdfail_detail;
						goto failed_skip_revert;
					}
					ASSERT_IS_WITHIN_SHM_BOUNDS((sm_uc_ptr_t)cs->old_block, csa);
				}
			}
		}
		assert(tmp_jnlpool == jnlpool);
	}
	if (JNL_ENABLED(csa))
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
		{
			SET_GBL_JREC_TIME;	/* initializes jgbl.gbl_jrec_time */
			if (WBTEST_ENABLED(WBTEST_TEND_GBLJRECTIME_SLEEP))
				LONG_SLEEP(2);	/* needed by white-box test case v62002/gtm8332 */
		}
		assert(jgbl.gbl_jrec_time);
	}
	block_saved = FALSE;
	ESTABLISH_NOUNWIND(t_ch);	/* avoid hefty setjmp call, which is ok since we never unwind t_ch */
	assert(!csa->hold_onto_crit || csa->now_crit);
	if (!csa->now_crit)
	{
		/* Get more space if needed. This is done outside crit so that any necessary IO has a chance of occurring
		 * outside crit. The available space must be double-checked inside crit.
		 */
		DEBUG_ONLY(tmp_jnlpool = jnlpool;)
		if (!is_mm && !WCS_GET_SPACE(reg, cw_set_depth + 1, NULL))
			assert(FALSE);	/* wcs_get_space should have returned TRUE unconditionally in this case */
		assert(tmp_jnlpool == jnlpool);
		for (;;)
		{
			DEBUG_ONLY(tmp_jnlpool = jnlpool;)
			grab_crit(reg);	/* Step CMT01 (see secshr_db_clnup.c for CMTxx step descriptions) */
			if (!FROZEN_HARD(csa))
			{
				assert(tmp_jnlpool == jnlpool);
				break;
			}
			rel_crit(reg);
			assert(tmp_jnlpool == jnlpool);
			/* We are about to wait for freeze. Assert that we are not in phase2 of a bitmap free operation
			 * (part of an M-kill or REORG operation). Most freeze operations (e.g. MUPIP FREEZE) wait for the
			 * phase2 to complete. Some (e.g. MUPIP EXTRACT -FREEZE) don't. The cnl->freezer_waited_for_kip flag
			 * indicates which type of freeze it is. Assert based on that.
			 */
			assert(!cnl->freezer_waited_for_kip
				|| (inctn_bmp_mark_free_gtm != inctn_opcode) && (inctn_bmp_mark_free_mu_reorg != inctn_opcode));
			while (FROZEN_HARD(csa))
				hiber_start(1000);
			assert(tmp_jnlpool == jnlpool);
		}
	} else
	{	/* We expect the process to be in its final retry as it is holding crit. The only exception is if hold_onto_crit
		 * is TRUE but in that case we don't expect csd->freeze to be TRUE so we don't care much about that case. The other
		 * exception is if this is DSE which gets crit even without being in the final retry. In that case, skip the check
		 * about whether we are about to update a frozen db. DSE is the only utility allowed to update frozen databases.
		 */
		assert((CDB_STAGNATE == t_tries) || csa->hold_onto_crit || IS_DSE_IMAGE);
		if (FROZEN_HARD(csa) && !IS_DSE_IMAGE)
		{	/* We are about to update a frozen database. This is possible in rare cases even though
			 * we waited for the freeze to be lifted in t_retry (see GTM-7004). Restart in this case.
			 */
			status = cdb_sc_needcrit;
			goto failed;
		}
	}
	/* We should never proceed to update a frozen database. Only exception is DSE */
	assert(!FROZEN_HARD(csa) || IS_DSE_IMAGE);
	/* We never expect to come here with file_corrupt set to TRUE (in case of an online rollback) because
	 * grab_crit done above will make sure of that. The only exception is RECOVER/ROLLBACK itself coming
	 * here in the forward phase
	 */
	assert(!csd->file_corrupt || mupip_jnl_recover);
	if (MISMATCH_ROOT_CYCLES(csa, cnl))
	{
		status = cdb_sc_gvtrootmod2;
		DEBUG_ONLY(tmp_jnlpool = jnlpool;)
		if (MISMATCH_ONLN_RLBK_CYCLES(csa, cnl))
		{
			assert(!mupip_jnl_recover);
			status = ONLN_RLBK_STATUS(csa, cnl);
			SYNC_ONLN_RLBK_CYCLES;
			if (!TREF(in_gvcst_bmp_mark_free) || mu_reorg_process)
				SYNC_ROOT_CYCLES(NULL);
		} else
			SYNC_ROOT_CYCLES(csa);
		assert(tmp_jnlpool == jnlpool);
		goto failed;
	}
	/* We should never proceed to commit if the global variable - only_reset_clues_if_onln_rlbk - is TRUE AND if the prior
	 * retry was due to ONLINE ROLLBACK. This way, we ensure that, whoever set the global variable knows to handle ONLINE
	 * ROLLBACK and resets it before returning control to the application.
	 */
	DEBUG_ONLY(prev_status = LAST_RESTART_CODE);
	assert((cdb_sc_normal == prev_status) || ((cdb_sc_onln_rlbk1 != prev_status) && (cdb_sc_onln_rlbk2 != prev_status))
		|| (!TREF(in_gvcst_bmp_mark_free) || mu_reorg_process));
	if (is_mm && ((csa->hdr != csd) || (csa->total_blks != cti->total_blks)))
        {       /* If MM, check if wcs_mm_recover was invoked as part of the grab_crit done above OR if
                 * the file has been extended. If so, restart.
                 */
                status = cdb_sc_helpedout;      /* force retry with special status so philanthropy isn't punished */
                goto failed;
        }
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
			assert(!is_updproc || (csa->jnlpool && (csa->jnlpool == jnlpool)));
			assert(!is_updproc || (jnlpool && jnlpool->repl_inst_filehdr->is_supplementary
				&& !jnlpool->jnlpool_ctl->upd_disabled));
			assert(!jgbl.forw_phase_recovery);
			assert(cycle > csa->db_trigger_cycle);
			/* csa->db_trigger_cycle will be set to csd->db_trigger_cycle in t_retry */
			status = cdb_sc_triggermod;
			goto failed;
		}
	}
#	endif
	if ((NULL != csa->encr_ptr) && (csa->encr_ptr->reorg_encrypt_cycle != cnl->reorg_encrypt_cycle))
	{
		assert(csa->now_crit);
		SIGNAL_REORG_ENCRYPT_RESTART(mu_reorg_encrypt_in_prog, reorg_encrypt_restart_csa,
				cnl, csa, csd, status, process_id);
		goto failed;
	}
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
	CHK_AND_UPDATE_SNAPSHOT_STATE_IF_NEEDED(csa, cnl, ss_need_to_restart);
	if (cw_depth)
	{
		CHK_AND_UPDATE_BKUP_STATE_IF_NEEDED(cnl, csa, new_bkup_started);
		/* recalculate based on the new values of snapshot_in_prog and backup_in_prog. Since read_before_image used
		 * only in the context of acquired blocks, recalculation should happen only for non-zero cw_depth
		 */
		read_before_image = ((JNL_ENABLED(csa) && csa->jnl_before_image)
				     || csa->backup_in_prog
				     || SNAPSHOTS_IN_PROG(csa));
	}
	if ((cw_depth && new_bkup_started) || ss_need_to_restart)
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
	DEBUG_ONLY(tmp_jnlpool = jnlpool;)
	if (!is_mm && !WCS_GET_SPACE(reg, cw_set_depth + 1, NULL))
	{
		assert(cnl->wc_blocked);	/* only reason we currently know why wcs_get_space could fail */
		assert(gtm_white_box_test_case_enabled);
		SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
		BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_hist);
		SET_CACHE_FAIL_STATUS(status, csd);
		assert(tmp_jnlpool == jnlpool);
		goto failed;
	}
	assert(tmp_jnlpool == jnlpool);
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
			cti->curr_tn--;
			cti->early_tn--;
			decremented_currtn = TRUE;
		}
	}
	assert(csd == csa->hdr);
	valid_thru = dbtn = cti->curr_tn;
	if (!is_mm)
		oldest_hist_tn = OLDEST_HIST_TN(csa);
	valid_thru++;
	n_blks_validated = 0;
	for EACH_HIST(hist, hist1, hist2)
	{
		for (t1 = hist->h;  t1->blk_num;  t1++)
		{
			if (is_mm)
			{
				if (t1->tn <= ((blk_hdr_ptr_t)(t1->buffaddr))->tn)
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_blkmod;
					NONTP_TRACE_HIST_MOD(t1, t_blkmod_t_end2);
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
							NONTP_TRACE_HIST_MOD(t1, t_blkmod_t_end3);
							goto failed;
						} else
						{
							status = gvincr_recompute_upd_array(t1, cw_set, cr);
							if (cdb_sc_normal != status)
							{
								status = cdb_sc_blkmod;
								NONTP_TRACE_HIST_MOD(t1, t_blkmod_t_end4);
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
	 * heavily and therefore it is hard to maintain an up to date clue. reorg therefore handles this situation by actually
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
	{	/* Bit maps from mu_reorg (from a call to mu_swap_blk) or mu_reorg_upgrd_dwngrd */
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
	if ((0 != cw_map_depth) && (mu_reorg_upgrd_dwngrd_in_prog || mu_reorg_encrypt_in_prog))
	{	/* Bit maps from mu_reorg_upgrd_dwngrd. Bitmap history has been validated.
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
				 * Note that in_cw_set is set to 0 ahead of in_tend in "bg_update_phase2". Therefore
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
					cs->blk_checksum = jnl_get_checksum(old_block, csa, bsiz);
				}
#				ifdef DEBUG
				else
					assert(cs->blk_checksum == jnl_get_checksum(old_block, csa, old_block->bsiz));
#				endif
				assert(cs->cr->blk == cs->blk);
			}
		}
	}
	/* If we are not writing an INCTN record, we better have a non-zero cw_depth.
	 * The only known exceptions are
	 * 	a) If we were being called from gvcst_put for a duplicate SET
	 * 	b) If we were being called from gvcst_kill for a duplicate KILL
	 * 	c) If we were called from DSE MAPS
	 * 	d) If we were being called from gvcst_jrt_null.
	 * In case (a) and (b), we want to write logical SET or KILL journal records and replicate them.
	 * In case (c), we do not want to replicate them. we want to assert that is_replicator is FALSE in this case.
	 * the following assert achieves that purpose.
	 */
	assert((inctn_invalid_op != inctn_opcode) || cw_depth
			|| !is_replicator						/* exception case (c) */
			|| (ERR_GVPUTFAIL == t_err) && gvdupsetnoop			/* exception case (a) */
			|| (ERR_JRTNULLFAIL == t_err)					/* exception case (d) */
			|| (ERR_GVKILLFAIL == t_err) && gv_play_duplicate_kills);	/* exception case (b) */
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
		 * an issue since we don't expect to be off by more than a second or two if at all.
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
		/* Note that jnl_ensure_open can call cre_jnl_file which
		 * in turn assumes jgbl.gbl_jrec_time is set. Also jnl_file_extend can call
		 * jnl_write_epoch_rec which in turn assumes jgbl.gbl_jrec_time is set.
		 * In case of forw-phase-recovery, mur_output_record would have already set this.
		 */
		assert(jgbl.gbl_jrec_time);
		jnl_status = (!JNL_FILE_SWITCHED2(jpc, jbp) ? 0 : jnl_ensure_open(reg, csa));
		GTM_WHITE_BOX_TEST(WBTEST_T_END_JNLFILOPN, jnl_status, ERR_JNLFILOPN);
		if (0 != jnl_status)
		{
			if (save_jnlpool != jnlpool)
			{
				assert(!jnlpool_csa || (jnlpool_csa == csa));
				jnlpool = save_jnlpool;
			}
			if (SS_NORMAL != jpc->status)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) jnl_status, 4, JNL_LEN_STR(csd),
						DB_LEN_STR(reg), jpc->status);
			else
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg));
			assert(FALSE);	/* rts_error_csa done above should never return */
		}
		/* tmp_cw_set_depth was used to do TOTAL_NONTPJNL_REC_SIZE calculation earlier in this function.
		 * It is now though that the actual jnl record write occurs. Ensure that the current value of
		 * cw_set_depth does not entail any change in journal record size than was calculated.
		 * Same case with csa->jnl_before_images & jbp->before_images.
		 * The only exception is that in case of mu_reorg_{upgrd_dwngrd,encrypt}_in_prog cw_set_depth will be
		 * LESS than tmp_cw_set_depth (this is still fine as there is more size allocated than used).
		 */
		assert((cw_set_depth == tmp_cw_set_depth) || ((mu_reorg_upgrd_dwngrd_in_prog || mu_reorg_encrypt_in_prog)
				&& cw_map_depth && (cw_set_depth < tmp_cw_set_depth)));
		assert(jbp->before_images == csa->jnl_before_image);
		assert((csa->jnl_state == csd->jnl_state) && (csa->jnl_before_image == csd->jnl_before_image));
		if (jbp->last_eof_written
			|| (DISK_BLOCKS_SUM(jbp->rsrv_freeaddr, total_jnl_rec_size) > jbp->filesize))
		{	/* Moved as part of change to prevent journal records splitting
			 * across multiple generation journal files. */
			if (SS_NORMAL != (jnl_status = jnl_flush(jpc->region)))
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFLUSH, 2, JNL_LEN_STR(csd),
					ERR_TEXT, 2, RTS_ERROR_TEXT("Error with journal flush during t_end"), jnl_status);
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
		assert(!jbp->last_eof_written);
		assert(jgbl.gbl_jrec_time >= jbp->prev_jrec_time);
		if (MAXUINT4 == jbp->next_epoch_time)
			jbp->next_epoch_time = (uint4)(jgbl.gbl_jrec_time + jbp->epoch_interval);
		if (((jbp->next_epoch_time <= jgbl.gbl_jrec_time) UNCONDITIONAL_EPOCH_ONLY(|| TRUE))
						&& !FROZEN_CHILLED(csa))
		{	/* Flush the cache. Since we are in crit, defer syncing epoch */
			if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_IN_COMMIT | WCSFLU_SPEEDUP_NOBEFORE))
			{
				SET_WCS_FLU_FAIL_STATUS(status, csd);
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wc_blocked_t_end_jnl_wcsflu);
				goto failed;
			}
			assert(csd == csa->hdr);
		}
	}
	/* At this point, we are done with validation and so we need to assert that donot_commit is set to FALSE */
	assert(!TREF(donot_commit));	/* We should never commit a transaction that was determined restartable */
	jrs = NULL;
	if (REPL_ALLOWED(csa) && ((NULL != jnlpool) && (NULL != (jpl = jnlpool->jnlpool_ctl))))	/* note: assignment of "jpl" */
	{
		assert(!csa->jnlpool || (csa->jnlpool == jnlpool));
		repl_csa = &FILE_INFO(jnlpool->jnlpool_dummy_reg)->s_addrs;
		if (!repl_csa->hold_onto_crit)
			grab_lock(jnlpool->jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);	/* Step CMT02 */
		assert(repl_csa->now_crit);
		jnlpool_crit_acquired = TRUE;
		/* With jnlpool lock held, check instance freeze, and retry if set. */
		if (jpl->freeze)
		{
			rel_lock(jnlpool->jnlpool_dummy_reg);
			status = cdb_sc_instancefreeze;
			goto failed;
		}
		if (is_replicator && (inctn_invalid_op == inctn_opcode))
		{	/* Update needs to write something to the journal pool */
			replication = TRUE;
			temp_jnl_seqno = jpl->jnl_seqno;
			jnl_fence_ctl.token = temp_jnl_seqno;
			if (INVALID_SUPPL_STRM != strm_index)
			{	/* Need to also update supplementary stream seqno */
				supplementary = TRUE;
				assert(0 <= strm_index);
				strm_seqno = jpl->strm_seqno[strm_index];
				ASSERT_INST_FILE_HDR_HAS_HISTREC_FOR_STRM(strm_index, jnlpool);
				jnl_fence_ctl.strm_seqno = SET_STRM_INDEX(strm_seqno, strm_index);
			} else
			{	/* Note: "supplementary == FALSE" if strm_seqno is 0 is relied upon by "mutex_salvage" */
				assert(!jnl_fence_ctl.strm_seqno);
				supplementary = FALSE;
			}
			assert(jgbl.cumul_jnl_rec_len);
			jgbl.cumul_jnl_rec_len += SIZEOF(jnldata_hdr_struct);
			/* Make sure timestamp of this seqno is >= timestamp of previous seqno. Note: The below macro
			 * invocation should be done AFTER the ADJUST_GBL_JREC_TIME call as the below resets
			 * jpl->prev_jnlseqno_time. Doing it the other way around would mean the reset will happen
			 * with a potentially lower value than the final adjusted time written in the jnl record.
			 */
			ADJUST_GBL_JREC_TIME_JNLPOOL(jgbl, jpl);
			UPDATE_JPL_RSRV_WRITE_ADDR(jpl, jnlpool, jgbl.cumul_jnl_rec_len);/* sets jpl->rsrv_write_addr. Step CMT03 */
			/* Source server does not read in crit. It relies on the transaction data, lastwrite_len,
			 * rsrv_write_addr being updated in that order. To ensure this order, we have to force out
			 * rsrv_write_addr to its coherency point now. If not, the source server may read data that
			 * is overwritten (or stale). This is true only on architectures and OSes that allow unordered
			 * memory access.
			 */
			SHM_WRITE_MEMORY_BARRIER;
		}
	}
	assert(TN_NOT_SPECIFIED > MAX_TN_V6); /* Ensure TN_NOT_SPECIFIED isn't a valid TN number */
	blktn = (TN_NOT_SPECIFIED == ctn) ? dbtn : ctn;
	cti->early_tn = dbtn + 1;	/* Step CMT04 */
	csa->t_commit_crit = T_COMMIT_CRIT_PHASE0;	/* phase0 : write journal records. Step CMT05 */
	if (JNL_ENABLED(csa))
	{
		jrs = TREF(nontp_jbuf_rsrv);
		REINIT_JBUF_RSRV_STRUCT(jrs, csa, jpc, jbp);
		if (0 == jpc->pini_addr)
			jnl_write_reserve(csa, jrs, JRT_PINI, PINI_RECLEN, NULL);
		DEBUG_ONLY(save_gbl_jrec_time = jgbl.gbl_jrec_time;)
		if (jbp->before_images && !mu_reorg_nosafejnl)
		{	/* Write out before-update journal image records.
			 * Do not write PBLKs if MUPIP REORG UPGRADE/DOWNGRADE with -NOSAFEJNL.
			 */
			epoch_tn = jbp->epoch_tn; /* store in a local as it is used in a loop below */
			for (cs = cw_set, cs_top = cs + cw_set_depth;  cs < cs_top;  ++cs)
			{
				/* PBLK computations for FREE blocks are not needed */
				if (WAS_FREE(cs->blk_prior_state))
					continue;
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
				assert(((NULL != old_block) && (old_block->tn < epoch_tn)) || (0 == cs->jnl_freeaddr));
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
					bsiz = MIN(bsiz, csd->blk_size);	/* be safe in PRO */
					bsiz += FIXED_PBLK_RECLEN + JREC_SUFFIX_SIZE;
					bsiz = ROUND_UP2(bsiz, JNL_REC_START_BNDRY);
					jnl_write_reserve(csa, jrs, JRT_PBLK, bsiz, cs);
				}
			}
		}
		if (write_after_image)
		{	/* either DSE or MUPIP RECOVER playing an AIMG record */
			assert(1 == cw_set_depth); /* only one block at a time */
			assert(!replication);
			cs = cw_set;
			old_block = (blk_hdr_ptr_t)cs->new_buff;
			bsiz = old_block->bsiz;
			bsiz = MIN(bsiz, csd->blk_size);	/* be safe in PRO */
			bsiz += FIXED_AIMG_RECLEN + JREC_SUFFIX_SIZE;
			bsiz = ROUND_UP2(bsiz, JNL_REC_START_BNDRY);
			jnl_write_reserve(csa, jrs, JRT_AIMG, bsiz, cs);
		} else if (write_inctn)
		{
			assert(!replication);
			if ((inctn_blkupgrd == inctn_opcode) || (inctn_blkdwngrd == inctn_opcode)
					|| (inctn_blkreencrypt == inctn_opcode))
			{
				assert(1 == cw_set_depth); /* upgrade/downgrade/(re)encrypt one block at a time */
				cs = cw_set;
				assert(inctn_detail.blknum_struct.blknum == cs->blk);
				assert((inctn_blkreencrypt != inctn_opcode) || (mu_reorg_upgrd_dwngrd_blktn < dbtn));
				if (mu_reorg_nosafejnl)
				{
					assert(inctn_blkreencrypt != inctn_opcode);
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
			jnl_write_reserve(csa, jrs, JRT_INCTN, INCTN_RECLEN, NULL);
		} else if (0 == jnl_fence_ctl.level)
		{
			assert(!replication || !jgbl.forw_phase_recovery);
			if (!replication && !jgbl.forw_phase_recovery)
				jnl_fence_ctl.token = 0;
			/* In case of forw-phase of recovery, jnl_fence_ctl.token would have been set by mur_output_record */
			jnl_write_reserve(csa, jrs, non_tp_jfb_ptr->rectype, non_tp_jfb_ptr->record_size, non_tp_jfb_ptr);
		} else
		{
			if (0 == jnl_fence_ctl.token)
			{	/* generate token once after op_ztstart and use for all its mini-transactions
				 * jnl_fence_ctl.token is set to 0 in op_ztstart.
				 */
				assert(!replication);
				TOKEN_SET(&jnl_fence_ctl.token, local_tn, process_id);
			}
			jnl_write_reserve(csa, jrs, non_tp_jfb_ptr->rectype, non_tp_jfb_ptr->record_size, non_tp_jfb_ptr);
		}
		UPDATE_JRS_RSRV_FREEADDR(csa, jpc, jbp, jrs, jpl, jnl_fence_ctl, replication);	/* updates jbp->rsrv_freeaddr.
												 * Step CMT06
												 */
		/* In non-TP, there is a max of ONE set journal record that can take MAX_LOGI_JNL_REC_SIZE.
		 * This is guaranteed to fit in a journal buffer whose minimum size JNL_BUFFER_MIN is defined
		 * such that it takes all this into account.
		 */
		assert(!IS_PHASE2_JNL_COMMIT_NEEDED_IN_CRIT(jbp, jrs->tot_jrec_len));
		/* Ensure jgbl.gbl_jrec_time did not get reset by any of the jnl writing functions */
		assert(save_gbl_jrec_time == jgbl.gbl_jrec_time);
		/* If MM, there is no 2-phase commit for db. Likewise no 2-phase commit for journal. Do it all inside crit */
		if (is_mm)
			NONTP_FINISH_JNL_PHASE2_IN_JNLBUFF_AND_JNLPOOL(csa, jrs, replication, jnlpool);	/* Step CMT06a & CMT06b */
	} else if (replication)
	{	/* Case where JNL_ENABLED(csa) is FALSE but REPL_WAS_ENABLED(csa) is TRUE and therefore we need to
		 * write logical jnl records in the journal pool (no need to write in journal buffer or journal file).
		 */
		assert(!JNL_ENABLED(csa) && REPL_WAS_ENABLED(csa));
		assert(0 == jnl_fence_ctl.level);	/* ZTP & replication are not supported */
		jpc = csa->jnl;
		jbp = jpc->jnl_buff;
		jrs = TREF(nontp_jbuf_rsrv);
		REINIT_JBUF_RSRV_STRUCT(jrs, csa, jpc, jbp);
		jnl_write_reserve(csa, jrs, non_tp_jfb_ptr->rectype, non_tp_jfb_ptr->record_size, non_tp_jfb_ptr);
		/* If MM, there is no 2-phase commit for db. Likewise no 2-phase commit for journal. Do it all inside crit */
		if (is_mm)
			NONTP_FINISH_JNL_PHASE2_IN_JNLBUFF_AND_JNLPOOL(csa, jrs, replication, jnlpool);	/* Step CMT06a & CMT06b */
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
			if (SNAPSHOTS_IN_PROG(csa))
			{	/* we write the before-image to snapshot file only for FAST_INTEG and not for
				 * regular integ because the block is going to be marked free at this point
				 * and in case of a regular integ a before image will be written to the snapshot
				 * file eventually when the free block gets reused. So the before-image writing
				 * effectively gets deferred but does happen.
				 */
				lcl_ss_ctx = SS_CTX_CAST(cs_addrs->ss_ctx);
				if (lcl_ss_ctx && FASTINTEG_IN_PROG(lcl_ss_ctx) && (blkid < lcl_ss_ctx->total_blks))
					WRITE_SNAPSHOT_BLOCK(cs_addrs, cr, NULL, blkid, lcl_ss_ctx);
			}
		}
		/* Write the journal record before releasing crit as we will UNPIN the associated cache-records before
		 * releasing crit (sooner than normal).
		 */
		if (NEED_TO_FINISH_JNL_PHASE2(jrs))
		{
			/* "jnl_write_pblk" will look at "cse->old_mode" (instead of cse->mode) with the assumption that
			 * it is called in phase2 after crit has been released. But in this case we are calling
			 * "jnl_write_phase2" (which in turn calls "jnl_write_pblk") before releasing crit.
			 * So set cse->old_mode = cse->mode before the call.
			 */
			first_jre = jrs->jrs_array;
			jre_top = first_jre + jrs->usedlen;
			for (jre = first_jre; jre < jre_top; jre++)
			{
				if (JRT_PBLK != jre->rectype)
					continue;
				cs = (cw_set_element *)jre->param1;
				cs->old_mode = cs->mode;
			}
			assert(!replication);	/* so "JPL_PHASE2_WRITE_COMPLETE" does not need to be called inside
						 * NONTP_FINISH_JNL_PHASE2_IN_JNLBUFF_AND_JNLPOOL invocation below.
						 */
			NONTP_FINISH_JNL_PHASE2_IN_JNLBUFF_AND_JNLPOOL(csa, jrs, replication, jnlpool);	/* Step CMT06a */
		}
	}
	if (replication)
		SET_JNL_SEQNO(jpl, temp_jnl_seqno, supplementary, strm_seqno, strm_index, next_strm_seqno);	/* Step CMT07 */
	csa->prev_free_blks = cti->free_blocks;
	SET_T_COMMIT_CRIT_PHASE1(csa, cnl, dbtn); /* Step CMT08 */
	if (replication)
		SET_REG_SEQNO(csa, temp_jnl_seqno, supplementary, strm_index, next_strm_seqno, SKIP_ASSERT_FALSE); /* Step CMT09 */
	if (cw_set_depth)
	{
		if (!is_mm)	/* increment counter of # of processes that are actively doing two-phase commit */
			INCR_WCS_PHASE2_COMMIT_PIDCNT(csa, cnl);
#		ifdef DEBUG
		/* Assert that cs->old_mode if uninitialized, never contains a negative value (relied by secshr_db_clnup) */
		for (cs = cw_set, cs_top = cs + cw_set_depth;  cs < cs_top;  ++cs)
			assert(0 <= cs->old_mode);
#		endif
		for (cs = cw_set, cs_top = cs + cw_set_depth;  cs < cs_top;  ++cs)
		{
			mode = cs->mode;
			assert((gds_t_write_root != mode) || ((cs - cw_set) + 1 == cw_depth));
			assert((gds_t_committed > mode) ||
				(gds_t_busy2free == mode) || (gds_t_recycled2free == mode) || (gds_t_write_root == mode));
			cs->old_mode = (int4)mode;	/* note down before being reset to gds_t_committed */
			if (gds_t_committed > mode)
			{
#				ifdef DEBUG
				/* Check bitmap status of block we are about to modify.
				 * Two exceptions are
				 *	a) DSE which can modify bitmaps at will.
				 *	b) MUPIP RECOVER writing an AIMG. In this case it is playing
				 *		forward a DSE action so is effectively like DSE doing it.
				 */
				if (!IS_DSE_IMAGE && !write_after_image)
					bml_status_check(cs);
#				endif
				if (is_mm)
					status = mm_update(cs, dbtn, blktn, dummysi);	/* Step CMT10 */
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
					status = bg_update_phase1(cs, dbtn, dummysi);	/* Step CMT10 */
					if ((cdb_sc_normal == status) && (gds_t_writemap == mode))
					{	/* If we are about to do phase2 db commit while holding crit,
						 * then check if jnl phase2 is pending. If so do it also in crit
						 * before the first db phase2 commit happens.
						 */
						if (NEED_TO_FINISH_JNL_PHASE2(jrs))
							NONTP_FINISH_JNL_PHASE2_IN_JNLBUFF_AND_JNLPOOL(csa,	\
									jrs, replication, jnlpool); /* Step CMT06a & CMT06b */
						status = bg_update_phase2(cs, dbtn, blktn, dummysi);	/* Step CMT10a */
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
	update_trans |= UPDTRNS_TCOMMIT_STARTED_MASK; /* Step CMT11 */
	assert(cdb_sc_normal == status);
	/* should never increment curr_tn on a frozen database except if DSE */
	assert(!(FROZEN_HARD(csa) || (replication && jnlpool && jnlpool->jnlpool_ctl->freeze)) || IS_DSE_IMAGE);
	/* To avoid confusing concurrent processes, MM requires a barrier before incrementing db TN. For BG, cr->in_tend
	 * serves this purpose so no barrier is needed. See comment in tp_tend.
	 */
	if (is_mm)
		MM_WRITE_MEMORY_BARRIER;
	INCREMENT_CURR_TN(csd); /* Step CMT12 */
	csa->t_commit_crit = T_COMMIT_CRIT_PHASE2;	/* phase2 : update database buffers. Step CMT13.
							 * Set this BEFORE releasing crit but AFTER incrementing curr_tn.
							 */
	/* If db is journaled, then db header is flushed periodically when writing the EPOCH record,
	 * otherwise do it here every HEADER_UPDATE_COUNT transactions.
	 */
	assert(!JNL_ENABLED(csa) || (jbp == csa->jnl->jnl_buff));
	if (!JNL_ENABLED(csa) && !(csd->trans_hist.curr_tn & (HEADER_UPDATE_COUNT - 1)) && !FROZEN_CHILLED(csa))
		fileheader_sync(reg);
	assert((MUSWP_INCR_ROOT_CYCLE != TREF(in_mu_swap_root_state)) || need_kip_incr);
	if (need_kip_incr)		/* increment kill_in_prog */
	{
		INCR_KIP(csd, csa, kip_csa);
		need_kip_incr = FALSE;
		if (MUSWP_INCR_ROOT_CYCLE == TREF(in_mu_swap_root_state))
		{	/* Increment root_search_cycle to let other processes know that they should redo_root_search. */
			assert((0 != cw_map_depth) && !TREF(in_gvcst_redo_root_search));
			csa->nl->root_search_cycle++;
		}
	}
	start_tn = dbtn; /* start_tn temporarily used to store currtn (for "bg_update_phase2") before releasing crit */
	if (free_seen)
	{	/* need to do below BEFORE releasing crit as we have no other lock on this buffer */
		assert((2 <= cr_array_index) && (cr_array_index <= 3));	/* 3 is possible if we pinned a twin for bitmap update */
		assert((2 == cw_set_depth) && (process_id == cr_array[0]->in_cw_set));
		UNPIN_CACHE_RECORD(cr_array[0]);
	}
	if (!csa->hold_onto_crit)
		rel_crit(reg);	/* Step CMT14 */
	/* Now that all buffers needed for commit are locked in shared memory (in phase1 for BG), it is safe to
	 * release the jnlpool lock. Releasing it before this could cause an instance freeze to sneak in while
	 * the phase1 is still midway causing trouble for this transaction in case it needs to do any db/jnl writes.
	 */
	if (jnlpool_crit_acquired)
	{
		assert((NULL != jnlpool->jnlpool_ctl) && repl_csa->now_crit && REPL_ALLOWED(csa));
		rel_lock(jnlpool->jnlpool_dummy_reg);	/* Step CMT15 */
	}
	/* If BG, check that we have not pinned any more buffers than we are updating */
	DBG_CHECK_PINNED_CR_ARRAY_CONTENTS(csd, is_mm, cr_array, cr_array_index);
	assert((NULL == jrs) || JNL_ALLOWED(csa));
	assert((NULL == jrs) || !jrs->tot_jrec_len || !replication || jnlpool->jrs.tot_jrec_len);
	assert((NULL == jrs) || jrs->tot_jrec_len || !replication || !jnlpool->jrs.tot_jrec_len);
	if (NEED_TO_FINISH_JNL_PHASE2(jrs))
		NONTP_FINISH_JNL_PHASE2_IN_JNLBUFF_AND_JNLPOOL(csa, jrs, replication, jnlpool);	/* Step CMT16 & CMT17 */
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
					status = bg_update_phase2(cs, dbtn, blktn, dummysi);	/* Step CMT18 */
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
	ASSERT_CR_ARRAY_IS_UNPINNED(csd, cr_array, cr_array_index);
	cr_array_index = 0;
	csa->t_commit_crit = FALSE;	/* Step CMT19 */
	/* Phase 2 commits are completed. See if we had done a snapshot init (csa->snapshot_in_prog == TRUE). If so,
	 * try releasing the resources obtained while snapshot init.
	 */
	if (SNAPSHOTS_IN_PROG(csa))
		SS_RELEASE_IF_NEEDED(csa, cnl);

skip_cr_array:
	assert(!csa->now_crit || csa->hold_onto_crit);
	assert(cdb_sc_normal == status);
	REVERT;	/* no need for t_ch to be invoked if any errors occur after this point */
	DEFERRED_EXIT_HANDLING_CHECK; /* now that all crits are released, check if deferred signal/exit handling needs to be done */
	assert(update_trans);
	if (REPL_ALLOWED(csa) && IS_DSE_IMAGE)
	{
		temp_tn = dbtn + 1;
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_NOTREPLICATED, 4, &temp_tn, LEN_AND_LIT("DSE"), process_id);
	}
	INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_readwrite, 1);
	INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_blkread, n_blks_validated);
	INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_blkwrite, cw_set_depth);
	GVSTATS_SET_CSA_STATISTIC(csa, db_curr_tn, dbtn);
	/* "secshr_db_clnup/t_commit_cleanup" assume an active non-TP transaction if cw_set_depth is non-zero
	 * or if update_trans is set to T_COMMIT_STARTED. Now that the transaction is complete, reset these fields.
	 */
	DEBUG_ONLY(jgbl.cumul_index = jgbl.cu_jnl_index = 0;) 	/* symmetrical with TP and makes op_tstart checks happy */
	cw_set_depth = 0;
	/* Resetting this before CWS_RESET and process_deferred_stale is necessary to avoid coming to wcs_wtstart with non-zero
	 * update_trans and thus causing to skip the operation as a precaution against timer-based writes occurring in the midst of
	 * transaction logic.
	 */
	update_trans = 0;
	CWS_RESET;
	/* although we have the same assert at the beginning of "skip_cr_array" label, the below assert ensures that we did not grab
	 * crit in between (in process_deferred_stale() or backup_buffer_flush() or wcs_timer_start()). The assert at the beginning
	 * of the loop is needed as well to ensure that if we are done with the commit, we should have released crit
	 */
	assert(!csa->now_crit || csa->hold_onto_crit);
	t_tries = 0;	/* commit was successful so reset t_tries */
	assert(0 == cr_array_index);
	if (block_saved)
		backup_buffer_flush(reg);
	if (unhandled_stale_timer_pop)
		process_deferred_stale();
	wcs_timer_start(reg, TRUE);
	if (save_jnlpool != jnlpool)
	{
		assert(!jnlpool_csa || (jnlpool_csa == csa));
		jnlpool = save_jnlpool;
	}
	return dbtn;
failed:
	assert(cdb_sc_normal != status);
	REVERT;
failed_skip_revert:
	RESTORE_CURRTN_IF_NEEDED(csa, cti, write_inctn, decremented_currtn);
	retvalue = t_commit_cleanup(status, 0);	/* we expect to get a return value indicating update was NOT underway */
	assert(!retvalue); 			/* if it was, then we would have done a "goto skip_cr_array:" instead */
	if ((NULL != hist1) && (NULL != (gvnh = hist1->h[0].blk_num ? hist1->h[0].blk_target : NULL)))
		gvnh->clue.end = 0;
	if ((NULL != hist2) && (NULL != (gvnh = hist2->h[0].blk_num ? hist2->h[0].blk_target : NULL)))
		gvnh->clue.end = 0;
#	ifdef DEBUG
	/* Ensure we don't have t1->cse set for any gv_targets that also have their clue non-zero.
	 * As this can cause following transactions to rely on out-of-date information and do wrong things.
	 * (e.g. in t_end of the following transaction, we will see t1->cse non-NULL and conclude the buffer
	 * needs to be pinned when actually it is not necessary).
	 * The only exception to this is if we are in gvcst_bmp_mark_free. It could be invoked from op_tcommit
	 * (through gvcst_expand_free_subtree) to free up blocks in a bitmap in which case the gv_target->hist.h[x].cse
	 * cleanup has not yet happened (will take place in tp_clean_up() called a little later). In that case
	 * skip the below assert. There is a similar assert in tp_clean_up after the cleanup to ensure we do not have
	 * any non-NULL t1->cse. The global variable bml_save_dollar_tlevel identifies this exactly for us.
	 */
	if (!bml_save_dollar_tlevel)
	{
		for (gvnh = gv_target_list; NULL != gvnh; gvnh = gvnh->next_gvnh)
		{
			if (gvnh->clue.end)
			{
				for (t1 = &gvnh->hist.h[0]; t1->blk_num; t1++)
					assert(NULL == t1->cse);
			}
		}
	}
#	endif
	/* t_commit_cleanup releases crit as long as the transition is from 2nd to 3rd retry or 3rd to 3rd retry. The only exception
	 * is if hold_onto_crit is set to TRUE in which case t_commit_cleanup honors it. Assert accordingly.
	 */
	assert(!csa->now_crit || !NEED_TO_RELEASE_CRIT(t_tries, status) || csa->hold_onto_crit);
	DEFERRED_EXIT_HANDLING_CHECK; /* now that all crits are released, check if deferred signal/exit handling needs to be done */
	t_retry(status);
	/* Note that even though cw_stagnate is used only in the final retry, it is possible we restart in the final retry
	 * (see "final_retry_ok" codes in cdb_sc_table.h) and so a CWS_RESET is necessary in that case. It is anyways a no-op
	 * for non-final-retry restarts (relies on "cw_stagnate_reinitialized") so do it always to avoid unnecessary "if" check.
	 */
	CWS_RESET;
	cw_map_depth = 0;
	assert(0 == cr_array_index);
	if (save_jnlpool != jnlpool)
	{
		assert(!jnlpool_csa || (jnlpool_csa == csa));
		jnlpool = save_jnlpool;
	}
	return 0;
}
