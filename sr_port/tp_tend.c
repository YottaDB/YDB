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

#include <stddef.h>		/* for offsetof macro */

#include "gtm_stdio.h"
#include "gtm_time.h"
#include "gtm_inet.h"		/* Required for gtmsource.h */
#include "gtm_string.h"
#include "gtm_signal.h"		/* for VSIG_ATOMIC_T type */

#include "gtm_ctype.h"
#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdsbml.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"		/* needed for recompute_upd_array routine */
#include "ccp.h"
#include "copy.h"
#include "error.h"
#include "iosp.h"
#include "jnl.h"
#include "jnl_typedef.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "interlock.h"
#include "gdsbgtr.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "t_commit_cleanup.h"
#include "gvcst_blk_build.h"
#include "gvcst_protos.h"	/* for gvcst_search_blk prototype */
#include "cache.h"
#include "rc_cpt_ops.h"
#include "wcs_flu.h"
#include "jnl_write_pblk.h"
#include "jnl_write.h"
#include "wcs_backoff.h"
#include "mm_update.h"
#include "bg_update.h"
#include "wcs_get_space.h"
#include "send_msg.h"
#include "add_inter.h"
#include "t_qread.h"
#include "memcoherency.h"
#include "jnl_get_checksum.h"
#include "wbox_test_init.h"
#include "cert_blk.h"
#include "have_crit.h"
#include "bml_status_check.h"
#include "gtmimagename.h"
#ifdef DEBUG
#include "anticipatory_freeze.h"	/* needed for IS_REPL_INST_FROZEN macro */
#endif

#include "gtmrecv.h"
#include "deferred_signal_handler.h"
#include "repl_instance.h"
#include "shmpool.h"
#include "db_snapshot.h"
#include "is_proc_alive.h"
#include "process_reorg_encrypt_restart.h"

GBLREF	uint4			dollar_tlevel;
GBLREF	uint4			dollar_trestart;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*first_sgm_info, *sgm_info_ptr;
GBLREF	sgm_info		*first_tp_si_by_ftok; /* List of participating regions in the TP transaction sorted on ftok order */
GBLREF	tp_region		*tp_reg_list;
GBLREF	boolean_t		tp_kill_bitmaps;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	int4			n_pvtmods, n_blkmods;
GBLREF	unsigned int		t_tries;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	boolean_t		is_updproc;
GBLREF	boolean_t		is_replicator;
GBLREF	seq_num			seq_num_zero;
GBLREF	seq_num			seq_num_one;
GBLREF	int			gv_fillfactor;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	int			rc_set_fragment;
GBLREF	uint4			update_array_size, cumul_update_array_size;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	struct_jrec_tcom	tcom_record;
GBLREF	boolean_t		certify_all_blocks;
GBLREF	boolean_t		gvdupsetnoop; /* if TRUE, duplicate SETs update journal but not database (except for curr_tn++) */
GBLREF	gv_namehead		*gv_target;
GBLREF	trans_num		local_tn;	/* transaction number for THIS PROCESS */
GBLREF	uint4			process_id;
GBLREF	sgmnt_addrs		*reorg_encrypt_restart_csa;
GBLREF unsigned char		rdfail_detail;
GBLREF	recvpool_addrs		recvpool;
GBLREF	int4			strm_index;
#ifdef GTM_TRIGGER
GBLREF	boolean_t		skip_dbtriggers;	/* see gbldefs.c for description of this global */
GBLREF	int4			gtm_trigger_depth;
#endif
#ifdef DEBUG
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	boolean_t		multi_proc_in_use;
GBLREF	uint4			mu_reorg_encrypt_in_prog;	/* non-zero if MUPIP REORG ENCRYPT is in progress */
#endif

error_def(ERR_DLCKAVOIDANCE);
error_def(ERR_JNLFILOPN);
error_def(ERR_JNLFLUSH);
error_def(ERR_JNLTRANS2BIG);
error_def(ERR_REPLOFFJNLON);
error_def(ERR_TEXT);

enum cdb_sc	reallocate_bitmap(sgm_info *si, cw_set_element *bml_cse);
enum cdb_sc	recompute_upd_array(srch_blk_status *hist1, cw_set_element *cse);

boolean_t	tp_crit_all_regions()
{
	int			lcnt;
	boolean_t		x_lock;
	tp_region		*tr;
	sgmnt_addrs		*tmpcsa, *frozen_csa, *encr_csa;
	sgm_info		*tmpsi;
	sgmnt_data_ptr_t	tmpcsd;
	gd_region		*reg;
	enc_info_t		*encr_ptr;
	enum cdb_sc		status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(dollar_tlevel);
	/* This function is in tp_tend because its technique and structures should be maintained in parallel with tp_tend.
	 * The following section grabs crit in all regions touched by the transaction. We use a different
	 * structure here for grabbing crit. The tp_reg_list region list contains all the regions that
	 * were touched by this transaction. Since this array is sorted by the ftok value of the database
	 * file being operated on, the obtains will always occurr in a consistent manner. Therefore, we
	 * will grab crit on each file with wait since deadlock should not be able to occurr. We cannot
	 * use first_tp_si_by_ftok list because it will be setup only in tp_tend which is further down the line.
	 */
	for (lcnt = 0; ;lcnt++)
	{
		x_lock = TRUE;		/* Assume success */
		frozen_csa = NULL;
		encr_csa = NULL;
		for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
		{
			reg = tr->reg;
			tmpcsa = &FILE_INFO(reg)->s_addrs;
			tmpcsd = tmpcsa->hdr;
			tmpsi = (sgm_info *)(tmpcsa->sgm_info_ptr);
			DEBUG_ONLY(
				/* Track retries in debug mode */
				if (0 != lcnt)
				{
					BG_TRACE_ANY(tmpcsa, tp_crit_retries);
				}
			)
			assert(!tmpcsa->hold_onto_crit);
			if (!tmpcsa->now_crit)
				grab_crit(reg);	/* In "t_retry", we used "grab_crit_encr_cycle_sync" to ensure encryption
						 * cycles are in sync and crit is obtained. We cannot do that here as with TP
						 * and multiple regions, we might end up calling "grab_crit_encr_cycle_sync"
						 * for a region while holding crit on one or more previous regions and the
						 * encryption keys need to be resynced. We don't want to do that while holding
						 * crit on any region. So take care of that explicitly here just like csd->freeze.
						 */
			assert(!(tmpsi->update_trans & ~UPDTRNS_VALID_MASK));
			if (FROZEN_HARD(tmpcsa) && tmpsi->update_trans)
			{
				tr = tr->fPtr;		/* Increment so we release the crit lock we actually got */
				x_lock = FALSE;
				frozen_csa = tmpcsa;
				break;
			}
			encr_ptr = tmpcsa->encr_ptr;
			if ((NULL != encr_ptr) && (tmpcsa->nl->reorg_encrypt_cycle != encr_ptr->reorg_encrypt_cycle))
			{
				tr = tr->fPtr;		/* Increment so we release the crit lock we actually got */
				x_lock = FALSE;
				encr_csa = tmpcsa;
				SIGNAL_REORG_ENCRYPT_RESTART(mu_reorg_encrypt_in_prog, reorg_encrypt_restart_csa,
						tmpcsa->nl, tmpcsa, tmpcsd, status, process_id);
				break;
			}
		}
		if (x_lock)
			break;
		for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
		{	/* We may already have crit, if we're entering the final retry or an extra retry. So we need to
			 * release crit on all before waiting for freezes.
			 */
			reg = tr->reg;
			tmpcsa = &FILE_INFO(reg)->s_addrs;
			if (tmpcsa->now_crit)
				rel_crit(tr->reg);
		}
		if (NULL != frozen_csa)
		{	/* Wait for region to be unfrozen before re-grabbing crit on ALL regions */
			WAIT_FOR_REGION_TO_UNFREEZE(frozen_csa);
		}
		if (NULL != encr_csa)
		{
			assert(encr_csa == reorg_encrypt_restart_csa);
			process_reorg_encrypt_restart();
			assert(NULL == reorg_encrypt_restart_csa);
		}
	}	/* for (;;) */
	return TRUE;
}

boolean_t	tp_tend()
{
	block_id		tp_blk;
	boolean_t		is_mm, release_crit, x_lock, do_validation;
	boolean_t		replication = FALSE, region_is_frozen;
	boolean_t		supplementary = FALSE;	/* this variable is initialized ONLY if "replication" is TRUE. */
	seq_num			strm_seqno, next_strm_seqno;
	bt_rec_ptr_t		bt;
	cache_rec_ptr_t		cr;
	cw_set_element		*cse, *first_cw_set, *bmp_begin_cse;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	jnl_format_buffer	*jfb;
	sgm_info		*si, *si_last, *tmpsi, *si_not_validated;
	tp_region		*tr;
	sgmnt_addrs		*csa, *repl_csa = NULL;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	srch_blk_status		*t1;
	trans_num		ctn, oldest_hist_tn, epoch_tn, old_block_tn;
	trans_num		valid_thru;	/* buffers touched by this transaction will be valid thru this tn */
	enum cdb_sc		status;
	gd_region		*save_gv_cur_region;
	int			lcnt, jnl_participants, replay_jnl_participants;
	jnl_record		*rec;
	boolean_t		yes_jnl_no_repl, recompute_cksum, cksum_needed;
	boolean_t		save_dont_reset_gbl_jrec_time;
	uint4			jnl_status, leafmods, indexmods;
	uint4			total_jnl_rec_size, in_tend;
	uint4			lcl_update_trans;
	jnlpool_addrs_ptr_t	save_jnlpool, update_jnlpool, local_jnlpool;
	jnlpool_ctl_ptr_t	jpl;
	boolean_t		read_before_image; /* TRUE if before-image journaling or online backup in progress */
	blk_hdr_ptr_t		old_block;
	unsigned int		bsiz;
	cache_rec_ptr_t		*tp_cr_array;
	sgm_info		**prev_tp_si_by_ftok, *tmp_first_tp_si_by_ftok;
	gv_namehead		*prev_target, *curr_target;
	enum gds_t_mode		mode;
	boolean_t		ss_need_to_restart, new_bkup_started;
	seq_num			temp_jnl_seqno;
	jbuf_rsrv_struct_t	*jrs;
#	ifdef DEBUG
	char			upd_num_seen[256];
	int			tmp_jnl_participants;
	jnl_tm_t		save_gbl_jrec_time;
	uint4			max_upd_num, prev_upd_num, upd_num, upd_num_end, upd_num_start;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(dollar_tlevel);
	assert(0 == jnl_fence_ctl.level);
	status = cdb_sc_normal;
	save_jnlpool = jnlpool;
	update_jnlpool = NULL;
	/* if the transaction does no updates and the transaction history has not changed, we do not need any more validation */
	do_validation = FALSE;	/* initially set to FALSE, but set to TRUE below */
	assert(NULL == first_tp_si_by_ftok);
	first_tp_si_by_ftok = NULL;	/* just in case it is not set */
	prev_tp_si_by_ftok = &tmp_first_tp_si_by_ftok;
	yes_jnl_no_repl = FALSE;
	jnl_participants = 0;	/* # of regions that had a LOGICAL journal record written for this TP */
	assert(!IS_DSE_IMAGE && !TREF(in_gvcst_redo_root_search)); /* DSE and gvcst_redo_root_search work in Non-TP */
	for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
	{
		TP_CHANGE_REG_IF_NEEDED(tr->reg);
		csa = cs_addrs;
		csd = cs_data;
		cnl = csa->nl;
		is_mm = (dba_mm == csd->acc_meth);
		assert(!csa->hold_onto_crit || jgbl.onlnrlbk || TREF(in_trigger_upgrade));
		assert(!jgbl.onlnrlbk || (csa->hold_onto_crit && csa->now_crit));
		assert(!TREF(in_trigger_upgrade) || (csa->hold_onto_crit && csa->now_crit));
		si = (sgm_info *)(csa->sgm_info_ptr);
		sgm_info_ptr = si;
		*prev_tp_si_by_ftok = si;
		prev_tp_si_by_ftok = &si->next_tp_si_by_ftok;
		if ((cnl->wc_blocked) || (is_mm && (csa->total_blks != csd->trans_hist.total_blks)))
		{	/* If blocked, or we have MM and file has been extended, force repair */
			status = cdb_sc_helpedout; /* special status to prevent punishing altruism */
			assert((CDB_STAGNATE > t_tries) || !is_mm || (csa->total_blks == csd->trans_hist.total_blks));
			TP_TRACE_HIST(CR_BLKEMPTY, NULL);
			goto failed_skip_revert;
		}
		/* Note that there are three ways a deadlock can occur.
		 * 	(a) If we are not in the final retry and we already hold crit on some region.
		 * 	(b) If we are in the final retry and we don't hold crit on some region.
		 * 	(c) If we are in the final retry and we hold crit on a frozen region that we want to update.
		 * 		This is possible if:
		 *		(1) We did a "grab_crit_immediate" through one of the gvcst_* routines when we first encountered the
		 *		    region in the TP transaction and it wasn't locked down although it was frozen then.
		 *		(2) tp_crit_all_regions notices that at least one of the participating regions did ONLY READs, it
		 *		    will not wait for any freeze on THAT region to complete before grabbing crit. Later, in the
		 *		    final retry, if THAT region did an update which caused op_tcommit to invoke bm_getfree ->
		 *		    gdsfilext, then we would have come here with a frozen region on which we hold crit.
		 *	The first two cases, (a) and (b), we don't know of any way they can happen. Case (c) though can happen.
		 *	Nevertheless, we restart for all the three and in dbg version assert so we get some information.
		 *
		 *	Note that in case of an online mupip journal rollback/recover, we will hold onto crit for the entire life
		 *	of the process so that needs to be taken into account below.
		 */
		lcl_update_trans = si->update_trans;
		assert(!(lcl_update_trans & ~UPDTRNS_VALID_MASK));
		assert((UPDTRNS_JNL_LOGICAL_MASK & lcl_update_trans) || (NULL == si->jnl_head));
		assert(!(UPDTRNS_JNL_LOGICAL_MASK & lcl_update_trans) || (NULL != si->jnl_head));
		assert(!tr->reg->read_only || !lcl_update_trans);
		region_is_frozen = (lcl_update_trans && FROZEN_HARD(csa));
		if ((CDB_STAGNATE > t_tries)
				? (csa->now_crit && !csa->hold_onto_crit)
				: (!csa->now_crit || region_is_frozen))
		{
			assert(!csa->hold_onto_crit);
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_DLCKAVOIDANCE, 6, DB_LEN_STR(tr->reg),
						&csd->trans_hist.curr_tn, t_tries, dollar_trestart, csa->now_crit);
			/* The only possible case we know of is (c). assert to that effect. Use local variable region_is_frozen
			 * instead of csd->freeze as it could be concurrently changed even though we hold crit (freeze holding
			 * pid can clear it in secshr_db_clnup as part of exit processing).
			 */
			assert((CDB_STAGNATE <= t_tries) && csa->now_crit && region_is_frozen);
			status = cdb_sc_needcrit;	/* break the possible deadlock by signalling a restart */
			TP_TRACE_HIST(CR_BLKEMPTY, NULL);
			goto failed_skip_revert;
		}
		/* Whenever si->first_cw_set is non-NULL, ensure that update_trans is non-zero */
		assert((NULL == si->first_cw_set) || lcl_update_trans);
		/* Whenever si->first_cw_set is NULL, ensure that si->update_trans is FALSE. See op_tcommit.c for exceptions */
		assert((NULL != si->first_cw_set) || !si->update_trans || (UPDTRNS_ZTRIGGER_MASK & si->update_trans)
			|| (gvdupsetnoop && (!JNL_ENABLED(csa) || (NULL != si->jnl_head))));
		if (!lcl_update_trans)
		{
			/* See if we can take a fast path for read transactions based on the following conditions :
			 * 1. If the transaction number hasn't changed since we read the blocks from the disk or cache
			 * 2. If NO concurrent online rollback is running. This is needed because we don't want read transactions
			 *    to succeed. The issue with this check is that for a rollback that was killed, the PID will be non-
			 *    zero. In that case, we might skip the fast path and go ahead and do the validation. The validation
			 *    logic gets crit anyways and so will salvage the lock and do the necessary recovery and issue
			 *    DBFLCORRP if it notices that csd->file_corrupt is TRUE.
			 */
			if ((si->start_tn == csd->trans_hist.early_tn) && (0 == cnl->onln_rlbk_pid))
			{	/* read with no change to the transaction history. ensure we haven't overrun
				 * our history buffer and we have reasonable values for first and last */
				assert(si->last_tp_hist - si->first_tp_hist <= si->tp_hist_size);
				continue;
			} else
				do_validation = TRUE;
		} else
		{
			do_validation = TRUE;
			/* We are still out of crit if this is not our last attempt. If so, run the region list and check
			 * that we have sufficient free blocks for our update. If not, get them now while we can.
			 * We will repeat this check later in crit but it will hopefully have little or nothing to do.
			 * bypass 1st check if already in crit -- check later
			 */
			if (!csa->now_crit && !is_mm && !WCS_GET_SPACE(gv_cur_region, si->cw_set_depth + 1, NULL))
				assert(FALSE);	/* wcs_get_space should have returned TRUE unconditionally in this case */
			if (JNL_ENABLED(csa))
			{	/* compute the total journal record size requirements before grab_crit.
				 * there is code later that will check for state changes from now to then
				 */
				TOTAL_TPJNL_REC_SIZE(total_jnl_rec_size, si, csa);
				/* compute current transaction's maximum journal space needs in number of disk blocks */
				si->tot_jrec_size = MAX_REQD_JNL_FILE_SIZE(total_jnl_rec_size);
				GTM_WHITE_BOX_TEST(WBTEST_TP_TEND_TRANS2BIG, si->tot_jrec_size, (2 * csd->autoswitchlimit));
				/* check if current TP transaction's jnl size needs are greater than max jnl file size */
				if (si->tot_jrec_size > csd->autoswitchlimit)
					/* can't fit in current transaction's journal records into one journal file */
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_JNLTRANS2BIG, 4, si->tot_jrec_size,
						JNL_LEN_STR(csd), csd->autoswitchlimit);
			}
			if (REPL_ALLOWED(csa))
			{
				assert(JNL_ENABLED(csa) || REPL_WAS_ENABLED(csa));
				replication = TRUE;
				if (is_replicator)
				{
					if (csa->jnlpool && (jnlpool != csa->jnlpool))
						local_jnlpool = csa->jnlpool;
					else
						local_jnlpool = jnlpool;
					if (!update_jnlpool)
					{
						update_jnlpool = local_jnlpool;
						assert(update_jnlpool && update_jnlpool->jnlpool_dummy_reg);
						repl_csa = &FILE_INFO(update_jnlpool->jnlpool_dummy_reg)->s_addrs;
					} else
						assert(update_jnlpool == local_jnlpool);
				}
				jnl_participants++;
			} else if (JNL_ENABLED(csa))
			{
				yes_jnl_no_repl = TRUE;
				save_gv_cur_region = gv_cur_region; /* save the region for later error reporting */
				jnl_participants++;
			}
		}
		if (region_is_frozen)
		{	/* Wait for it to be unfrozen before proceeding to commit. This reduces the
			 * chances that we find it frozen after we grab crit further down below.
			 */
			WAIT_FOR_REGION_TO_UNFREEZE(csa);
		}
	}	/* for (tr... ) */
	*prev_tp_si_by_ftok = NULL;
	if (replication || yes_jnl_no_repl)
	{	/* The SET_GBL_JREC_TIME done below should be done before any journal writing activity
		 * on ANY region's journal file. This is because all the jnl record writing routines assume
		 * jgbl.gbl_jrec_time is initialized appropriately.
		 */
		assert(!jgbl.forw_phase_recovery || jgbl.dont_reset_gbl_jrec_time);
		if (!jgbl.dont_reset_gbl_jrec_time)
			SET_GBL_JREC_TIME;	/* initializes jgbl.gbl_jrec_time */
		assert(jgbl.gbl_jrec_time);
		/* If any one DB that we are updating has replication turned on and another has only journaling, issue error */
		if (replication)
		{
			if (yes_jnl_no_repl)
			{
				if (save_jnlpool != jnlpool)
					jnlpool = save_jnlpool;
				rts_error_csa(CSA_ARG(REG2CSA(save_gv_cur_region)) VARLSTCNT(4) ERR_REPLOFFJNLON, 2,
						DB_LEN_STR(save_gv_cur_region));
			}
			/* If caller does NOT want this update to be replicated, turn "replication" local variable off.
			 * The only such caller known at this time is "trigger_upgrade" - 2014/05/02.
			 */
			if (!is_replicator)
				replication = FALSE;
		}
	}
	if (!do_validation)
	{
		if ((CDB_STAGNATE <= t_tries) && !jgbl.onlnrlbk)
		{
			for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
			{
#				ifdef DEBUG
				csa = &FILE_INFO(tr->reg)->s_addrs;
				assert(!csa->hold_onto_crit);
#				endif
				rel_crit(tr->reg);
			}
		} /* else we are online rollback and we already hold crit on all regions */
		if (save_jnlpool != jnlpool)
			jnlpool = save_jnlpool;
		return TRUE;
	}
	if (jnlpool && (NULL == update_jnlpool))
		update_jnlpool = jnlpool;
	/* Because secshr_db_clnup uses first_tp_si_by_ftok to determine if a TP transaction is underway and expects
	 * a well-formed linked list if it is non-zero, the following assignment to the head of the region list must occur
	 * after the loop above
	 */
	first_tp_si_by_ftok = tmp_first_tp_si_by_ftok;
#	ifdef DEBUG
	/* Cross-check the validity of the ftok sorted sgm_info list with "tp_reg_list" */
	tr = tp_reg_list;
	for (si = first_tp_si_by_ftok;  (NULL != si);  si = si->next_tp_si_by_ftok)
	{
		tmpsi = (sgm_info *)(FILE_INFO(tr->reg)->s_addrs.sgm_info_ptr);
		assert(tmpsi == si);
		tr = tr->fPtr;
	}
	assert(NULL == tr);
#	endif
	assert(cdb_sc_normal == status);
	/* The following section of code (initial part of the for loop) is similar to the function "tp_crit_all_regions".
	 * The duplication is there only because of performance reasons. The latter function has to go through tp_reg_list
	 * linked list while here we can go through first_tp_si_by_ftok list which offers a performance advantage.
	 *
	 * The following section grabs crit in all regions touched by the transaction. We use a different
	 * structure here for grabbing crit. The tp_reg_list region list contains all the regions that
	 * were touched by this transaction. Since this array is sorted by the ftok value of the database
	 * file being operated on, the obtains will always occurr in a consistent manner. Therefore, we
	 * will grab crit on each file with wait since deadlock should not be able to occurr.
	 */
	ESTABLISH_NOUNWIND(t_ch);	/* avoid hefty setjmp call, which is ok since we never unwind t_ch */
	for (lcnt = 0; ; lcnt++)
	{
		x_lock = TRUE;		/* Assume success */
		DEBUG_ONLY(tmp_jnl_participants = 0;)
		/* The following loop grabs crit, does validations and prepares for commit on ALL participating regions */
		for (si = first_tp_si_by_ftok;  (NULL != si); si = si->next_tp_si_by_ftok)
		{
			sgm_info_ptr = si;
			TP_TEND_CHANGE_REG(si);
			csa = cs_addrs;
			cnl = csa->nl;
			csd = cs_data;
			assert(!si->cr_array_index);
			DEBUG_ONLY(
				/* Track retries in debug mode */
				if (0 != lcnt)
				{
					BG_TRACE_ANY(csa, tp_crit_retries);
				}
			)
			lcl_update_trans = si->update_trans;
			assert(!(lcl_update_trans & ~UPDTRNS_VALID_MASK));
			first_cw_set = si->first_cw_set;
			/* whenever si->first_cw_set is non-NULL, ensure that si->update_trans is non-zero */
			assert((NULL == first_cw_set) || lcl_update_trans);
			/* When si->first_cw_set is NULL, ensure that si->update_trans is FALSE. See op_tcommit.c for exceptions */
			assert((NULL != si->first_cw_set) || !si->update_trans || (UPDTRNS_ZTRIGGER_MASK & si->update_trans)
				|| (gvdupsetnoop && (!JNL_ENABLED(csa) || (NULL != si->jnl_head))));
			leafmods = indexmods = 0;
			is_mm = (dba_mm == csd->acc_meth);
			if (TREF(tprestart_syslog_delta))
				n_blkmods = n_pvtmods = 0;
			/* If we already hold crit (possible if we are in the final retry), do not invoke grab_crit as it will
			 * invoke wcs_recover unconditionally if cnl->wc_blocked is set to TRUE. In that case, we want to
			 * restart with a helped out code because the cache recovery will most likely result in a restart of
			 * the current transaction which we want to avoid if we are in the final retry.
			 */
			if (!csa->now_crit)
				grab_crit(gv_cur_region); /* Step CMT01 (see secshr_db_clnup.c for CMTxx step descriptions) */
			else if (cnl->wc_blocked)
			{
				status = cdb_sc_helpedout;
				goto failed;
			}
			if (is_mm && ((csa->hdr != csd) || (csa->total_blks != csd->trans_hist.total_blks)))
			{       /* If MM, check if wcs_mm_recover was invoked as part of the grab_crit done above OR if
				 * the file has been extended. If so, restart.
				 */
				status = cdb_sc_helpedout;      /* force retry with special status so philanthropy isn't punished */
				goto failed;
			}
			/* Note that even though we ensured that regions are not frozen outside of crit, it is still possible
			 * that they become frozen just before we grab crit. In this case (should be rare though) release
			 * crit on ALL regions that we have grabbed uptil this point and wait for the freeze to be removed.
			 */
			if (FROZEN_HARD(csa) && lcl_update_trans)
			{
				x_lock = FALSE;
				break;
			}
			CHECK_TN(csa, csd, csd->trans_hist.curr_tn);	/* can issue rts_error TNTOOLARGE */
			if (!is_mm)
				oldest_hist_tn = OLDEST_HIST_TN(csa);
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
					SYNC_ROOT_CYCLES(NULL);
				} else
					SYNC_ROOT_CYCLES(csa);
				goto failed;
			}
#			ifdef GTM_TRIGGER
			if (!skip_dbtriggers && si->tp_set_sgm_done && (csa->db_trigger_cycle != csd->db_trigger_cycle))
			{	/* The process' view of the triggers could be potentially stale. restart to be safe.
				 * Note: We need to validate triggers ONLY if the region (pointed to by si) was actually referenced
				 * in this retry of the transaction. Hence the si->tp_set_sgm_done check.
				 */
				/* Triggers can be invoked only by GT.M and Update process. Out of these, we expect only
				 * GT.M to see restarts due to concurrent trigger changes. Update process is the only
				 * updater on the secondary so we don't expect it to see any concurrent trigger changes.
				 * The only exception is if this is a supplementary root primary instance. In that case,
				 * the update process coexists with GT.M processes and hence can see restarts due to
				 * concurrent trigger changes. Assert accordingly.
				 */
				assert(CDB_STAGNATE > t_tries);
				assert(!is_updproc || (update_jnlpool && (!csa->jnlpool || (update_jnlpool == csa->jnlpool))));
				assert(!is_updproc || (update_jnlpool && (update_jnlpool->repl_inst_filehdr->is_supplementary
								&& !update_jnlpool->jnlpool_ctl->upd_disabled)));
				assert(csd->db_trigger_cycle > csa->db_trigger_cycle);
				/* csa->db_trigger_cycle will be set to csd->db_trigger_cycle for all participating
				 * regions when they are each first referenced in the next retry (in tp_set_sgm)\
				 */
				status = cdb_sc_triggermod;
				goto failed;
			}
#			endif
			if ((NULL != csa->encr_ptr) && (csa->encr_ptr->reorg_encrypt_cycle != cnl->reorg_encrypt_cycle))
			{
				assert(csa->now_crit);
				SIGNAL_REORG_ENCRYPT_RESTART(mu_reorg_encrypt_in_prog, reorg_encrypt_restart_csa,
						cnl, csa, csd, status, process_id);
				goto failed;
			}
			if (lcl_update_trans)
			{
				assert((NULL == first_cw_set) || (0 != si->cw_set_depth));
#				ifdef DEBUG
				/* Recompute # of replicated regions inside of crit */
				if (REPL_ALLOWED(csa))
				{
					tmp_jnl_participants++;
				} else if (JNL_ENABLED(csa))
				{
					assert(!replication); /* should have issued a REPLOFFJNLON error outside of crit */
					tmp_jnl_participants++;
				}
#				endif
				if (JNL_ALLOWED(csa))
				{
					if ((csa->jnl_state != csd->jnl_state) || (csa->jnl_before_image != csd->jnl_before_image))
					{	/* Take this opportunity to check/sync ALL regions where csa/csd don't match */
						for (tmpsi = first_tp_si_by_ftok;
							(NULL != tmpsi);
							tmpsi = tmpsi->next_tp_si_by_ftok)
						{
							csa = tmpsi->tp_csa;
							csd = csa->hdr;
							cnl = csa->nl;
							csa->jnl_state = csd->jnl_state;
							csa->jnl_before_image = csd->jnl_before_image;
							/* jnl_file_lost causes a jnl_state transition from jnl_open to jnl_closed
							 * and additionally causes a repl_state transition from repl_open to
							 * repl_closed all without standalone access. This means that
							 * csa->repl_state might be repl_open while csd->repl_state might be
							 * repl_closed. update csa->repl_state in this case as otherwise the rest
							 * of the code might look at csa->repl_state and incorrectly conclude
							 * replication is on and generate sequence numbers when actually no journal
							 * records are being generated. [C9D01-002219]
							 */
							csa->repl_state = csd->repl_state;
						}
						status = cdb_sc_jnlstatemod;
						goto failed;
					}
				}
				/* Flag retry, if other mupip activities like BACKUP, INTEG or FREEZE are in progress.
				 * If in final retry, go ahead with kill. BACKUP/INTEG/FREEZE will wait for us to be done.
				 */
				if ((NULL != si->kill_set_head) && (0 < cnl->inhibit_kills) && (CDB_STAGNATE > t_tries))
				{
					status = cdb_sc_inhibitkills;
					goto failed;
				}
				/* Caution : since csa->backup_in_prog was initialized in op_tcommit only if si->first_cw_set was
				 * non-NULL, it should be used in tp_tend only within an if (NULL != si->first_cw_set)
				 */
				if (NULL != first_cw_set)
				{
					ss_need_to_restart = new_bkup_started = FALSE;
					CHK_AND_UPDATE_SNAPSHOT_STATE_IF_NEEDED(csa, cnl, ss_need_to_restart);
					CHK_AND_UPDATE_BKUP_STATE_IF_NEEDED(cnl, csa, new_bkup_started);
					if (ss_need_to_restart
						|| (new_bkup_started && !(JNL_ENABLED(csa) && csa->jnl_before_image)))
					{
						/* If online backup is in progress now and before-image journaling is
						 * not enabled, we would not have read before-images for created blocks.
						 * Although it is possible that this transaction might not have blocks
						 * with gds_t_create at all, we expect this backup_in_prog state change
						 * to be so rare that it is ok to restart.
						 */
						status = cdb_sc_bkupss_statemod;
						goto failed;
					}
				}
				/* recalculate based on the new values of snapshot_in_prog and backup_in_prog */
				read_before_image = ((JNL_ENABLED(csa) && csa->jnl_before_image)
						     || csa->backup_in_prog
						     || SNAPSHOTS_IN_PROG(csa));
				if (!is_mm)
				{	/* in crit, ensure cache-space is available.
					 * the out-of-crit check done above might not be enough
					 */
					if (!WCS_GET_SPACE(gv_cur_region, si->cw_set_depth + 1, NULL))
					{
						assert(cnl->wc_blocked);	/* only reason we currently know
										 * why wcs_get_space could fail */
						assert(gtm_white_box_test_case_enabled);
						SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_wcsgetspace);
						SET_CACHE_FAIL_STATUS(status, csd);
						TP_TRACE_HIST(CR_BLKEMPTY, NULL);
						goto failed;
					}
				}
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
					/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time order
					 * of jnl records. This needs to be done BEFORE the jnl_ensure_open as that could write
					 * journal records (if it decides to switch to a new journal file).
					 */
					ADJUST_GBL_JREC_TIME(jgbl, jbp);
					/* Note that jnl_ensure_open can call cre_jnl_file which in turn assumes
					 * jgbl.gbl_jrec_time is set. Also jnl_file_extend can call jnl_write_epoch_rec
					 * which in turn assumes jgbl.gbl_jrec_time is set. In case of forw-phase-recovery,
					 * mur_output_record would have already set this.
					 */
					assert(jgbl.gbl_jrec_time);
					jnl_status = jnl_ensure_open(gv_cur_region, csa);
					GTM_WHITE_BOX_TEST(WBTEST_TP_TEND_JNLFILOPN, jnl_status, ERR_JNLFILOPN);
					if (0 != jnl_status)
					{
						ctn = csd->trans_hist.curr_tn;
						assert(csd->trans_hist.early_tn == ctn);
						if (save_jnlpool != jnlpool)
							jnlpool = save_jnlpool;
						if (SS_NORMAL != jpc->status)
							rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) jnl_status, 4, JNL_LEN_STR(csd),
								DB_LEN_STR(gv_cur_region), jpc->status);
						else
							rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd),
								DB_LEN_STR(gv_cur_region));
						assert(FALSE);	/* rts_error_csa done above should never return */
					}
					if (jbp->last_eof_written
							|| (DISK_BLOCKS_SUM(jbp->rsrv_freeaddr, si->total_jnl_rec_size)
														> jbp->filesize))
					{	/* Moved here to prevent jnlrecs split across multiple generation journal files. */
						if (SS_NORMAL != (jnl_status = jnl_flush(jpc->region)))
						{
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFLUSH, 2, JNL_LEN_STR(csd),
								ERR_TEXT, 2, RTS_ERROR_TEXT("Error with journal flush in tp_tend"),
								jnl_status);
							assert((!JNL_ENABLED(csd)) && JNL_ENABLED(csa));
							status = cdb_sc_jnlclose;
							TP_TRACE_HIST(CR_BLKEMPTY, NULL);
							goto failed;
						} else if (EXIT_ERR == jnl_file_extend(jpc, si->total_jnl_rec_size))
						{
							assert((!JNL_ENABLED(csd)) && JNL_ENABLED(csa));
							assert(csd == csa->hdr);	/* If MM, csd shouldn't have been reset */
							status = cdb_sc_jnlclose;
							TP_TRACE_HIST(CR_BLKEMPTY, NULL);
							goto failed;
						}
						assert(csd == csa->hdr);	/* If MM, csd shouldn't have been reset */
					}
					if (MAXUINT4 == jbp->next_epoch_time)
						jbp->next_epoch_time = (uint4)(jgbl.gbl_jrec_time + jbp->epoch_interval);
					if (((jbp->next_epoch_time <= jgbl.gbl_jrec_time) UNCONDITIONAL_EPOCH_ONLY(|| TRUE))
						&& !FROZEN_CHILLED(csa))
					{	/* Flush the cache. Since we are in crit, defer syncing the epoch */
						/* Note that at this point, jgbl.gbl_jrec_time has been computed taking into
						 * account the current system time & the last journal record timestamp of ALL
						 * regions involved in this TP transaction. To prevent wcs_flu from inadvertently
						 * setting this BACK in time (poses out-of-order timestamp issues for backward
						 * recovery and is asserted later in tp_tend) set jgbl.dont_reset_gbl_jrec_time
						 * to TRUE for the duration of the wcs_flu.
						 * Also, in case of rts_error from wcs_flu, t_ch will be invoked which will take
						 * care of restoring this variable to FALSE. Any new codepath in mumps that sets
						 * this variable for the duration of wcs_flu should take care of resetting this
						 * back to FALSE in an existing condition handler (or by creating a new one if not
						 * already present)
						 * Since, this global is set to TRUE explicitly by forward recovery, we should NOT
						 * reset this to FALSE unconditionally. But, instead of checking if forward recovery
						 * is TRUE, save and restore this variable unconditionally thereby saving a few
						 * CPU cycles.
						 */
						save_dont_reset_gbl_jrec_time = jgbl.dont_reset_gbl_jrec_time;
						jgbl.dont_reset_gbl_jrec_time = TRUE;
						if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_IN_COMMIT
												| WCSFLU_SPEEDUP_NOBEFORE))
						{
							assert(csd == csa->hdr);
							jgbl.dont_reset_gbl_jrec_time = save_dont_reset_gbl_jrec_time;
							SET_WCS_FLU_FAIL_STATUS(status, csd);
							SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_jnl_wcsflu);
							TP_TRACE_HIST(CR_BLKEMPTY, NULL);
							goto failed;
						}
						jgbl.dont_reset_gbl_jrec_time = save_dont_reset_gbl_jrec_time;
						assert(csd == csa->hdr);
					}
					assert(jgbl.gbl_jrec_time >= jbp->prev_jrec_time);
				}	/* if (journaling) */
			}
			/* the following section verifies that the optimistic concurrency was justified */
			assert(cdb_sc_normal == status);
			for (t1 = si->first_tp_hist;  t1 != si->last_tp_hist; t1++)
			{
				assert(NULL != t1->blk_target);
				cse = t1->cse;
				if (is_mm)
				{	/* the check below is different from the one for BG (i.e. doesn't have the killtn check)
					 * because there is no BT equivalent in MM. there is a mmblk_rec which is more or
					 * less the same as a BT. when the mmblk_rec becomes as fully functional as BT, we
					 * can use the killtn optimization for MM also.
					 */
					if (t1->tn <= ((blk_hdr_ptr_t)t1->buffaddr)->tn)
					{
						assert(CDB_STAGNATE > t_tries);
						assert(!cse || !cse->high_tlevel);
						if (!cse || !cse->recompute_list_head || cse->write_type
							|| (cdb_sc_normal != recompute_upd_array(t1, cse)) || !++leafmods)
						{
							status = cdb_sc_blkmod;
							TP_TRACE_HIST(t1->blk_num, t1->blk_target);
							DEBUG_ONLY(continue;)
							PRO_ONLY(goto failed;)
						}
					}
				} else
				{
					bt = bt_get(t1->blk_num);
					if (NULL != bt)
					{
						if (t1->tn <= bt->tn)
						{
							assert(!cse || !cse->high_tlevel);
							assert(CDB_STAGNATE > t_tries);
							/* "indexmods" and "leafmods" are to monitor number of blocks that used
							 * indexmod and noisolation optimizations respectively. Note that once
							 * in this part of the code, atleast one of them will be non-zero and
							 * if both of them turn out to be non-zero, then we need to restart.
						 	 * See gdscc.h for a description of the indexmod optimization.
							 */
							if (t1->level)
							{
								if (cse || t1->tn <= bt->killtn)
									status = cdb_sc_blkmod;
								else
								{
									indexmods++;
									t1->blk_target->clue.end = 0;
									if (leafmods)
										status = cdb_sc_blkmod;
								}
							} else
							{	/* For a non-isolated global, if the leaf block isn't part of the
								 * cw-set, this means that it was involved in an M-kill that freed
								 * the data-block from the B-tree. In this case, if the leaf-block
								 * has changed since we did our read of the block, we have to redo
								 * the M-kill. But since redo of that M-kill might involve much
								 * more than just leaf-level block changes, we will be safe and do
								 * a restart. If the need for NOISOLATION optimization for M-kills
								 * is felt, we need to revisit this.
								 */
								if (!t1->blk_target->noisolation || !cse)
									status = cdb_sc_blkmod;
								else
								{
									assert(cse->write_type || cse->recompute_list_head);
									leafmods++;
									if (indexmods || cse->write_type
											|| (cdb_sc_normal !=
												recompute_upd_array(t1, cse)))
										status = cdb_sc_blkmod;
								}
							}
							if (cdb_sc_normal != status)
							{
								if (TREF(tprestart_syslog_delta))
								{
									n_blkmods++;
									if (cse)
										n_pvtmods++;
									if (1 != n_blkmods)
										continue;
								}
								assert(CDB_STAGNATE > t_tries);
								status = cdb_sc_blkmod;
								TP_TRACE_HIST_MOD(t1->blk_num, t1->blk_target,
										tp_blkmod_tp_tend, csd, t1->tn, bt->tn, t1->level);
								DEBUG_ONLY(continue;)
								PRO_ONLY(goto failed;)
							}
						}
					} else if (t1->tn <= oldest_hist_tn)
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_losthist;
						TP_TRACE_HIST(t1->blk_num, t1->blk_target);
						DEBUG_ONLY(continue;)
						PRO_ONLY(goto failed;)
					}
					assert(CYCLE_PVT_COPY != t1->cycle);
					if (cse)
					{	/* Do cycle check only if blk has cse and hasn't been built (if it has, then tp_hist
						 * would have done the cdb_sc_lostcr check soon after it got built) or if we have BI
						 * journaling or online backup is currently running. The BI-journaling/online-backup
						 * check is to ensure that the before-image/pre-update-copy we write hasn't been
						 * recycled.
						 */
						if ((NULL == bt) || (CR_NOTVALID == bt->cache_index))
							cr = db_csh_get(t1->blk_num);
						else
						{
							cr = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
							if ((NULL != cr) && (cr->blk != bt->blk))
							{
								assert(FALSE);
								SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
								BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_crbtmismatch1);
								status = cdb_sc_crbtmismatch;
								TP_TRACE_HIST(t1->blk_num, t1->blk_target);
								goto failed;
							}
						}
						if ((cache_rec_ptr_t)CR_NOTVALID == cr)
						{
							SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_t1);
							SET_CACHE_FAIL_STATUS(status, csd);
							TP_TRACE_HIST(t1->blk_num, t1->blk_target);
							goto failed;
						}
						assert(lcl_update_trans);	/* ensure read_before_image was computed above */
						if (!cse->new_buff || read_before_image)
						{
							if ((NULL == cr) || (cr->cycle != t1->cycle)
								|| ((sm_long_t)GDS_ANY_REL2ABS(csa, cr->buffaddr)
									!= (sm_long_t)t1->buffaddr))
							{
								if ((NULL != cr) && (NULL != bt) && (cr->blk != bt->blk))
								{
									assert(FALSE);
									SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
									BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_crbtmismatch2);
									status = cdb_sc_crbtmismatch;
									TP_TRACE_HIST(t1->blk_num, t1->blk_target);
									goto failed;
								}
								assert(CDB_STAGNATE > t_tries);
								status = cdb_sc_lostcr;
								TP_TRACE_HIST(t1->blk_num, t1->blk_target);
								DEBUG_ONLY(continue;)
								PRO_ONLY(goto failed;)
							}
							/* Now that we know the cache-record is still valid, pin it.
							 *
							 * It is possible that t1->cse is non-NULL even though we eventually
							 * decided NOT to update that particular block e.g. if t1->cse->mode
							 * was originally t_write but later got set to kill_t_write. In such
							 * cases, don't set in_cw_set as we don't need this buffer pinned at all.
							 */
							assert(n_gds_t_op != cse->mode);
							assert(kill_t_create > n_gds_t_op);
							assert(kill_t_write  > n_gds_t_op);
							if (n_gds_t_op > cse->mode)
								TP_PIN_CACHE_RECORD(cr, si);
						}
						/* The only case cr can be NULL at this point of code is when
						 * 	a) cse->new_buff is non-NULL
						 *	b) AND the block is not in cache
						 *	c) AND we don't have before-image-journaling
						 *	d) AND online backup is not running.
						 * In this case bg_update will do a db_csh_getn and appropriately set in_cw_set
						 * field to be TRUE so no need to pin the cache-record here.
						 */
					}
				}
			} /* for (t1 ... ) */
			DEBUG_ONLY(
				if (cdb_sc_normal != status)
					goto failed;
				else
				{	/* Now that we have successfully validated all histories, check that there is no
					 * gv_target mismatch between history and corresponding cse.
					 */
					for (t1 = si->first_tp_hist;  t1 != si->last_tp_hist; t1++)
					{
						cse = t1->cse;
						if (NULL != cse)
							assert(t1->blk_target == cse->blk_target);
					}
				}
			)
			if (DIVIDE_ROUND_UP(si->num_of_blks, 4) < leafmods)	/* if status == cdb_sc_normal, then leafmods  */
			{
				status = cdb_sc_toomanyrecompute;		/* is exactly the number of recomputed blocks */
				goto failed;
			}
			assert(cdb_sc_normal == status);
			if (NULL == first_cw_set)
				continue;
			/* Check bit maps for usage */
			for (cse = si->first_cw_bitmap; NULL != cse; cse = cse->next_cw_set)
			{
				assert(0 == cse->jnl_freeaddr);	/* ensure haven't missed out resetting jnl_freeaddr for any cse in
								 * t_write/t_create/{t,mu}_write_map/t_write_root [D9B11-001991] */
				TRAVERSE_TO_LATEST_CSE(cse);
				assert(0 == ((off_chain *)&cse->blk)->flag);
				assert(!cse->high_tlevel);
				if (is_mm)
				{
					if ((cse->tn <= ((blk_hdr_ptr_t)cse->old_block)->tn)
						&& (status = reallocate_bitmap(si, cse)))	/* WARNING assignment */
					{
						assert(CDB_STAGNATE > t_tries);
						TP_TRACE_HIST(cse->blk, NULL);
						goto failed;
					}
				} else
				{
					tp_blk = cse->blk;
					bt = bt_get(tp_blk);
					if (NULL != bt)
					{
						if ((cse->tn <= bt->tn) && (status = reallocate_bitmap(si, cse)))
						{	/* WARNING assignment above */
							assert(CDB_STAGNATE > t_tries);
							TP_TRACE_HIST(tp_blk, NULL);
							goto failed;
						}
					} else if (cse->tn <= oldest_hist_tn)
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_lostbmlhist;
						TP_TRACE_HIST(tp_blk, NULL);
						goto failed;
					}
					assert(NULL == cse->new_buff);
					if ((NULL == bt) || (CR_NOTVALID ==  bt->cache_index))
					{
						cr = db_csh_get(tp_blk);
						if ((cache_rec_ptr_t)CR_NOTVALID == cr)
						{
							SET_CACHE_FAIL_STATUS(status, csd);
							TP_TRACE_HIST(tp_blk, NULL);
							SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_bitmap);
							goto failed;
						}
					} else
					{
						cr = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
						if (cr->blk != bt->blk)
						{
							assert(FALSE);
							SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_crbtmismatch3);
							status = cdb_sc_crbtmismatch;
							TP_TRACE_HIST(tp_blk, NULL);
							goto failed;
						}
					}
					if ((NULL == cr) || (cr->cycle != cse->cycle) ||
						((sm_long_t)GDS_ANY_REL2ABS(csa, cr->buffaddr) != (sm_long_t)cse->old_block))
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_lostbmlcr;
						TP_TRACE_HIST(tp_blk, NULL);
						goto failed;
					}
					TP_PIN_CACHE_RECORD(cr, si);
				}
			}	/* for (all bitmaps written) */
			si->backup_block_saved = FALSE;
			jbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
			/* Caution : since csa->backup_in_prog was initialized in op_tcommit only if si->first_cw_set was
			 * non-NULL, it should be used in tp_tend only within an if (NULL != si->first_cw_set)
			 */
			if (!is_mm && ((NULL != jbp) || csa->backup_in_prog || SNAPSHOTS_IN_PROG(csa)))
			{
				for (cse = first_cw_set;  NULL != cse;  cse = cse->next_cw_set)
				{	/* have already read old block for creates before we got crit, make sure
					 * cache record still has correct block. if not, reset "cse" fields to
					 * point to correct cache-record. this is ok to do since we only need the
					 * prior content of the block (for online backup or before-image journaling)
					 * and did not rely on it for constructing the transaction. Restart if
					 * block is not present in cache now or is being read in currently.
					 */
					TRAVERSE_TO_LATEST_CSE(cse);
					if (gds_t_acquired == cse->mode && (NULL != cse->old_block))
					{
						assert(CYCLE_PVT_COPY != cse->cycle);
						cr = db_csh_get(cse->blk);
						if ((cache_rec_ptr_t)CR_NOTVALID == cr)
						{
							TP_TRACE_HIST(cse->blk, cse->blk_target);
							SET_CACHE_FAIL_STATUS(status, csd);
							SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_jnl_cwset);
							goto failed;
						}
						/* It is possible that cr->in_cw_set is non-zero in case a concurrent MUPIP REORG
						 * UPGRADE/DOWNGRADE is in PHASE2 touching this very same block. In that case,
						 * we cannot reuse this block so we restart. We could try finding a different block
						 * to acquire instead and avoid a restart (tracked as part of C9E11-002651).
						 * Note that in_cw_set is set to 0 ahead of in_tend in bg_update_phase2. Therefore
						 * it is possible that we see in_cw_set 0 but in_tend is still non-zero. In that
						 * case, we cannot proceed with pinning this cache-record as the cr is still locked
						 * by the other process. We can choose to wait here but instead decide to restart.
						 */
						if ((NULL == cr) || (0 <= cr->read_in_progress)
							|| (0 != cr->in_cw_set) || (0 != cr->in_tend))
						{
							TP_TRACE_HIST(cse->blk, cse->blk_target);
							assert(CDB_STAGNATE > t_tries);
							status = cdb_sc_lostbefor;
							goto failed;
						}
						TP_PIN_CACHE_RECORD(cr, si);
						cse->ondsk_blkver = cr->ondsk_blkver;
						old_block = (blk_hdr_ptr_t)GDS_REL2ABS(cr->buffaddr);
						assert((cse->cr != cr) || (cse->old_block == (sm_uc_ptr_t)old_block));
						old_block_tn = old_block->tn;
						/* Need checksums if before imaging and if a PBLK record is going to be written. */
						cksum_needed = (!WAS_FREE(cse->blk_prior_state) && (NULL != jbp)
									 && (old_block_tn < jbp->epoch_tn));
						if ((cse->cr != cr) || (cse->cycle != cr->cycle))
						{	/* Block has relocated in the cache. Adjust pointers to new location. */
							cse->cr = cr;
							cse->cycle = cr->cycle;
							cse->old_block = (sm_uc_ptr_t)old_block;
							/* PBLK checksum was computed outside-of-crit when block was read but
							 * block has relocated in the cache since then so recompute the checksum
							 * if this block needs a checksum in the first place (cksum_needed is TRUE).
							 */
							recompute_cksum = cksum_needed;
						} else if (cksum_needed)
						{	/* We have determined that a checksum is needed for this block. If we
							 * have not previously computed one outside crit OR if the block contents
							 * have changed since the checksum was previously computed, we need to
							 * recompute it. Otherwise, the out-of-crit computed value can be safely
							 * used. Note that cse->tn is valid only if a checksum was computed outside
							 * of crit. So make sure it is used only if checksum is non-zero. There is
							 * a rare chance that the computed checksum could be zero in which case we
							 * will recompute unnecessarily. Since that is expected to be very rare,
							 * it is considered ok for now.
							 */
							recompute_cksum = (!cse->blk_checksum || (cse->tn <= old_block_tn));
						}
						if (!cksum_needed)
							cse->blk_checksum = 0;	/* zero any out-of-crit computed checksum */
						else if (recompute_cksum)
						{	/* We hold crit at this point so we are guaranteed valid bsiz field.
							 * Hence we do not need to verify if bsiz is lesser than csd->blk_size
							 * like we did in an earlier call to jnl_get_checksum (in op_tcommit.c).
							 */
							assert(NULL != jbp);
							assert(SIZEOF(bsiz) == SIZEOF(old_block->bsiz));
							bsiz = old_block->bsiz;
							assert(bsiz <= csd->blk_size);
							cse->blk_checksum = jnl_get_checksum(old_block, csa, bsiz);
						}
#						ifdef DEBUG
						else
							assert(cse->blk_checksum ==
									jnl_get_checksum(old_block, csa, old_block->bsiz));
#						endif
						assert(cse->cr->blk == cse->blk);
					}	/* end if acquired block */
				}	/* end cse for loop */
			}	/* end if !mm && before-images need to be written */
			assert(cdb_sc_normal == status);
		}
		if (x_lock)
			break;
		assert(csd == si->tp_csd);
		si = si->next_tp_si_by_ftok;	/* Increment so we release the lock we actually got */
		si_last = si;
		for (si = first_tp_si_by_ftok;  (si_last != si);  si = si->next_tp_si_by_ftok)
		{
			assert(si->tp_csa->now_crit);
			tp_cr_array = si->cr_array;
			UNPIN_CR_ARRAY_ON_RETRY(tp_cr_array, si->cr_array_index);
			assert(!si->cr_array_index);
			if (!si->tp_csa->hold_onto_crit)
				rel_crit(si->gv_cur_region);
		}
		/* Check that we DON'T own crit/commit on ANY region. The only exception is online mupip journal rollback/recovery
		 * which holds crit for the entire process lifetime.
		 */
		assert(jgbl.onlnrlbk || (0 == have_crit(CRIT_HAVE_ANY_REG | CRIT_IN_COMMIT)));
		/* Wait for it to be unfrozen before re-grabbing crit on ALL regions */
		WAIT_FOR_REGION_TO_UNFREEZE(csa);
		assert(CDB_STAGNATE > t_tries);
	}	/* for (;;) */
	/* At this point, we are done with validation and so we need to assert that donot_commit is set to FALSE */
	assert(!TREF(donot_commit));	/* We should never commit a transaction that was determined restartable */
	/* Validate the correctness of the calculation of # of replication/journaled regions inside & outside of crit */
	assert(tmp_jnl_participants == jnl_participants);
	assert(cdb_sc_normal == status);
	DEBUG_ONLY(if (!jgbl.forw_phase_recovery) jnl_fence_ctl.token = 0;)
	jnl_fence_ctl.replication = replication;	/* used by "secshr_finish_CMT08_to_CMT14" to finish CMT09 if needed */
	if (replication)
	{
		assert(update_jnlpool);
		jpl = update_jnlpool->jnlpool_ctl;
		assert(jpl);
		assert(update_jnlpool->pool_init);
		if (!repl_csa->hold_onto_crit)
		{
			jnlpool = update_jnlpool;	/* required by grab lock */
			grab_lock(update_jnlpool->jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);	/* Step CMT02 */
		}
		if (update_jnlpool->jnlpool_ctl->freeze)
		{
			status = cdb_sc_instancefreeze;	/* break the possible deadlock by signalling a restart */
			TP_TRACE_HIST(CR_BLKEMPTY, NULL);
			goto failed;
		}
		temp_jnl_seqno = jpl->jnl_seqno;
		jnl_fence_ctl.token = temp_jnl_seqno;
		if (INVALID_SUPPL_STRM != strm_index)
		{	/* Need to also update supplementary stream seqno */
			supplementary = TRUE;
			assert(0 <= strm_index);
			strm_seqno = jpl->strm_seqno[strm_index];
			ASSERT_INST_FILE_HDR_HAS_HISTREC_FOR_STRM(strm_index, update_jnlpool);
			jnl_fence_ctl.strm_seqno = SET_STRM_INDEX(strm_seqno, strm_index);
		} else
		{	/* Note: "supplementary == FALSE" if strm_seqno is 0 is relied upon by "mutex_salvage" */
			assert(!jnl_fence_ctl.strm_seqno);
			supplementary = FALSE;
		}
		assert(jgbl.cumul_jnl_rec_len);
		jgbl.cumul_jnl_rec_len += TCOM_RECLEN * jnl_participants + SIZEOF(jnldata_hdr_struct);
		DEBUG_ONLY(jgbl.cumul_index += jnl_participants;)
		/* Make sure timestamp of this seqno is >= timestamp of previous seqno. Note: The below macro
		 * invocation should be done AFTER the ADJUST_GBL_JREC_TIME call as the below resets
		 * jpl->prev_jnlseqno_time. Doing it the other way around would mean the reset will happen
		 * with a potentially lower value than the final adjusted time written in the jnl record.
		 */
		ADJUST_GBL_JREC_TIME_JNLPOOL(jgbl, jpl);
		/* updates jpl->rsrv_write_addr. Step CMT03 */
		UPDATE_JPL_RSRV_WRITE_ADDR(jpl, update_jnlpool, jgbl.cumul_jnl_rec_len);
		/* Source server does not read in crit. It relies on the transaction data, lastwrite_len,
		 * rsrv_write_addr being updated in that order. To ensure this order, we have to force out
		 * rsrv_write_addr to its coherency point now. If not, the source server may read data that
		 * is overwritten (or stale). This is true only on architectures and OSes that allow unordered
		 * memory access.
		 */
		SHM_WRITE_MEMORY_BARRIER;
	}
	/* There are two possible approaches that can be taken from now onwards.
	 * 	a) Write journal and database records together for a region and move onto the next region.
	 * 	b) Write journal records for all regions and only then move onto writing database updates for all regions.
	 * If journal and database updates are done together region by region, there is a problem in that if an error
	 * occurs after one region's updates are committed (to jnl and db) or if the process gets STOP/IDed in VMS,
	 * secshr_db_clnup should then commit BOTH the journal and database updates of the remaining regions.
	 * committing journal updates is not trivial in secshr_db_clnup since it can also be invoked as a user termination
	 * handler in VMS in which case it cannot do any I/O.
	 *
	 * We therefore take approach (b) below. Write journal records for all regions in one loop. Write database updates
	 * for all regions in another loop. This way if any error occurs before database updates for any region begins in
	 * the second loop, we cleanup the structures as if the transaction is rolled back (there is an exception to this in
	 * that the journal buffers are not rolled back to undo the write of journal records but MUPIP RECOVER knows to handle
	 * such records and TR C9905-001072 exists to make the source-server handle such records). If any error occurs while we
	 * are committing database updates, secshr_db_clnup will be invoked and will complete the updates for this TP transaction.
	 */
	/* the following section writes journal records in all regions */
	DEBUG_ONLY(save_gbl_jrec_time = jgbl.gbl_jrec_time;)
	DEBUG_ONLY(tmp_jnl_participants = 0;)
#	ifdef DEBUG
	/* Check that upd_num in jnl records got set in increasing order (not necessarily contiguous) within each region.
	 * This is true for GT.M and journal recovery. Take the chance to also check that jnl_head & update_trans are in sync.
	 */
	for (si = first_tp_si_by_ftok;  (NULL != si); si = si->next_tp_si_by_ftok)
	{
		prev_upd_num = 0;
		jfb = si->jnl_head;
		/* If we have formatted journal records for this transaction, ensure update_trans is TRUE. Not doing so
		 * would mean we miss out on writing journal records. This might be ok since the database was not seen as
		 * needing any update at all but in tp_clean_up, we will not free up si->jnl_head structures so there might
		 * be a memory leak. In addition, want to know if such a situation happens so assert accordingly.
		 */
		assert((NULL == jfb) || si->update_trans);
		for ( ; NULL != jfb; jfb = jfb->next)
		{
			upd_num = ((struct_jrec_upd *)(jfb->buff))->update_num;
			assert((prev_upd_num < upd_num)
				GTMTRIG_ONLY(|| ((prev_upd_num == upd_num)
						&& IS_ZTWORM(jfb->prev->rectype) && !IS_ZTWORM(jfb->rectype))));
			assert(upd_num);
			prev_upd_num = upd_num;
		}
	}
	/* Check that tp_ztp_jnl_upd_num got set in contiguous increasing order across all regions.
	 * In case of forward processing phase of journal recovery, multi-region TP transactions are
	 * played as multi-region transactions only after resolve-time is reached and that too in
	 * region-by-region order (not necessarily upd_num order across all regions). Until then they
	 * are played as multiple single-region transactions. Also if -fences=none is specified, then
	 * ALL multi-region TP transactions (even those after resolve time) are played as multiple
	 * single-region TP transactions. And if "multi_proc_in_use" is TRUE, ALL multi-region TP
	 * transactions could be played as multiple single-region TP transactions depending on which
	 * regions get assigned to which process. Assert accordingly.
	 */
	max_upd_num = jgbl.tp_ztp_jnl_upd_num;
	if (jgbl.forw_phase_recovery)
		max_upd_num = jgbl.max_tp_ztp_jnl_upd_num;
	if (max_upd_num)
	{
		upd_num_end = 0;
		for (upd_num = 0; upd_num < ARRAYSIZE(upd_num_seen); upd_num++)
			upd_num_seen[upd_num] = FALSE;
		upd_num_seen[0] = TRUE;	/* 0 will never be seen but set it to TRUE to simplify below logic */
		do
		{
			upd_num_start = upd_num_end;
			upd_num_end += ARRAYSIZE(upd_num_seen);
			for (si = first_tp_si_by_ftok;  (NULL != si); si = si->next_tp_si_by_ftok)
			{
				for (jfb = si->jnl_head; NULL != jfb; jfb = jfb->next)
				{
					/* ZTWORMHOLE has same update_num as following SET/KILL record so don't double count */
					if (IS_ZTWORM(jfb->rectype))
						continue;
					upd_num = ((struct_jrec_upd *)(jfb->buff))->update_num;
					if ((upd_num >= upd_num_start) && (upd_num < upd_num_end))
					{
						assert(FALSE == upd_num_seen[upd_num - upd_num_start]);
						upd_num_seen[upd_num - upd_num_start] = TRUE;
					}
					assert(upd_num <= max_upd_num);
				}
			}
			for (upd_num = 0; upd_num < ARRAYSIZE(upd_num_seen); upd_num++)
			{
				if (upd_num <= (max_upd_num - upd_num_start))
				{
					assert((TRUE == upd_num_seen[upd_num])
						|| (jgbl.forw_phase_recovery && ((jgbl.gbl_jrec_time < jgbl.mur_tp_resolve_time)
						|| jgbl.mur_fences_none || multi_proc_in_use)));
					upd_num_seen[upd_num] = FALSE;
				} else
					assert(FALSE == upd_num_seen[upd_num]);
			}
		} while (upd_num_end <= max_upd_num);
	}
#	endif
	replay_jnl_participants = (!jgbl.forw_phase_recovery ? jnl_participants : jgbl.mur_jrec_participants);
	/* In case of journal recovery, token would be initialized to a non-zero value */
	for (si = first_tp_si_by_ftok; (NULL != si); si = si->next_tp_si_by_ftok)
	{
		if (!si->update_trans)
			continue;
		assert((NULL == si->first_cw_set) || (0 != si->cw_set_depth));
		TP_TEND_CHANGE_REG(si);
		csa = cs_addrs;
		csd = cs_data;
		ctn = csd->trans_hist.curr_tn;
		ASSERT_CURR_TN_EQUALS_EARLY_TN(csa, ctn);
		csd->trans_hist.early_tn = ctn + 1;	/* Step CMT04 */
		/* If this process had used "cnl->tp_hint" and done some block allocations in "bm_getfree", now that
		 * all history validation is complete and this transaction is going to commit, take this opportunity
		 * to update the shared memory hint to reflect this process' allocations in this TP transaction.
		 */
		if (csa->tp_hint)
		{
			cnl = csa->nl;
			cnl->tp_hint = csa->tp_hint;	/* update the region hint to reflect any (successful) allocations */
		}
		is_mm = (dba_mm == csd->acc_meth);
		csa->t_commit_crit = T_COMMIT_CRIT_PHASE0;	/* phase0 : write journal records. Step CMT05 */
		if (JNL_ALLOWED(csa))
		{
			jpc = csa->jnl;
			jbp = jpc->jnl_buff;
			jrs = si->jbuf_rsrv_ptr;
			REINIT_JBUF_RSRV_STRUCT(jrs, csa, jpc, jbp);
			/* Write non-logical records (PBLK) if applicable */
			if (JNL_ENABLED(csa))
			{
				/* si->tmp_cw_set_depth is a copy of si->cw_set_depth at TOTAL_TPJNL_REC_SIZE calculation time;
				 * ensure it has not changed until now when the actual jnl record write occurs.
				 * same case with csa->jnl_before_images & jbp->before_images.
				 */
				assert(si->cw_set_depth == si->tmp_cw_set_depth);
				assert(jbp->before_images == csa->jnl_before_image);
				assert(jgbl.gbl_jrec_time >= jbp->prev_jrec_time);
				if (0 == jpc->pini_addr)
					jnl_write_reserve(csa, jrs, JRT_PINI, PINI_RECLEN, NULL);
				if (jbp->before_images)
				{
					epoch_tn = jbp->epoch_tn; /* store in a local variable as it is used in a loop below */
					for (cse = si->first_cw_set;  NULL != cse;  cse = cse->next_cw_set)
					{	/* Write out before-update journal image records */
						TRAVERSE_TO_LATEST_CSE(cse);
						if (WAS_FREE(cse->blk_prior_state))
							continue;
						old_block = (blk_hdr_ptr_t)cse->old_block;
						ASSERT_IS_WITHIN_SHM_BOUNDS((sm_uc_ptr_t)old_block, csa);
						assert((n_gds_t_op != cse->mode) && (gds_t_committed != cse->mode));
						assert(n_gds_t_op < kill_t_create);
						assert(n_gds_t_op < kill_t_write);
						if (n_gds_t_op <= cse->mode)
							continue;
						DEBUG_ONLY(is_mm = (dba_mm == csd->acc_meth));
						DBG_ENSURE_OLD_BLOCK_IS_VALID(cse, is_mm, csa, csd);
						assert(((NULL != old_block) && (old_block->tn < epoch_tn))
											|| (0 == cse->jnl_freeaddr));
						if ((NULL != old_block) && (old_block->tn < epoch_tn))
						{	/* For acquired blocks, we should have computed checksum already.
							 * The only exception is if we found no need to compute checksum
							 * outside of crit but before we got crit, an EPOCH got written
							 * concurrently so we have to write a PBLK (and hence compute the
							 * checksum as well) when earlier we thought none was necessary.
							 * An easy way to check this is that an EPOCH was written AFTER
							 * we started this transaction.
							 */
							assert((gds_t_acquired != cse->mode) || cse->blk_checksum
								|| (epoch_tn >= si->start_tn));
							bsiz = old_block->bsiz;
							assert(bsiz <= csd->blk_size);
							bsiz = MIN(bsiz, csd->blk_size);	/* be safe in PRO */
							bsiz += FIXED_PBLK_RECLEN + JREC_SUFFIX_SIZE;
							bsiz = ROUND_UP2(bsiz, JNL_REC_START_BNDRY);
							jnl_write_reserve(csa, jrs, JRT_PBLK, bsiz, cse);
						}
					}
				}
			}
		}
		/* Write logical journal records if applicable. */
		if (JNL_WRITE_LOGICAL_RECS(csa))
		{
			if (!replication && !jgbl.forw_phase_recovery)
				TOKEN_SET(&jnl_fence_ctl.token, local_tn, process_id);
			/* else : jnl_fence_ctl.token would be pre-filled by journal recovery OR "if (replication)" logic above */
			assert(0 != jnl_fence_ctl.token);
			jfb = si->jnl_head;
			assert(NULL != jfb);
			/* Fill in "num_participants" field in TSET/TKILL/TZKILL/TZTRIG/TZTWORM/TLGTRIG record.
			 * The rest of the records (USET/UKILL/UZKILL/UZTRIG/UZTWORM/ULGTRIG) don't have this initialized.
			 * Recovery looks at this field only in the T* records.
			 */
			rec = (jnl_record *)jfb->buff;
			assert(IS_TUPD(jfb->rectype));
			assert(IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(jfb->rectype));
			assert(&rec->jrec_set_kill.num_participants == &rec->jrec_ztworm.num_participants);
			assert(&rec->jrec_set_kill.num_participants == &rec->jrec_lgtrig.num_participants);
			rec->jrec_set_kill.num_participants = replay_jnl_participants;
			DEBUG_ONLY(++tmp_jnl_participants;)
			do
			{
				jnl_write_reserve(csa, jrs, jfb->rectype, jfb->record_size, jfb);
				jfb = jfb->next;
			} while (NULL != jfb);
		}
	}
	assert(tmp_jnl_participants == jnl_participants);
	/* the next section marks the transaction complete in the journal by writing TCOM record in all regions */
	tcom_record.prefix.time = jgbl.gbl_jrec_time;
	tcom_record.num_participants = replay_jnl_participants;
	assert((JNL_FENCE_LIST_END == jnl_fence_ctl.fence_list) || (0 != jnl_fence_ctl.token));
	tcom_record.token_seq.token = jnl_fence_ctl.token;
	tcom_record.strm_seqno = jnl_fence_ctl.strm_seqno;
	/* tcom_record.jnl_tid was set in op_tstart or updproc */
	/* Note that only those regions that are actively journaling will appear in the following list: */
	DEBUG_ONLY(tmp_jnl_participants = 0;)
	for (csa = jnl_fence_ctl.fence_list;  JNL_FENCE_LIST_END != csa;  csa = csa->next_fenced)
	{
		jpc = csa->jnl;
		DEBUG_ONLY(lcl_update_trans = ((sgm_info *)(csa->sgm_info_ptr))->update_trans;)
		assert(!(lcl_update_trans & ~UPDTRNS_VALID_MASK));
		assert(UPDTRNS_DB_UPDATED_MASK & lcl_update_trans);
		jrs = csa->sgm_info_ptr->jbuf_rsrv_ptr;
		jnl_write_reserve(csa, jrs, JRT_TCOM, TCOM_RECLEN, &tcom_record);
		jbp = jpc->jnl_buff;
		assert(JNL_ALLOWED(csa));
		if (JNL_ENABLED(csa))	/* Need "if" check to handle WAS_ON scenario */
			UPDATE_JRS_RSRV_FREEADDR(csa, jpc, jbp, jrs, jpl, jnl_fence_ctl, replication);/* sets jbp->rsrv_freeaddr.
												       * Step CMT06.
												       */
		assert(jrs->tot_jrec_len);
		if (IS_PHASE2_JNL_COMMIT_NEEDED_IN_CRIT(jbp, jrs->tot_jrec_len) || (dba_mm == csa->hdr->acc_meth))
			FINISH_JNL_PHASE2_IN_JNLBUFF(csa, jrs);	/* Step CMT06a */
		DEBUG_ONLY(tmp_jnl_participants++;)
	}
	if (replication)
		SET_JNL_SEQNO(jpl, temp_jnl_seqno, supplementary, strm_seqno, strm_index, next_strm_seqno);	/* Step CMT07 */
	assert(jnl_participants == tmp_jnl_participants);
	/* Ensure jgbl.gbl_jrec_time did not get reset by any of the jnl writing functions */
	assert(save_gbl_jrec_time == jgbl.gbl_jrec_time);
	/* the following section is the actual commitment of the changes in the database (phase1 for BG) */
	for (si = first_tp_si_by_ftok;  (NULL != si); si = si->next_tp_si_by_ftok)
	{
		if (lcl_update_trans = si->update_trans)
		{
			assert((NULL == si->first_cw_set) || (0 != si->cw_set_depth));
			sgm_info_ptr = si;
			TP_TEND_CHANGE_REG(si);
			csa = cs_addrs;
			csd = cs_data;
			cnl = csa->nl;
			is_mm = (dba_mm == csd->acc_meth);
			ctn = csd->trans_hist.curr_tn;
			assert((ctn + 1) == csd->trans_hist.early_tn);
			csa->prev_free_blks = csd->trans_hist.free_blocks;
			SET_T_COMMIT_CRIT_PHASE1(csa, cnl, ctn); /* Step CMT08 */
			if (REPL_ALLOWED(csa) && is_replicator)
			{
				assert(NULL != csa->next_fenced);
				SET_REG_SEQNO(csa, temp_jnl_seqno, supplementary, strm_index, next_strm_seqno,
											SKIP_ASSERT_FALSE); /* Step CMT09 */
			}
			if (csd->dsid && tp_kill_bitmaps)
				rc_cpt_inval();
			cse = si->first_cw_set;
			if (NULL != cse)
			{
				if (!is_mm)	/* increment counter of # of processes that are actively doing two-phase commit */
					INCR_WCS_PHASE2_COMMIT_PIDCNT(csa, cnl);
#				ifdef DEBUG
				/* Assert that cse->old_mode, if uninitialized, never contains a negative value
				 * (this is relied upon by secshr_db_clnup)
				 */
				do
				{
					TRAVERSE_TO_LATEST_CSE(cse);
					assert(0 <= cse->old_mode);
					cse = cse->next_cw_set;
				} while (NULL != cse);
				cse = si->first_cw_set;
#				endif
				do
				{
					TRAVERSE_TO_LATEST_CSE(cse);
					mode = cse->mode;
					assert((n_gds_t_op != mode) && (gds_t_committed != mode));
					assert(n_gds_t_op < kill_t_create);
					assert(n_gds_t_op < kill_t_write);
					assert(gds_t_committed < gds_t_write_root);
					assert(gds_t_committed < gds_t_busy2free);
					assert(gds_t_write_root < n_gds_t_op);
					assert(gds_t_busy2free < n_gds_t_op);
					assert(gds_t_write_root != mode);
					assert(gds_t_busy2free != mode);
					cse->old_mode = (int4)mode;	/* note down before being reset to gds_t_committed */
					if (n_gds_t_op > mode)
					{
						DEBUG_ONLY(bml_status_check(cse));
						if (csd->dsid && !tp_kill_bitmaps && (0 == cse->level))
						{
							assert(!is_mm);
							rc_cpt_entry(cse->blk);
						}
						/* Do phase1 of bg_update while holding crit on the database.
						 * This will lock the buffers that need to be changed.
						 * Once crit is released, invoke phase2 which will update those locked buffers.
						 * There are two exceptions.
						 * 1) If it is a bitmap block. In that case we also do phase2
						 * while holding crit so the next process to use this bitmap will see a
						 * consistent copy of this bitmap when it gets crit for commit. This avoids
						 * the reallocate_bitmap routine from restarting or having to wait for a
						 * concurrent phase2 construction to finish. When the change request C9E11-002651
						 * (to reduce restarts due to bitmap collisions) is addressed, we can reexamine
						 * whether it makes sense to move bitmap block builds back to phase2.
						 * 2) If the block has a recompute update array. This means it is a global that
						 * has NOISOLATION turned on. In this case, we have seen that deferring the
						 * updates to phase2 can cause lots of restarts in the "recompute_upd_array"
						 * function (where cr->in_tend check fails) in a highly contentious environment.
						 * Hence build such blocks in phase1 while holding crit and avoid such restarts.
						 */
						if (is_mm)
							status = mm_update(cse, ctn, ctn, si);	/* Step CMT10 */
						else
						{
							status = bg_update_phase1(cse, ctn, si);	/* Step CMT10 */
							if ((cdb_sc_normal == status) && IS_BG_PHASE2_COMMIT_IN_CRIT(cse, mode))
							{	/* If we are about to do phase2 db commit while holding crit,
								 * then check if jnl phase2 commit is pending on this region.
								 * If so do it before any db phase2 commit happens in this region.
								 */
								jrs = si->jbuf_rsrv_ptr;
								if (NEED_TO_FINISH_JNL_PHASE2(jrs))
									FINISH_JNL_PHASE2_IN_JNLBUFF(csa, jrs);	/* Step CMT10a */
								status = bg_update_phase2(cse, ctn, ctn, si);	/* Step CMT10a */
								if (cdb_sc_normal == status)
									cse->mode = gds_t_committed;
							}
						}
						if (cdb_sc_normal != status)
						{	/* the database is probably in trouble */
							TP_TRACE_HIST(cse->blk, cse->blk_target);
							INVOKE_T_COMMIT_CLEANUP(status, csa);
							assert(cdb_sc_normal == status);
							/* At this time "si->cr_array_index" could be non-zero for one or more
							 * regions and a few cache-records might have their "in_cw_set" field set
							 * to TRUE. We should not reset "in_cw_set" as we don't hold crit at this
							 * point and also because we might still need those buffers pinned until
							 * their before-images are backed up in wcs_recover (in case an online
							 * backup was running while secshr_db_clnup did its job). The variable
							 * "si->cr_array_index" is reset to 0 by secshr_db_clnup.
							 */
							assert(0 == si->cr_array_index);
							goto skip_failed; /* do not do "failed:" processing as we don't hold crit */
						}
					} else
					{
						if (!cse->done)
						{	/* This block is needed in the 2nd-phase of KILL. Build a private
							 * copy right now while we hold crit and the update array points
							 * to validated buffer contents.
							 */
							gvcst_blk_build(cse, (uchar_ptr_t)cse->new_buff, ctn);
							cse->done = TRUE;
							assert(NULL != cse->blk_target);
							CERT_BLK_IF_NEEDED(certify_all_blocks, gv_cur_region,
											cse, cse->new_buff, gv_target);
						}
						cse->mode = gds_t_committed;
					}
					cse = cse->next_cw_set;
				} while (NULL != cse);
			}
			/* signal secshr_db_clnup/t_commit_cleanup, roll-back is no longer possible */
			assert(!(lcl_update_trans & ~UPDTRNS_VALID_MASK));
			assert(!(UPDTRNS_TCOMMIT_STARTED_MASK & lcl_update_trans));
			si->update_trans = lcl_update_trans | UPDTRNS_TCOMMIT_STARTED_MASK; /* Step CMT11 */
			/* should never increment curr_tn on a frozen database */
			assert(!(FROZEN_HARD(csa) || (replication && IS_REPL_INST_FROZEN_JPL(update_jnlpool))));
			/* For MM, barrier ensures blocks updates complete before incrementing db TN. Otherwise concurrent
			 * processes could note a premature db TN value in gvcst_search and later fail to detect a block
			 * modification.
			 * For BG, no barrier is needed. We increment db TN only after pinning the relevant buffers.
			 * Concurrent processes wait for the buffer to be unpinned before accessing it and so never mistake
			 * the old contents for the new contents. The acts of pinning and unpinning use compswap which does the
			 * needed memory barriers.
			 */
			if (is_mm)
				MM_WRITE_MEMORY_BARRIER;
			INCREMENT_CURR_TN(csd); /* Step CMT12 */
			csa->t_commit_crit = T_COMMIT_CRIT_PHASE2;	/* phase2 : update database buffers. Step CMT13.
									 * Set this BEFORE releasing crit but AFTER
									 * incrementing curr_tn.
									 */
#			ifdef GTM_TRIGGER
			if (csa->incr_db_trigger_cycle)
			{
				csd->db_trigger_cycle++;
				if (0 == csd->db_trigger_cycle)
					csd->db_trigger_cycle = 1;	/* Don't allow cycle set to 0 which means uninitialized */
				/* Update the process private view of trigger cycle also since we are the ones
				 * who updated csd->db_trigger_cycle so we can safely keep csa in sync as well.
				 * Not doing this would cause an unnecessary cdb_sc_triggermod restart for the
				 * next transaction. In fact this restart will create an out-of-design situation
				 * for recovery (which operates in the final-retry) and cause an unnecessary
				 * replication pipe drain for the update process (a costly operation). So it
				 * is in fact a necessary step (considering recovery).
				 */
				csa->db_trigger_cycle = csd->db_trigger_cycle;
				csa->incr_db_trigger_cycle = FALSE;
			}
#			endif
			/* If db is journaled, then db header is flushed periodically when writing the EPOCH record,
			 * otherwise do it here every HEADER_UPDATE_COUNT transactions.
			 */
			if (!JNL_ENABLED(csa) && !(csd->trans_hist.curr_tn & (HEADER_UPDATE_COUNT - 1)) && !FROZEN_CHILLED(csa))
				fileheader_sync(gv_cur_region);
			if (NULL != si->kill_set_head)
				INCR_KIP(csd, csa, si->kip_csa);
		} else
			ctn = si->tp_csd->trans_hist.curr_tn;
		si->start_tn = ctn; /* start_tn used temporarily to store currtn (for bg_update_phase2) before releasing crit */
		if (!si->tp_csa->hold_onto_crit)
		{	/* Note: Should use si->gv_cur_region (not gv_cur_region) as the latter is not set
			 * in case we are not updating this region as part of this TP transaction.
			 */
			rel_crit(si->gv_cur_region);	/* Step CMT14 */
		}
	} /* for (si ... ) */
	assert(cdb_sc_normal == status);
	/* Now that all buffers needed for commit are locked in shared memory (in phase1 for BG), it is safe to
	 * release the jnlpool lock. Releasing it before this could cause an instance freeze to sneak in while
	 * the phase1 is still midway causing trouble for this transaction in case it needs to do any db/jnl writes.
	 */
	if (replication)
	{
		assert(update_jnlpool);
		assert(update_jnlpool->jnlpool_ctl == jpl);
		assert(NULL != repl_csa);
		assert(update_jnlpool && update_jnlpool->pool_init);
		assert(repl_csa->now_crit);
		if (!repl_csa->hold_onto_crit)
			rel_lock(update_jnlpool->jnlpool_dummy_reg);	/* Step CMT15 */
	}
	/* Check that we DON'T own crit on ANY region. The only exception is online mupip journal rollback/recovery
	 * or MUPIP TRIGGER -UPGRADE which holds crit for the entire process lifetime. */
	assert(jgbl.onlnrlbk || TREF(in_trigger_upgrade) || (0 == have_crit(CRIT_HAVE_ANY_REG)));
	/* the following section is the actual commitment of the journal records */
	TP_FINISH_JNL_PHASE2_IN_JNLBUFF_AND_JNLPOOL(jnl_fence_ctl, replication, update_jnlpool);	/* Step CMT16 & CMT17 */
	/* The following section is the actual commitment of the changes in the database (phase2 for BG) */
	for (si = first_tp_si_by_ftok;  (NULL != si); si = si->next_tp_si_by_ftok)
	{
		cse = si->first_cw_set;
		if (NULL != cse)
		{
			sgm_info_ptr = si;
			TP_TEND_CHANGE_REG(si);
			ctn = si->start_tn;
			is_mm = (dba_mm == cs_data->acc_meth);
			/* If BG, check that we have not pinned any more buffers than we are updating */
			DBG_CHECK_PINNED_CR_ARRAY_CONTENTS(si->tp_csd, is_mm, si->cr_array, si->cr_array_index);
			do
			{
				TRAVERSE_TO_LATEST_CSE(cse);
				if (gds_t_committed > cse->mode)
				{	/* Finish 2nd phase of commit for BG (updating the buffers locked in phase1) now that CRIT
					 * has been released. For MM, only thing needed is to set cse->mode to gds_t_committed.
					 */
					if (!is_mm)
					{	/* Validate old_mode noted down in first phase is the same as the current mode.
						 * Note that cse->old_mode is negated by bg_update_phase1 (to help secshr_db_clnup).
						 */
						assert(-cse->old_mode == (int4)cse->mode);
						status = bg_update_phase2(cse, ctn, ctn, si);	/* Step CMT18 */
						if (cdb_sc_normal != status)
						{	/* the database is probably in trouble */
							TP_TRACE_HIST(cse->blk, cse->blk_target);
							INVOKE_T_COMMIT_CLEANUP(status, si->tp_csa);
							assert(cdb_sc_normal == status);
							/* At this time "si->cr_array_index" could be non-zero for one or more
							 * regions and a few cache-records might have their "in_cw_set" field set
							 * to TRUE. We should not reset "in_cw_set" as we don't hold crit at this
							 * point and also because we might still need those buffers pinned until
							 * their before-images are backed up in wcs_recover (in case an online
							 * backup was running while secshr_db_clnup did its job). The local
							 * variable "si->cr_array_index" is reset to 0 by secshr_db_clnup.
							 */
							assert(0 == si->cr_array_index);
							/* Note that seshr_db_clnup (invoked by t_commit_cleanup above) would have
							 * done a lot of cleanup for us including decrementing the counter
							 * "wcs_phase2_commit_pidcnt" so it is ok to skip all that processing
							 * below and go directly to skip_failed.
							 */
							goto skip_failed; /* do not do "failed:" processing as we don't hold crit */
						}
					}
					cse->mode = gds_t_committed;
				} else
				{	/* blk build should have been completed in phase1 for kill_t_* modes */
					assert((n_gds_t_op > cse->mode) || cse->done);
					assert(gds_t_committed == cse->mode);
				}
				cse = cse->next_cw_set;
			} while (NULL != cse);
			/* Free up all pinnned cache-records */
			tp_cr_array = si->cr_array;
			ASSERT_CR_ARRAY_IS_UNPINNED(si->tp_csd, tp_cr_array, si->cr_array_index);
			si->cr_array_index = 0;
			if (!is_mm)
			{	/* In BG, now that two-phase commit is done, decrement counter */
				csa = cs_addrs;
				cnl = csa->nl;
				DECR_WCS_PHASE2_COMMIT_PIDCNT(csa, cnl);
				/* Phase 2 commits are completed for the current region. See if we had done a snapshot
				 * init (csa->snapshot_in_prog == TRUE). If so, try releasing the resources obtained
				 * while snapshot init.
				 */
				if (SNAPSHOTS_IN_PROG(csa))
				{
					assert(NULL != si->first_cw_set);
					SS_RELEASE_IF_NEEDED(csa, cnl);
				}
			}
		}
		assert(!si->cr_array_index);
		si->tp_csa->t_commit_crit = FALSE;	/* Step CMT19 */
	}
	si_not_validated = NULL;	/* all "si" have been validated at this point */
	/* Caution: followthrough, cleanup for normal and abnormal "status" */
failed:
	if (cdb_sc_normal != status)
	{
		si_not_validated = si;
		si_last = (NULL == si_not_validated) ? NULL : si_not_validated->next_tp_si_by_ftok;
		/* Free up all pinnned cache-records and release crit */
		release_crit = (NEED_TO_RELEASE_CRIT(t_tries, status) && !jgbl.onlnrlbk);
		for (si = first_tp_si_by_ftok; si_last != si; si = si->next_tp_si_by_ftok)
		{
			assert(si->tp_csa->now_crit);
			tp_cr_array = si->cr_array;
			UNPIN_CR_ARRAY_ON_RETRY(tp_cr_array, si->cr_array_index);
			assert(!si->cr_array_index);
			si->start_tn = si->tp_csd->trans_hist.curr_tn;	/* start_tn used temporarily to store currtn
									 * before releasing crit */
			if (release_crit)
			{
				assert(!si->tp_csa->hold_onto_crit);
				rel_crit(si->gv_cur_region);
			}
		}
		if (replication && repl_csa->now_crit && release_crit)
		{	/* The only restart that is possible once we acquired the journal pool lock is due to instance freeze */
			assert(cdb_sc_instancefreeze == status);
			assert(update_jnlpool && update_jnlpool->pool_init);
			rel_lock(update_jnlpool->jnlpool_dummy_reg);
		}
		/* Check that we DON'T own crit/commit on ANY region. The only exception is online mupip journal rollback/recovery
		 * which holds crit for the entire process lifetime.
		 */
		assert(jgbl.onlnrlbk || !release_crit || (0 == have_crit(CRIT_HAVE_ANY_REG | CRIT_IN_COMMIT)));
	}
	/* We have finished validation on this region. Reset transaction numbers in the gv_target
	 * histories so they will be valid for a future access utilizing the clue field. This occurs
	 * to improve performance (of next tn in case of commit of current tn) or the chances of commit
	 * (of current tn in case of a restart/retry).
	 */
	for (si = first_tp_si_by_ftok;  (si_not_validated != si);  si = si->next_tp_si_by_ftok)
	{
		if ((cdb_sc_normal == status) && (si->update_trans))
			valid_thru = si->start_tn + 1;	/* curr_tn of db AFTER incrementing it but before releasing crit */
		else
			valid_thru = si->start_tn;
		assert(valid_thru <= si->tp_csd->trans_hist.curr_tn);
		is_mm = (dba_mm == si->tp_csd->acc_meth);
		bmp_begin_cse = si->first_cw_bitmap;
		prev_target = NULL;
		for (cse = si->first_cw_set; bmp_begin_cse != cse; cse = cse->next_cw_set)
		{
			TRAVERSE_TO_LATEST_CSE(cse);
			curr_target = cse->blk_target;
			/* Avoid redundant updates to gv_target's history using a simplistic scheme (check previous iteration) */
			if ((prev_target != curr_target) && (0 != curr_target->clue.end))
			{
				prev_target = curr_target;
				for (t1 = curr_target->hist.h; t1->blk_num; t1++)
				{	/* If MM, the history can be safely updated. In BG, phase2 of commit happens outside of
					 * crit. So we need to check if the global buffer corresponding to this block is
					 * in the process of being updated concurrently by another process. If so, we have no
					 * guarantee that the concurrent update started AFTER valid_thru db-tn so we cannot safely
					 * reset t1->tn in this case. If no update is in progress, we can safely update our history
					 * to reflect the fact that all updates to this block before the current transaction number
					 * are complete as of this point. Note that it is ok to do the cr->in_tend check outside
					 * of crit. If this update started after we released crit, t1->tn will still be lesser than
					 * the transaction at which this update occurred so a cdb_sc_blkmod check is guaranteed to
					 * be signalled. If this update started and ended after we released crit but before we
					 * reached here, it is ok to set t1->tn to valid_thru as the concurrent update corresponds
					 * to a higher transaction number and will still fail the cdb_sc_blkmod check in the next
					 * validation. Also note that because of this selective updation of t1->tn, it is possible
					 * that for a given gv_target->hist, hist[0].tn is not guaranteed to be GREATER than
					 * hist[1].tn. Therefore t_begin has to now determine the minimum and use that as the
					 * start_tn instead of looking at hist[MAXDEPTH].tn and using that.
					 */
					cr = !is_mm ? t1->cr : NULL;
					in_tend = (NULL != cr) ? cr->in_tend : 0;
					assert(process_id != in_tend);
					if (!in_tend)
						t1->tn = valid_thru;
				}
			}
		}
	}
skip_failed:
	REVERT;
	DEFERRED_EXIT_HANDLING_CHECK; /* now that all crits are released, check if deferred signal/exit handling needs to be done */
	if (cdb_sc_normal == status)
	{
		if (save_jnlpool != jnlpool)
			jnlpool = save_jnlpool;
		first_tp_si_by_ftok = NULL; /* Signal t_commit_cleanup/secshr_db_clnup that TP transaction is NOT underway */
		return TRUE;
	}
failed_skip_revert:
	assert(cdb_sc_normal != status);
	t_fail_hist[t_tries] = status;
	SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, cnl, status);
	TP_RETRY_ACCOUNTING(csa, cnl);
	first_tp_si_by_ftok = NULL;	/* Signal t_commit_cleanup/secshr_db_clnup that TP transaction is NOT underway */
	if (save_jnlpool != jnlpool)
		jnlpool = save_jnlpool;
	return FALSE;
}

/* --------------------------------------------------------------------------------------------
 * This code is very similar to the code in gvcst_put for the non-block-split case. Any changes
 * in either place should be reflected in the other.
 * --------------------------------------------------------------------------------------------
 */
enum cdb_sc	recompute_upd_array(srch_blk_status *bh, cw_set_element *cse)
{
	blk_segment		*bs1, *bs_ptr;
	boolean_t		new_rec;
	cache_rec_ptr_t		cr;
	char			*va;
	enum cdb_sc		status;
	gv_key			*pKey = NULL;
	int4			blk_size, blk_fill_size, cur_blk_size, blk_seg_cnt, delta ;
	int4                    n, new_rec_size, next_rec_shrink;
	int4			rec_cmpc, target_key_size;
	int			tmp_cmpc;
	uint4			segment_update_array_size;
	key_cum_value		*kv, *kvhead;
	mstr			value;
	off_chain		chain1;
	rec_hdr_ptr_t		curr_rec_hdr, next_rec_hdr, rp;
	sm_uc_ptr_t		cp1, buffaddr;
	unsigned short		rec_size;
	unsigned int		bsiz;
	sgmnt_addrs		*csa;
	blk_hdr_ptr_t		old_block;
	gv_namehead		*gvt;
	srch_blk_status		*t1;
#	ifdef DEBUG
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	csa = cs_addrs;
	BG_TRACE_PRO_ANY(csa, recompute_upd_array_calls);
	assert(csa->now_crit && dollar_tlevel && sgm_info_ptr);
	assert(!cse->level && cse->blk_target && !cse->first_off && !cse->write_type);
	blk_size = cs_data->blk_size;
	blk_fill_size = (blk_size * gv_fillfactor) / 100 - cs_data->reserved_bytes;
	cse->first_copy = TRUE;
	if (dba_bg == csa->hdr->acc_meth)
	{	/* For BG method, modify history with uptodate cache-record, buffer and cycle information.
		 * Also modify cse->old_block and cse->ondsk_blkver to reflect the updated buffer.
		 * This is necessary in case history contains an older twin cr or a cr which has since been recycled
		 */
		cr = db_csh_get(bh->blk_num);
		assert(CR_NOTVALID != (sm_long_t)cr);
		if (NULL == cr || CR_NOTVALID == (sm_long_t)cr || 0 <= cr->read_in_progress)
		{
			BG_TRACE_PRO_ANY(csa, recompute_upd_array_rip);
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_lostcr;
		}
		if (cr->in_tend)
		{	/* Possible if this cache-record is being modified concurrently by another process in bg_update_phase2.
			 * In this case, we cannot determine if recomputation is possible. Have to restart.
			 */
			assert(CDB_STAGNATE > t_tries);
			BG_TRACE_PRO_ANY(csa, recompute_upd_array_in_tend);
			return cdb_sc_blkmod;
		}
		bh->cr = cr;
		bh->cycle = cr->cycle;
		cse->old_block = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
		cse->ondsk_blkver = cr->ondsk_blkver;
		/* old_block needs to be repointed to the NEW buffer but the fact that this block was free does not change in this
		 * entire function. So cse->blk_prior_state's free_status can stay as it is.
		 */
		bh->buffaddr = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
	}
	buffaddr = bh->buffaddr;
	assert(NULL != cse->recompute_list_head);
	for (kvhead = kv = cse->recompute_list_head; (NULL != kv); kv = kv->next)
	{
		pKey = &kv->key;
		value = kv->value;
		target_key_size = pKey->end + 1;
		if (kvhead != kv)
		{
			assert(FALSE == cse->done);
			assert(0 == cse->reference_cnt);
			assert(0 == cse->ins_off);		/* because leaf-level block */
			assert(0 == cse->level);
			assert(0 == cse->index);
			assert(FALSE == cse->forward_process);	/* because no kills should have taken place in this block */
			gvcst_blk_build(cse, (uchar_ptr_t)cse->new_buff, 0);
			bh->buffaddr = buffaddr = cse->new_buff;
		}
		if (cdb_sc_normal != (status = gvcst_search_blk(pKey, bh)))
		{
			BG_TRACE_PRO_ANY(csa, recompute_upd_array_search_blk);
			return status;
		}
		cur_blk_size = ((blk_hdr_ptr_t)buffaddr)->bsiz;
		new_rec = (target_key_size != bh->curr_rec.match);
		rp = (rec_hdr_ptr_t)(buffaddr + bh->curr_rec.offset);
		if (bh->curr_rec.offset == cur_blk_size)
		{
			if (FALSE == new_rec)
			{
				assert(CDB_STAGNATE > t_tries);
				BG_TRACE_PRO_ANY(csa, recompute_upd_array_new_rec);
				return cdb_sc_mkblk;
			}
			rec_cmpc = 0;
			rec_size = 0;
		} else
		{
			GET_USHORT(rec_size, &rp->rsiz);
			rec_cmpc = EVAL_CMPC(rp);
			if ((sm_uc_ptr_t)rp + rec_size > (sm_uc_ptr_t)buffaddr + cur_blk_size)
			{
				assert(CDB_STAGNATE > t_tries);
				BG_TRACE_PRO_ANY(csa, recompute_upd_array_rec_size);
				return cdb_sc_mkblk;
			}
		}
		if (new_rec)
		{
			new_rec_size = SIZEOF(rec_hdr) + target_key_size - bh->prev_rec.match + value.len;
			if (cur_blk_size <= (int)bh->curr_rec.offset)
				next_rec_shrink = 0;
			else
				next_rec_shrink = bh->curr_rec.match - rec_cmpc;
			delta = new_rec_size - next_rec_shrink;
		} else
		{
			if (rec_cmpc != bh->prev_rec.match)
			{
				assert(CDB_STAGNATE > t_tries);
				BG_TRACE_PRO_ANY(csa, recompute_upd_array_rec_cmpc);
				return cdb_sc_mkblk;
			}
			new_rec_size = SIZEOF(rec_hdr) + (target_key_size - rec_cmpc) + value.len;
			delta = new_rec_size - rec_size;
			next_rec_shrink = 0;
		}
		chain1 = *(off_chain *)&bh->blk_num;
		assert(0 == chain1.flag);
		if (cur_blk_size + delta <= blk_fill_size)
		{
			segment_update_array_size = UA_NON_BM_SIZE(cs_data);
			ENSURE_UPDATE_ARRAY_SPACE(segment_update_array_size);
			BLK_INIT(bs_ptr, bs1);
			assertpro(0 == rc_set_fragment);
			BLK_SEG(bs_ptr, buffaddr + SIZEOF(blk_hdr), bh->curr_rec.offset - SIZEOF(blk_hdr));
			BLK_ADDR(curr_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
			curr_rec_hdr->rsiz = new_rec_size;
			SET_CMPC(curr_rec_hdr, bh->prev_rec.match);
			BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, SIZEOF(rec_hdr));
			BLK_ADDR(cp1, target_key_size - bh->prev_rec.match, unsigned char);
			memcpy(cp1, pKey->base + bh->prev_rec.match, target_key_size - bh->prev_rec.match);
			BLK_SEG(bs_ptr, cp1, target_key_size - bh->prev_rec.match);
			if (0 != value.len)
			{
				BLK_ADDR(va, value.len, char);
				memcpy(va, value.addr, value.len);
				BLK_SEG(bs_ptr, (unsigned char *)va, value.len);
			}
			if (!new_rec)
				rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + rec_size);
			n = (int)(cur_blk_size - ((sm_uc_ptr_t)rp - buffaddr));
			if (n > 0)
			{
				if (new_rec)
				{
					BLK_ADDR(next_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
					next_rec_hdr->rsiz = rec_size - next_rec_shrink;
					SET_CMPC(next_rec_hdr, bh->curr_rec.match);
					BLK_SEG(bs_ptr, (sm_uc_ptr_t)next_rec_hdr, SIZEOF(rec_hdr));
					next_rec_shrink += SIZEOF(rec_hdr);
				}
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)rp + next_rec_shrink, n - next_rec_shrink);
			}
			if (0 == BLK_FINI(bs_ptr, bs1))
			{
				assert(CDB_STAGNATE > t_tries);
				BG_TRACE_PRO_ANY(csa, recompute_upd_array_blk_fini);
				return cdb_sc_mkblk;
			}
			cse->upd_addr = (unsigned char *)bs1;
			cse->done = FALSE;
		} else
		{
			BG_TRACE_PRO_ANY(csa, recompute_upd_array_blk_split);
			return cdb_sc_blksplit;
		}
	}
	/* Update bh->tn to reflect the fact that it is uptodate as of the current database transaction.
	 * Not doing so could actually cause unnecessary restarts.
	 */
	bh->tn = csa->hdr->trans_hist.curr_tn;
	/* If block in this history element is the same as gv_target's leaf block and it has a non-zero clue, update it */
	gvt = bh->blk_target;
	assert(!bh->level);	/* this is why it is safe to access 0th array index in the next line */
	t1 = gvt->hist.h;
	if (gvt->clue.end && (t1->blk_num == bh->blk_num))
	{
		*t1 = *bh;
		/* Update clue to reflect last key in recompute list. No need to update gvt->first_rec and gvt->last_rec
		 * as they are guaranteed to be the same as what it was when the clue was filled in by gvcst_search (if
		 * they are different, an index block would have changed which means we would restart this transaction
		 * anyways and the clue would be reset to 0).
		 */
		assert(NULL != pKey);
		assert(!TREF(expand_prev_key));	/* this ensures it is safe to use EXPAND_PREV_KEY_FALSE below */
		COPY_CURR_AND_PREV_KEY_TO_GVTARGET_CLUE(gvt, pKey, EXPAND_PREV_KEY_FALSE);
		if (new_rec)
			t1->curr_rec.match = gvt->clue.end + 1;	/* Keep srch_hist and clue in sync for NEXT gvcst_search */
		/* Now that the clue is known to be non-zero, we have the potential for the first_rec part of it to be
		 * unreliable. Reset it to be safe. See comment in similar section in tp_hist for details on why.
		 */
		GVT_CLUE_INVALIDATE_FIRST_REC(gvt);
	}
	/* At this point, cse->new_buff could be non-NULL either because the same variable was being updated multiple times
	 * inside of the TP transaction or because cse->recompute_list_head contained more than one variable (in which case
	 * cse->new_buff will be set by the invocation of gvcst_blk_build (above) for the second element in the list. In
	 * either case, the final update-array contents rely on the shared memory buffer (in case of BG access method) and
	 * not on cse->new_buff. Therefore we need to PIN the corresponding cache-record in tp_tend. So reset cse->new_buff.
	 */
	cse->new_buff = NULL;
	if (!WAS_FREE(cse->blk_prior_state) && (NULL != cse->old_block) && JNL_ENABLED(csa) && csa->jnl_before_image)
	{
		old_block = (blk_hdr_ptr_t)cse->old_block;
		if (old_block->tn < csa->jnl->jnl_buff->epoch_tn)
		{
			bsiz = old_block->bsiz;
			/* See comment before similar check in "gvincr_recompute_upd_array" for why this check is needed */
			if (bsiz > csa->hdr->blk_size)
			{	/* This is a restartable condition. Restart */
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_mkblk;
			}
			cse->blk_checksum = jnl_get_checksum(old_block, csa, bsiz);
		} else
			cse->blk_checksum = 0;
	}
	return cdb_sc_normal;
}

/* This function does not update "bml_cse->tn" (to reflect that the reallocation is valid as of the current database tn).
 * See similar comment before the function definition of "recompute_upd_array". For the same reasons, it is considered
 * ok to do the reallocation since frozen regions are considered relatively rare.
 */
enum cdb_sc	reallocate_bitmap(sgm_info *si, cw_set_element *bml_cse)
{
	blk_hdr_ptr_t		old_block;
	block_id		bml, free_bit;
	block_id_ptr_t		b_ptr;
	boolean_t		before_image_needed, blk_used, is_mm;
	boolean_t		read_before_image;		/* TRUE if before-image journaling or online backup in progress */
	cache_rec_ptr_t		cr;
	cw_set_element		*cse, *bmp_begin_cse;
	int4			offset;
	jnl_buffer_ptr_t	jbp;				/* jbp is non-NULL only if before-image journaling */
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	uint4			total_blks, map_size;
	unsigned int		bsiz;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = cs_addrs;
	csd = csa->hdr;
	is_mm = (dba_mm == csd->acc_meth);
	/* This optimization should only be used if blocks are being allocated (not if freed) in this bitmap. */
	assert(0 <= bml_cse->reference_cnt);
	bml = bml_cse->blk;
	if (!is_mm && bml_cse->cr->in_tend)
	{	/* Possible if this cache-record no longer contains the bitmap block we think it does. In this case restart.
		 * Since we hold crit at this point, the block that currently resides should not be a bitmap block since
		 * all updates to the bitmap (both phase1 and phase) happen inside of crit.
		 */
		assert(csa->now_crit && (bml != bml_cse->cr->blk) && (bml_cse->cr->blk % csd->bplmap));
		return cdb_sc_lostbmlcr;
	}
	assert(is_mm || (FALSE == bml_cse->cr->in_tend));
	assert(is_mm || (FALSE == bml_cse->cr->data_invalid));
	offset = 0;
	total_blks = is_mm ? csa->total_blks : csa->ti->total_blks;
	map_size = (ROUND_DOWN2(total_blks, BLKS_PER_LMAP) == bml) ? total_blks - bml : BLKS_PER_LMAP;
	assert(bml >= 0 && bml < total_blks);
	bmp_begin_cse = si->first_cw_bitmap;	/* stored in a local to avoid pointer de-referencing within the loop below */
	jbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
	read_before_image = ((NULL != jbp) || csa->backup_in_prog || SNAPSHOTS_IN_PROG(csa));
	b_ptr = (block_id_ptr_t)bml_cse->upd_addr;
	for (cse = si->first_cw_set;  cse != bmp_begin_cse; cse = cse->next_cw_set)
	{
		TRAVERSE_TO_LATEST_CSE(cse);
		if ((gds_t_acquired != cse->mode) || (ROUND_DOWN2(cse->blk, BLKS_PER_LMAP) != bml))
			continue;
		assert(gds_t_acquired == cse->mode);
		assert(GDSVCURR == cse->ondsk_blkver);
		assert(*b_ptr == (cse->blk - bml));
		do
		{
			/* If "bm_find_blk" is passed a hint (first arg) it assumes it is less than map_size and gives invalid
			 * results (for values >= map_size). Instead of changing "bm_find_blk" we do the check here and assert
			 * that "hint" < "map_size" in "bm_find_blk".
			 */
			if (offset >= map_size)
				return cdb_sc_bmlmod;
			free_bit = bm_find_blk(offset, (sm_uc_ptr_t)bml_cse->old_block + SIZEOF(blk_hdr), map_size, &blk_used);
			if (NO_FREE_SPACE == free_bit)
				return cdb_sc_bmlmod;
			cse->blk = bml + free_bit;
			if (cse->blk >= total_blks)
				return cdb_sc_lostbmlcr;
			/* Re-point before-images into cse->old_block if necessary; if not available: restart.
			 * Set cse->blk_prior_state before invoking BEFORE_IMAGE_NEEDED macro (as it needs this field set).
			 */
			if (blk_used)
				BIT_SET_RECYCLED_AND_CLEAR_FREE(cse->blk_prior_state);
			else
				BIT_CLEAR_RECYCLED_AND_SET_FREE(cse->blk_prior_state);
			BEFORE_IMAGE_NEEDED(read_before_image, cse, csa, csd, cse->blk, before_image_needed);
			if (!before_image_needed)
			{
				cse->old_block = NULL;
				cse->blk_checksum = 0;
			} else if (!is_mm)
			{
				cr = db_csh_get(cse->blk);			/* see if the before image is in memory and OK */
				assert(CR_NOTVALID != (sm_long_t)cr);
				if ((NULL == cr) || (CR_NOTVALID == (sm_long_t)cr) || (0 <= cr->read_in_progress))
				{	/* if this before image is not at hand don't wait for it in crit */
					offset = free_bit + 1;		/* try further in this bitmap */
					continue;
				}
				/* if we had not read a before-image previously (because cse->blk was not a reused block previously)
				 * OR if old cse->cr and cse->cycle don't match current cr, reset cse->cr, cycle, old_block and
				 * checksums
				 */
				assert((NULL == cse->old_block) || (cse->cr != cr)
					|| cse->old_block == (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr));
				if ((NULL == cse->old_block) || (cse->cr != cr) || (cse->cycle != cr->cycle)
						|| (cse->tn <= ((blk_hdr_ptr_t)GDS_REL2ABS(cr->buffaddr))->tn))
				{	/* Bitmap reallocation resulted in a situation where checksums etc. must be recomputed */
					cse->old_block = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
					old_block = (blk_hdr_ptr_t)cse->old_block;
					/* Note: cse->cr needs to be set BEFORE the JNL_GET_CHECKSUM_ACQUIRED macro call
					 * as the macro relies on this.
					 */
					cse->cr = cr;
					cse->cycle = cr->cycle;
					if (!WAS_FREE(cse->blk_prior_state) && (NULL != jbp))
					{
						if (old_block->tn < jbp->epoch_tn)
						{	/* See comment before similar check in "gvincr_recompute_upd_array"
							 * for why this check is needed.
							 */
							bsiz = old_block->bsiz;
							if (bsiz > csd->blk_size)
							{	/* if this before image is not valid don't wait for it in crit */
								offset = free_bit + 1;		/* try further in this bitmap */
								continue;
							}
							JNL_GET_CHECKSUM_ACQUIRED_BLK(cse, csd, csa, old_block, bsiz);
						} else
							cse->blk_checksum = 0;
					}
				}
			} else
			{	/* in MM, although mm_update does not use cse->old_block, tp_tend uses it to write before-images.
				 * therefore, fix it to point to the reallocated block's buffer address
				 */
				cse->old_block = t_qread(cse->blk, (sm_int_ptr_t)&cse->cycle, &cse->cr);
				assert(GDSVCURR == cse->ondsk_blkver);	/* should have been already initialized in t_write_map */
				old_block = (blk_hdr_ptr_t)cse->old_block;
				if (NULL == old_block)
					return ((enum cdb_sc)rdfail_detail);
				assert(NULL == jbp);	/* don't need to have any JNL_GET_CHECKSUM_ACQUIRED_BLK logic */
			}
			*b_ptr++ = free_bit;
			offset = free_bit + 1;
			break;
		} while (TRUE);
	}
	assert(cse == bmp_begin_cse);
	assert(0 == *b_ptr);
	/* since bitmap block got modified, copy latest "ondsk_blkver" status from cache-record to bml_cse */
	assert((NULL != bml_cse->cr) || is_mm);
	old_block = (blk_hdr_ptr_t)bml_cse->old_block;
	assert(!WAS_FREE(bml_cse->blk_prior_state));	/* Bitmap blocks are never of type gds_t_acquired or gds_t_create */
	if (NULL != jbp)
	{	/* recompute CHECKSUM for the modified bitmap block before-image */
		if (old_block->tn < jbp->epoch_tn)
		{
			bsiz = old_block->bsiz;
			/* See comment before similar check in "gvincr_recompute_upd_array" for why this check is needed */
			if (bsiz > csd->blk_size)
				return cdb_sc_lostbmlcr;	/* This is a restartable condition, so restart */
			bml_cse->blk_checksum = jnl_get_checksum(old_block, csa, bsiz);
		} else
			bml_cse->blk_checksum = 0;
	}
	if (!is_mm)
		bml_cse->ondsk_blkver = bml_cse->cr->ondsk_blkver;
	return cdb_sc_normal;
}
