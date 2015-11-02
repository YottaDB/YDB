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

#include <stddef.h>		/* for offsetof macro */

#ifdef UNIX
#include "gtm_stdio.h"
#endif

#include "gtm_time.h"
#include "gtm_inet.h"	/* Required for gtmsource.h */
#include "gtm_string.h"

#ifdef VMS
#include <descrip.h>	/* Required for gtmsource.h */
#endif

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
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "interlock.h"
#include "gdsbgtr.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "t_commit_cleanup.h"
#include "mupipbckup.h"
#include "gvcst_blk_build.h"
#include "gvcst_protos.h"	/* for gvcst_search_blk prototype */
#include "cache.h"
#include "rc_cpt_ops.h"
#include "wcs_flu.h"
#include "jnl_write_pblk.h"
#include "jnl_write.h"
#include "process_deferred_stale.h"
#include "wcs_backoff.h"
#include "mm_update.h"
#include "bg_update.h"
#include "wcs_get_space.h"
#include "wcs_timer_start.h"
#include "send_msg.h"
#include "add_inter.h"
#include "t_qread.h"
#include "memcoherency.h"
#include "jnl_get_checksum.h"
#include "wbox_test_init.h"
#include "cert_blk.h"

#ifdef UNIX
#include "gtmrecv.h"
#endif

GBLREF	short			dollar_tlevel;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*first_sgm_info, *sgm_info_ptr;
GBLREF	tp_region		*tp_reg_list;
GBLREF	bool			tp_kill_bitmaps;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	int4			n_pvtmods, n_blkmods;
GBLREF	int			t_tries;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl, temp_jnlpool_ctl;
GBLREF	boolean_t		is_updproc;
GBLREF	seq_num			seq_num_zero;
GBLREF	seq_num			seq_num_one;
GBLREF	int			gv_fillfactor;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	int			rc_set_fragment;
GBLREF	uint4			update_array_size, cumul_update_array_size;
GBLREF	boolean_t		unhandled_stale_timer_pop;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	int4			tprestart_syslog_delta;
GBLREF	struct_jrec_tcom	tcom_record;
GBLREF	boolean_t		gvdupsetnoop; /* if TRUE, duplicate SETs update journal but not database (except for curr_tn++) */
GBLREF	bool			certify_all_blocks;
GBLREF	gv_namehead		*gv_target;

#ifdef UNIX
GBLREF	recvpool_addrs		recvpool;
#endif

boolean_t	reallocate_bitmap(sgm_info *si, cw_set_element *bml_cse);
enum cdb_sc	recompute_upd_array(srch_blk_status *hist1, cw_set_element *cse);

boolean_t	tp_tend(boolean_t crit_only)
{
	block_id		tp_blk;
	boolean_t		history_validated, is_mm, was_crit, x_lock, do_validation;
	boolean_t		do_deferred_writes = FALSE, replication = FALSE;
	bt_rec_ptr_t		bt;
	cache_rec_ptr_t		cr;
	cw_set_element		*cse;
	file_control		*fc;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	jnl_format_buffer	*jfb;
	sgm_info		*si, *tmpsi;
	tp_region		*tr, *tr_last;
	sgmnt_addrs		*csa, *tmpcsa;
	sgmnt_data_ptr_t	csd;
	srch_blk_status		*t1;
	trans_num		ctn, tnque_earliest_tn;
	trans_num		valid_thru;	/* buffers touched by this transaction will be valid thru this tn */
	enum cdb_sc		status;
	gd_region		*save_gv_cur_region;
	int			lcnt, participants;
	jnldata_hdr_ptr_t	jnl_header;
	int			repl_tp_region_count = 0;
	boolean_t		first_time = TRUE, release_crit, yes_jnl_no_repl, retvalue;
	uint4			jnl_status, leafmods, indexmods;
	uint4			total_jnl_rec_size;
	jnlpool_ctl_ptr_t	jpl, tjpl;
	boolean_t		read_before_image; /* TRUE if before-image journaling or online backup in progress */
	blk_hdr_ptr_t		old_block;
	unsigned int		bsiz;

	error_def(ERR_DLCKAVOIDANCE);
	error_def(ERR_JNLTRANS2BIG);
	error_def(ERR_JNLFILOPN);
	error_def(ERR_REPLOFFJNLON);

	assert(dollar_tlevel > 0);
	assert(0 == jnl_fence_ctl.level);
	participants = 0;
	status = cdb_sc_normal;
	/* if the transaction does no updates and the transaction history has not changed, we do not need any more validation */
	do_validation = FALSE;	/* initially set to FALSE, but set to TRUE below */
	jnl_status = 0;
	if (FALSE == crit_only)
	{
		for (si = first_sgm_info;  (NULL != si); si = si->next_sgm_info)
		{
			sgm_info_ptr = si;
			TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
			csa = cs_addrs;
			csd = cs_data;
			if ((csd->wc_blocked) ||			/* If blocked, or.. */
				((dba_mm == csa->hdr->acc_meth) &&	/* we have MM and.. */
				(csa->total_blks != csa->ti->total_blks)))	/* and file has been extended */
			{	/* Force repair */
				t_fail_hist[t_tries] = cdb_sc_helpedout; /* special status to prevent punishing altruism */
				TP_TRACE_HIST(CR_BLKEMPTY, NULL);
				return FALSE;
			}
			/* whenever si->first_cw_set is non-NULL, ensure that si->update_trans is TRUE */
			assert((NULL == si->first_cw_set) || si->update_trans);
			/* whenever si->first_cw_set is NULL, ensure that si->update_trans is FALSE
			 * except when the set noop optimization is enabled */
			assert((NULL != si->first_cw_set) || !si->update_trans || gvdupsetnoop);
			if (!si->update_trans)
			{
				if (si->start_tn == csa->ti->early_tn)
				{	/* read with no change to the transaction history. ensure we haven't overrun
					 * our history buffer and we have reasonable values for first and last */
					assert(si->last_tp_hist - si->first_tp_hist <= si->tp_hist_size);
					continue;
				} else
					do_validation = TRUE;
			} else
			{
				do_validation = TRUE;
				is_mm = (dba_mm == cs_data->acc_meth);
				/* We are still out of crit if this is not our last attempt. If so, run the region list and check
				 * that we have sufficient free blocks for our update. If not, get them now while we can.
				 * We will repeat this check later in crit but it will hopefully have little or nothing to do.
				 * bypass 1st check if already in crit -- check later
				 */
				if (!csa->now_crit && !is_mm && (csa->nl->wc_in_free < si->cw_set_depth + 1)
						&& !wcs_get_space(si->gv_cur_region, si->cw_set_depth + 1, NULL))
					assert(FALSE);	/* wcs_get_space() should have returned TRUE unconditionally in this case */
			}
			if (si->update_trans && JNL_ENABLED(csa))
			{	/* compute the total journal record size requirements before grab_crit().
				 * there is code later that will check for state changes from now to then
				 */
				TOTAL_TPJNL_REC_SIZE(total_jnl_rec_size, si, csa);
				/* compute current transaction's maximum journal space needs in number of disk blocks */
				si->tot_jrec_size = MAX_REQD_JNL_FILE_SIZE(total_jnl_rec_size);
				GTM_WHITE_BOX_TEST(WBTEST_TP_TEND_TRANS2BIG, si->tot_jrec_size, (2 * csd->autoswitchlimit));
				/* check if current TP transaction's journal size needs are greater than max jnl file size */
				if (si->tot_jrec_size > csd->autoswitchlimit)
					/* can't fit in current transaction's journal records into one journal file */
					rts_error(VARLSTCNT(6) ERR_JNLTRANS2BIG, 4, si->tot_jrec_size,
						JNL_LEN_STR(csd), csd->autoswitchlimit);
			}
		}	/* for (si ... ) */
		if (!do_validation)
		{
			if (CDB_STAGNATE <= t_tries)
			{
				for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
					rel_crit(tr->reg);
			}
			/* Must be done after REVERT since we are no longer in crit */
			if (unhandled_stale_timer_pop)
				process_deferred_stale();
			return TRUE;
		}
	}
	ESTABLISH_RET(t_ch, FALSE);
	/* the following section grabs crit in all regions touched by the transaction. We use a different
	 * structure here for grabbing crit. The tp_reg_list region list contains all the regions that
	 * were touched by this transaction. Since this array is sorted by the ftok value of the database
	 * file being operated on, the obtains will always occurr in a consistent manner. Therefore, we
	 * will grab crit on each file with wait since deadlock should not be able to occurr.
	 */
	for (lcnt = 0; ;lcnt++)
	{
		x_lock = TRUE;		/* Assume success */
		for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
		{	/* Track retries in debug mode */
			tmpcsa = &FILE_INFO(tr->reg)->s_addrs;
			tmpsi = (sgm_info *)(tmpcsa->sgm_info_ptr);
			DEBUG_ONLY(
				if (0 != lcnt)
				{
					BG_TRACE_ANY(tmpcsa, tp_crit_retries);
				}
			)
			/* Note that there are three ways a deadlock can occur.
			 * 	(a) If we are not in the final retry and we already hold crit on some region.
			 * 	(b) If we are in the final retry and we don't hold crit on some region.
			 * 	(c) If we are in the final retry and we hold crit on a frozen region that we want to update.
			 * 		This is possible if we did a tp_grab_crit through one of the gvcst_* routines
			 * 		when we first encountered the region in the TP transaction and it wasn't locked down
			 * 		although it was frozen then.
			 *	The first two cases we don't know of any way they can happen. Case (c) though can happen.
			 *	Nevertheless, we restart for all the three and in dbg version assert so we get some information.
			 */
			if (!crit_only)		/* do crit check only for tp_tend through op_commit, not for tp_restart case */
			{
				if ((CDB_STAGNATE > t_tries)
						? tmpcsa->now_crit
						: (!tmpcsa->now_crit || tmpsi->update_trans && tmpcsa->hdr->freeze))
				{
					send_msg(VARLSTCNT(8) ERR_DLCKAVOIDANCE, 6, DB_LEN_STR(tr->reg),
								&tmpcsa->ti->curr_tn, t_tries, dollar_trestart, tmpcsa->now_crit);
					/* the only possible case we know of is (c). assert to that effect */
					assert((CDB_STAGNATE <= t_tries) && tmpcsa->now_crit
							&& tmpsi->update_trans && tmpcsa->hdr->freeze);
					status = cdb_sc_needcrit;	/* break the possible deadlock by signalling a restart */
					t_fail_hist[t_tries] = status;
					SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(tmpcsa, status);
					TP_RETRY_ACCOUNTING(tmpcsa, tmpcsa->hdr, status);
					TP_CHANGE_REG_IF_NEEDED(tmpsi->gv_cur_region);	/* needed by TP_TRACE_HIST */
					TP_TRACE_HIST(CR_BLKEMPTY, NULL);
					return FALSE;
				}
			}
			if (tmpsi->update_trans)
			{
				TP_CHANGE_REG_IF_NEEDED(tr->reg);
				grab_crit(tr->reg);
				if (cs_data->freeze)
				{
					tr = tr->fPtr;		/* Increment so we release the lock we actually got */
					x_lock = FALSE;
					break;
				}
			} else
				grab_crit(tr->reg);
		}
		if (x_lock)
			break;
		tr_last = tr;
		for (tr = tp_reg_list; tr_last != tr; tr = tr->fPtr)
			rel_crit(tr->reg);
		if (lcnt > MAXHARDCRITS)
			wcs_backoff(lcnt);
	}	/* for (;;) */
	if (crit_only)
	{
		REVERT;
		return TRUE;
	}
	/* Any retry transition where the destination state is the 3rd retry, we don't want to release crit,
	 * i.e. for 2nd to 3rd retry transition or 3rd to 3rd retry transition.
	 * Therefore we need to release crit only if (CDB_STAGNATE - 1) > t_tries
	 * But 2nd to 3rd retry transition doesn't occur if in 2nd retry we get jnlstatemod/jnlclose/backupstatemod code.
	 * Hence the variable release_crit to track the above.
	 */
	release_crit = (CDB_STAGNATE - 1) > t_tries;
	history_validated = FALSE;
	assert(cdb_sc_normal == status);
	for (si = first_sgm_info;  (NULL != si);  si = si->next_sgm_info)
	{
		leafmods = indexmods = 0;
		sgm_info_ptr = si;
		TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
		csa = cs_addrs;
		csd = cs_data;
		CHECK_TN(csa, csd, csd->trans_hist.curr_tn);	/* can issue rts_error TNTOOLARGE */
		is_mm = (dba_mm == csd->acc_meth);
		if (!is_mm)
			tnque_earliest_tn = ((th_rec_ptr_t)((sm_uc_ptr_t)csa->th_base + csa->th_base->tnque.fl))->tn;
		/* whenever si->first_cw_set is non-NULL, ensure that si->update_trans is TRUE */
		assert((NULL == si->first_cw_set) || si->update_trans);
		/* whenever si->first_cw_set is NULL, ensure that si->update_trans is FALSE
		 * except when the set noop optimization is enabled */
		assert((NULL != si->first_cw_set) || !si->update_trans || gvdupsetnoop);
		if (si->update_trans)
		{
			if (JNL_ALLOWED(csa))
			{
				if ((csa->jnl_state != csd->jnl_state) || (csa->jnl_before_image != csd->jnl_before_image))
				{
					for (si = first_sgm_info;  NULL != si;  si = si->next_sgm_info)
					{
						TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
						cs_addrs->jnl_state = cs_data->jnl_state;
						cs_addrs->jnl_before_image = cs_data->jnl_before_image;
						/* jnl_file_lost() causes a jnl_state transition from jnl_open to jnl_closed
						 * and additionally causes a repl_state transition from repl_open to repl_closed
						 * all without standalone access. This means that csa->repl_state might be repl_open
						 * while csd->repl_state might be repl_closed. update csa->repl_state in this case
						 * as otherwise the rest of the code might look at csa->repl_state and incorrectly
						 * conclude replication is on and generate sequence numbers when actually no journal
						 * records are being generated. [C9D01-002219]
						 */
						cs_addrs->repl_state = cs_data->repl_state;
					}
					status = cdb_sc_jnlstatemod;
					if ((CDB_STAGNATE - 1) == t_tries)
						release_crit = TRUE;
					goto failed;
				}
			}
			/* Caution : since csa->backup_in_prog was initialized in op_tcommit only if si->first_cw_set was
			 * non-NULL, it should be used in tp_tend() only within an if (NULL != si->first_cw_set)
			 */
			if ((NULL != si->first_cw_set) && (csa->backup_in_prog != (BACKUP_NOT_IN_PROGRESS != csa->nl->nbb)))
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
			}
			read_before_image = ((JNL_ENABLED(csa) && csa->jnl_before_image) || csa->backup_in_prog);
			if (!is_mm)
			{	/* in crit, ensure cache-space is available. the out-of-crit check done above might not be enough */
				if (csa->nl->wc_in_free < si->cw_set_depth + 1)
				{
					if (!wcs_get_space(si->gv_cur_region, si->cw_set_depth + 1, NULL))
					{
						assert(FALSE);
						SET_TRACEABLE_VAR(csd->wc_blocked,TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_wcsgetspace);
						status = cdb_sc_cacheprob;
						SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
						TP_RETRY_ACCOUNTING(csa, csd, status);
						TP_TRACE_HIST(CR_BLKEMPTY, NULL);
						goto failed;
					}
				}
				VMS_ONLY(
					if (csd->clustered  && !CCP_SEGMENT_STATE(csa->nl, CCST_MASK_HAVE_DIRTY_BUFFERS))
					{
						CCP_FID_MSG(si->gv_cur_region, CCTR_FLUSHLK);
						ccp_userwait(si->gv_cur_region, CCST_MASK_HAVE_DIRTY_BUFFERS,
											NULL, csa->nl->ccp_cycle);
					}
				)
			}
		}
		/* the following section verifies that the optimistic concurrency was justified */
		if (tprestart_syslog_delta)
			n_blkmods = n_pvtmods = 0;
		assert(cdb_sc_normal == status);
		for (t1 = si->first_tp_hist;  t1 != si->last_tp_hist; t1++)
		{
			assert(NULL != t1->blk_target);
			if (is_mm)
			{	/* the check below is different from the one for BG (i.e. doesn't have the killtn check)
				 * because there is no BT equivalent in MM. there is a mmblk_rec which is more or
				 * less the same as a BT. when the BT becomes full functional, we can use that
				 * optimization for MM also.
				 */
				if (t1->tn <= ((blk_hdr_ptr_t)t1->buffaddr)->tn)
				{
					assert(CDB_STAGNATE > t_tries);
					cse = t1->ptr;
					assert(!cse || !cse->high_tlevel);
					if (!cse || !cse->recompute_list_head || cse->write_type
						|| (cdb_sc_normal != recompute_upd_array(t1, cse)) || !++leafmods)
					{
						status = cdb_sc_blkmod;
						SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
						TP_RETRY_ACCOUNTING(csa, csd, status);
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
						cse = t1->ptr;
						assert(!cse || !cse->high_tlevel && cse->blk_target == t1->blk_target);
						assert(CDB_STAGNATE > t_tries);
						/* "indexmods" and "leafmods" are to monitor number of blocks that used
						 * indexmod and noisolation optimizations respectively. Note that once
						 * in this part of the code, atleast one of them will be non-zero and
						 * if both of them turn out to be non-zero, then we need to restart.
						 */
						if (t1->level)
						{
							if (cse || t1->tn <= bt->killtn)
								status = cdb_sc_blkmod;
							else
							{
								indexmods++;
								if (leafmods)
									status = cdb_sc_blkmod;
							}
						} else
						{	/* For a non-isolated global, if the leaf block isn't part of the cw-set,
							 * this means that it was involved in an M-kill that freed the data-block
							 * from the B-tree. In this case, if the leaf-block has changed since
							 * we did our read of the block, we have to redo the M-kill. But since
							 * redo of that M-kill might involve much more than just leaf-level block
							 * changes, we be safe and do a restart. If the need for NOISOLATION
							 * optimization for M-kills is felt, we need to revisit this.
							 */
							if (!t1->blk_target->noisolation || !cse)
								status = cdb_sc_blkmod;
							else
							{
								assert(cse->write_type || cse->recompute_list_head);
								leafmods++;
								if (indexmods || cse->write_type
										|| cdb_sc_normal != recompute_upd_array(t1, cse))
									status = cdb_sc_blkmod;
							}
						}
						if (cdb_sc_normal != status)
						{
							if (tprestart_syslog_delta)
							{
								n_blkmods++;
								if (t1->ptr)
									n_pvtmods++;
								if (1 != n_blkmods)
									continue;
							}
							status = cdb_sc_blkmod;
							SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
							TP_RETRY_ACCOUNTING(csa, csd, status);
							TP_TRACE_HIST_MOD(t1->blk_num, t1->blk_target,
									tp_blkmod_tp_tend, csd, t1->tn, bt->tn, t1->level);
							DEBUG_ONLY(continue;)
							PRO_ONLY(goto failed;)
						}
					}
				} else if (t1->tn <= tnque_earliest_tn)
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_losthist;
					SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
					TP_RETRY_ACCOUNTING(csa, csd, status);
					TP_TRACE_HIST(t1->blk_num, t1->blk_target);
					DEBUG_ONLY(continue;)
					PRO_ONLY(goto failed;)
				}
				assert(CYCLE_PVT_COPY != t1->cycle);
				if (t1->ptr)
				{	/* do cycle check only if blk has cse and hasn't been built (if it has, then tp_hist
					 * would have done the cdb_sc_lostcr check soon after it got built) or if we have BI
					 * journaling or online backup is currently running. The BI-journaling/online-backup check
					 * is to ensure that the before-image/pre-update-copy we write hasn't been recycled.
					 */
					if ((NULL == bt) || (CR_NOTVALID == bt->cache_index))
						cr = db_csh_get(t1->blk_num);
					else
					{
						cr = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
						if ((NULL != cr) && (cr->blk != bt->blk))
						{
							assert(FALSE);
							SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_crbtmismatch1);
							status = cdb_sc_crbtmismatch;
							SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
							TP_RETRY_ACCOUNTING(csa, csd, status);
							TP_TRACE_HIST(t1->blk_num, t1->blk_target);
							goto failed;
						}
					}
					if ((cache_rec_ptr_t)CR_NOTVALID == cr)
					{
						SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_t1);
						status = cdb_sc_cacheprob;
						SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
						TP_RETRY_ACCOUNTING(csa, csd, status);
						TP_TRACE_HIST(t1->blk_num, t1->blk_target);
						goto failed;
					}
					assert(si->update_trans);	/* ensure read_before_image was computed above */
					if (!t1->ptr->new_buff || read_before_image)
					{
						if ((NULL == cr) || (cr->cycle != t1->cycle) ||
							((sm_long_t)GDS_ANY_REL2ABS(csa, cr->buffaddr) != (sm_long_t)t1->buffaddr))
						{
							if ((NULL != cr) && (NULL != bt) && (cr->blk != bt->blk))
							{
								assert(FALSE);
								SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
								BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_crbtmismatch2);
								status = cdb_sc_crbtmismatch;
								SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
								TP_RETRY_ACCOUNTING(csa, csd, status);
								TP_TRACE_HIST(t1->blk_num, t1->blk_target);
								goto failed;
							}
							assert(CDB_STAGNATE > t_tries);
							status = cdb_sc_lostcr;
							SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
							TP_RETRY_ACCOUNTING(csa, csd, status);
							TP_TRACE_HIST(t1->blk_num, t1->blk_target);
							DEBUG_ONLY(continue;)
							PRO_ONLY(goto failed;)
						}
					}
					/* The only case cr can be NULL at this point of code is when
					 * 	a) t1->ptr->new_buff is non-NULL
					 *	b) AND the block is not in cache
					 *	c) AND we don't have before-image-journaling
					 *	d) AND online backup is not running.
					 * In this case bg_update will do a db_csh_getn() and appropriately set in_cw_set
					 * field to be TRUE so we shouldn't be manipulating those fields in that case.
					 */
					if (cr)
					{
						assert(si->cr_array_index < si->cr_array_size);
						assert(0 <= si->cr_array_index);
						si->cr_array[si->cr_array_index++] = cr;
						assert(FALSE == cr->in_cw_set);
						cr->in_cw_set = TRUE;
						cr->refer = TRUE;
					}
				}
			}
		} /* for (t1 ... ) */
		DEBUG_ONLY(
			if (cdb_sc_normal != status)
				goto failed;
		)
		if (DIVIDE_ROUND_UP(si->num_of_blks, 4) < leafmods)	/* if status == cdb_sc_normal, then leafmods  */
		{
			status = cdb_sc_toomanyrecompute;		/* is exactly the number of recomputed blocks */
			goto failed;
		}
		/* Check bit maps for usage */
		if (NULL == si->first_cw_set)
			continue;
		assert(cdb_sc_normal == status);
		for (cse = si->first_cw_bitmap; NULL != cse; cse = cse->next_cw_set)
		{
			assert(0 == cse->jnl_freeaddr);	/* ensure haven't missed out resetting jnl_freeaddr for any cse in
							 * t_write/t_create/{t,mu}_write_map/t_write_root [D9B11-001991] */
			TRAVERSE_TO_LATEST_CSE(cse);
			assert(0 == ((off_chain *)&cse->blk)->flag);
			assert(!cse || !cse->high_tlevel);
			if (is_mm)
			{
				if ((cse->tn <= ((blk_hdr_ptr_t)cse->old_block)->tn)
					&& ((0 > cse->reference_cnt) || !reallocate_bitmap(si, cse)))
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_bmlmod;
					SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
					TP_RETRY_ACCOUNTING(csa, csd, status);
					TP_TRACE_HIST(cse->blk, NULL);
					goto failed;
				}
			} else
			{
				tp_blk = cse->blk;
				bt = bt_get(tp_blk);
				if (NULL != bt)
				{
					if ((cse->tn <= bt->tn)
						&& ((0 > cse->reference_cnt) || !reallocate_bitmap(si, cse)))
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_bmlmod;
						SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
						TP_RETRY_ACCOUNTING(csa, csd, status);
						TP_TRACE_HIST(tp_blk, NULL);
						goto failed;
					}
				} else if (cse->tn <= tnque_earliest_tn)
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_lostbmlhist;
					SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
					TP_RETRY_ACCOUNTING(csa, csd, status);
					TP_TRACE_HIST(tp_blk, NULL);
					goto failed;
				}
				TRAVERSE_TO_LATEST_CSE(cse);
				assert((NULL == cse) || (NULL == cse->new_buff));
				if ((NULL == bt) || (CR_NOTVALID ==  bt->cache_index))
				{
					cr = db_csh_get(tp_blk);
					if ((cache_rec_ptr_t)CR_NOTVALID == cr)
					{
						status = cdb_sc_cacheprob;
						SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
						TP_RETRY_ACCOUNTING(csa, csd, status);
						TP_TRACE_HIST(tp_blk, NULL);
						SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_bitmap);
						goto failed;
					}
				} else
				{
					cr = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
					if (cr->blk != bt->blk)
					{
						assert(FALSE);
						SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_crbtmismatch3);
						status = cdb_sc_crbtmismatch;
						SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
						TP_RETRY_ACCOUNTING(csa, csd, status);
						TP_TRACE_HIST(tp_blk, NULL);
						goto failed;
					}
				}
				if ((NULL == cr) || (cr->cycle != cse->cycle) ||
					((sm_long_t)GDS_ANY_REL2ABS(csa, cr->buffaddr) != (sm_long_t)cse->old_block))
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_lostbmlcr;
					SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
					TP_RETRY_ACCOUNTING(csa, csd, status);
					TP_TRACE_HIST(tp_blk, NULL);
					goto failed;
				}
				assert(si->cr_array_index < si->cr_array_size);
				assert(0 <= si->cr_array_index);
				si->cr_array[si->cr_array_index++] = cr;
				assert(FALSE == cr->in_cw_set);
				cr->in_cw_set = TRUE;
				cr->refer = TRUE;
			}
		}	/* for (all bitmaps written) */
	}	/* for (si ... ) */
	history_validated = TRUE;
	assert(cdb_sc_normal == status);
	yes_jnl_no_repl = FALSE;
	for (si = first_sgm_info; (NULL != si); si = si->next_sgm_info)
	{
		si->backup_block_saved = FALSE;
		if (!si->update_trans)
			continue;
		assert((NULL == si->first_cw_set) || (0 != si->cw_set_depth));
		sgm_info_ptr = si;
		TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
		csa = cs_addrs;
		csd = cs_data;
		ctn = csd->trans_hist.curr_tn;
		if (REPL_ALLOWED(csa))
		{
			assert(JNL_ENABLED(csa) || REPL_WAS_ENABLED(csa));
			replication = TRUE;
			repl_tp_region_count++;
		} else if (JNL_ENABLED(csa))
		{
			yes_jnl_no_repl = TRUE;
			save_gv_cur_region = gv_cur_region; /* save the region for later error reporting */
		}
		is_mm = dba_mm == csa->hdr->acc_meth;
		jbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
		/* Caution : since csa->backup_in_prog was initialized in op_tcommit only if si->first_cw_set was
		 * non-NULL, it should be used in tp_tend() only within an if (NULL != si->first_cw_set)
		 */
		if (!is_mm && (NULL != si->first_cw_set) && ((NULL != jbp) || csa->backup_in_prog))
		{
			for (cse = si->first_cw_set;  NULL != cse;  cse = cse->next_cw_set)
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
						status = cdb_sc_cacheprob;
						SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_jnl_cwset);
						SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
						TP_RETRY_ACCOUNTING(csa, csd, status);
						goto failed;
					}
					if ((NULL == cr) || (0 <= cr->read_in_progress))
					{
						TP_TRACE_HIST(cse->blk, cse->blk_target);
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_lostbefor;
						SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
						TP_RETRY_ACCOUNTING(csa, csd, status);
						goto failed;
					}
					assert(si->cr_array_index < si->cr_array_size);
					assert(0 <= si->cr_array_index);
					si->cr_array[si->cr_array_index++] = cr;
					assert(FALSE == cr->in_cw_set);
					cr->in_cw_set = TRUE;
					cr->refer = TRUE;
					cse->ondsk_blkver = cr->ondsk_blkver;
					if ((cse->cr != cr) || (cse->cycle != cr->cycle))
					{	/* Global buffer containing "cse->blk" changed since we read it out of crit */
						old_block = (blk_hdr_ptr_t)GDS_REL2ABS(cr->buffaddr);
						assert(sizeof(bsiz) == sizeof(old_block->bsiz));
						if (NULL != jbp)
						{	/* PBLK checksum was computed outside-of-crit when block was read but
							 * block has relocated in the cache since then. Recompute the checksum.
							 * Even though we hold crit at this point we might see invalid bsiz fields
							 * in case it is a reused block that is still in V4 format. Assert this.
							 * In pro dont take chances so treat MIN(bsiz, csd->blk_size) as the
							 * block size to unconditionally compute the checksum.
							 */
							bsiz = old_block->bsiz;
							assert((bsiz <= csd->blk_size) || (csd->fully_upgraded
								&& (((v15_blk_hdr_ptr_t)old_block)->bsiz <= csd->blk_size)));
							if (old_block->tn < jbp->epoch_tn)
								cse->blk_checksum = jnl_get_checksum(INIT_CHECKSUM_SEED,
											(uint4 *)old_block,
											MIN(bsiz, old_block->bsiz));
							else
								cse->blk_checksum = 0;
						}
						cse->cr = cr;
						cse->cycle = cr->cycle;
						cse->old_block = (sm_uc_ptr_t)old_block;
					}
				}
			}
		}
		if (JNL_ENABLED(csa))
		{
			if (first_time)
			{	/* The JNL_SHORT_TIME done below should be done before any journal writing activity on the
				 * journal file. This is because all the jnl record writing routines assume jgbl.gbl_jrec_time
				 * initialized approporiately. Note that jnl_ensure_open() can call cre_jnl_file() which
				 * in turn assumes jgbl.gbl_jrec_time is set. Also jnl_file_extend() can call
				 * jnl_write_epoch_rec() which in turn assumes jgbl.gbl_jrec_time is set.
				 * In case of forw-phase-recovery, mur_output_record() would have already set this.
				 */
				if (!jgbl.forw_phase_recovery)
					JNL_SHORT_TIME(jgbl.gbl_jrec_time);
				assert(jgbl.gbl_jrec_time);
				first_time = FALSE;
			}
			jnl_status = jnl_ensure_open();
			GTM_WHITE_BOX_TEST(WBTEST_TP_TEND_JNLFILOPN, jnl_status, ERR_JNLFILOPN);
			if (jnl_status != 0)
			{
				assert(csa->ti->early_tn == ctn);
				rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
			}
			jpc = csa->jnl;
			jbp = jpc->jnl_buff;
			if (DISK_BLOCKS_SUM(jbp->freeaddr, si->total_jnl_rec_size) > jbp->filesize)
			{	/* Moved here to prevent jnlrecs split across multiple generation journal files. */
				jnl_flush(jpc->region);
				if (-1 == jnl_file_extend(jpc, si->total_jnl_rec_size))
				{
					assert((!JNL_ENABLED(csd)) && JNL_ENABLED(csa));
					status = t_fail_hist[t_tries] = cdb_sc_jnlclose;
					TP_TRACE_HIST(CR_BLKEMPTY, NULL);
					if ((CDB_STAGNATE - 1) == t_tries)
						release_crit = TRUE;
					goto failed;
				}
			}
			if (JNL_HAS_EPOCH(jbp) && (jbp->next_epoch_time <= jgbl.gbl_jrec_time))
			{	/* Flush the cache. Since we are in crit, defer syncing the epoch */
				if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH))
				{
					SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
					BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_jnl_wcsflu);
					status = t_fail_hist[t_tries] = cdb_sc_cacheprob;
					SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, status);
					TP_RETRY_ACCOUNTING(csa, csd, status);
					TP_TRACE_HIST(CR_BLKEMPTY, NULL);
					goto failed;
				}
			}
		}	/* if (journaling) */
	}
	if (repl_tp_region_count && yes_jnl_no_repl)
		rts_error(VARLSTCNT(4) ERR_REPLOFFJNLON, 2, DB_LEN_STR(save_gv_cur_region));
	assert(cdb_sc_normal == status);
	if (replication)
	{
		jpl = jnlpool_ctl;
		tjpl = temp_jnlpool_ctl;
		grab_lock(jnlpool.jnlpool_dummy_reg);
		QWASSIGN(tjpl->write_addr, jpl->write_addr);
		tjpl->write = jpl->write;
		QWASSIGN(tjpl->jnl_seqno, jpl->jnl_seqno);
		INT8_ONLY(assert(tjpl->write == tjpl->write_addr % tjpl->jnlpool_size);)
		tjpl->write += sizeof(jnldata_hdr_struct);
		if (tjpl->write >= tjpl->jnlpool_size)
		{
			assert(tjpl->write == tjpl->jnlpool_size);
			tjpl->write = 0;
		}
		assert(jgbl.cumul_jnl_rec_len);
		jgbl.cumul_jnl_rec_len += TCOM_RECLEN * repl_tp_region_count + sizeof(jnldata_hdr_struct);
		DEBUG_ONLY(jgbl.cumul_index += repl_tp_region_count;)
		assert(jgbl.cumul_jnl_rec_len % JNL_REC_START_BNDRY == 0);
		assert(QWEQ(jpl->early_write_addr, jpl->write_addr));
		QWADDDW(jpl->early_write_addr, jpl->write_addr, jgbl.cumul_jnl_rec_len);
		/* Source server does not read in crit. It relies on early_write_addr, the transaction
		 * data, lastwrite_len, write_addr being updated in that order. To ensure this order,
		 * we have to force out early_write_addr to its coherency point now. If not, the source
		 * server may read data that is overwritten (or stale). This is true only on
		 * architectures and OSes that allow unordered memory access
		 */
		SHM_WRITE_MEMORY_BARRIER;
	}
	/* There are two possible approaches that can be taken from now onwards.
	 * 	a) Write journal and database records together for a region and move onto the next region.
	 * 	b) Write journal records for all regions and only then move onto writing database updates for all regions.
	 *
	 * If journal and database updates are done together region by region, there is a problem in that if an error
	 * occurs after one region's updates are committed (to jnl and db) or if the process gets STOP/IDed in VMS,
	 * secshr_db_clnup() should then commit BOTH the journal and database updates of the remaining regions.
	 * committing journal updates is not trivial in secshr_db_clnup() since it can also be invoked as a user termination
	 * handler in VMS in which case it cannot do any I/O.
	 *
	 * We therefore take approach (b) below. Write journal records for all regions in one loop. Write database updates
	 * for all regions in another loop. This way if any error occurs before database updates for any region begins in
	 * the second loop, we cleanup the structures as if the transaction is rolled back (there is an exception to this in
	 * that currently the journal buffers are not rolled back to undo the write of journal records but currently
	 * MUPIP RECOVER knows to handle such records and TR C9905-001072 exists to make the source-server handle such records).
	 * If any error occurs (or if the process gets STOP/IDed in VMS) while we are committing database updates,
	 * secshr_db_clnup() will be invoked and will complete the updates for this TP transaction.
	 */
	/* the following section writes journal records in all regions */
	for (si = first_sgm_info; NULL != si; si = si->next_sgm_info)
	{
		if (!si->update_trans)
			continue;
		assert((NULL == si->first_cw_set) || (0 != si->cw_set_depth));
		TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
		csa = cs_addrs;
		ctn = csa->ti->curr_tn;
		assert(ctn == csa->ti->early_tn);
		csa->ti->early_tn = ctn + 1;
		/* Write non-logical records (PBLK) if applicable */
		if (JNL_ENABLED(csa))
		{
			jpc = csa->jnl;
			jbp = jpc->jnl_buff;
			/* si->tmp_cw_set_depth is a copy of si->cw_set_depth at TOTAL_TPJNL_REC_SIZE calculation time;
			 * ensure it has not changed until now when the actual jnl record write occurs.
			 * same case with csa->jnl_before_images & jbp->before_images.
			 */
			assert(si->cw_set_depth == si->tmp_cw_set_depth);
			assert(jbp->before_images == csa->jnl_before_image);
			if (0 == jpc->pini_addr)
				jnl_put_jrt_pini(csa);
			if (jbp->before_images)
			{
				for (cse = si->first_cw_set;  NULL != cse;  cse = cse->next_cw_set)
				{	/* Write out before-update journal image records */
					TRAVERSE_TO_LATEST_CSE(cse);
					old_block = (blk_hdr_ptr_t)cse->old_block;
					ASSERT_IS_WITHIN_SHM_BOUNDS((sm_uc_ptr_t)old_block, csa);
					DEBUG_ONLY(is_mm = (dba_mm == csa->hdr->acc_meth);)
					DEBUG_ONLY(csd = cs_data;)
					DBG_ENSURE_OLD_BLOCK_IS_VALID(cse, is_mm, csa, csd);
					assert((n_gds_t_op != cse->mode) && (gds_t_committed != cse->mode));
					assert(n_gds_t_op < kill_t_create);
					assert(n_gds_t_op < kill_t_write);
					if (n_gds_t_op <= cse->mode)
						continue;
					if ((NULL != old_block) && (old_block->tn < jbp->epoch_tn))
					{
						assert(old_block->bsiz <= csd->blk_size);
						if (!cse->blk_checksum)
							cse->blk_checksum = jnl_get_checksum(INIT_CHECKSUM_SEED,
											(uint4 *)old_block, old_block->bsiz);
						else
							assert(cse->blk_checksum == jnl_get_checksum(INIT_CHECKSUM_SEED,
								(uint4 *)old_block, old_block->bsiz));
						jnl_write_pblk(csa, cse, old_block);
						cse->jnl_freeaddr = jbp->freeaddr;
					} else
						cse->jnl_freeaddr = 0;
				}
			}
		}
		/* Write logical journal records if applicable. */
		if (JNL_WRITE_LOGICAL_RECS(csa))
		{
			if (!jgbl.forw_phase_recovery)
			{
				if (0 == participants)
				{
					if (replication)
						QWASSIGN(jnl_fence_ctl.token, tjpl->jnl_seqno);
					else
					{
						TOKEN_SET(&jnl_fence_ctl.token, csa->ti->curr_tn, csa->regnum);
					}
				}
				++participants;
			}
			for (jfb = si->jnl_head;  NULL != jfb; jfb = jfb->next)
				jnl_write_logical(csa, jfb);
		}
	}
	/* the next section marks the transaction complete in the journal by writing TCOM record in all regions */
	tcom_record.prefix.time = jgbl.gbl_jrec_time;
	if (!jgbl.forw_phase_recovery)
	{
		tcom_record.participants = participants;
		QWASSIGN(tcom_record.token_seq.token, jnl_fence_ctl.token);
	} else
	{
		tcom_record.participants = jgbl.mur_jrec_participants;
		QWASSIGN(tcom_record.token_seq, jgbl.mur_jrec_token_seq);
	}
	if (replication)
	{
		assert(!jgbl.forw_phase_recovery);
		QWINCRBY(tjpl->jnl_seqno, seq_num_one);
		if (is_updproc)
		{
			VMS_ONLY(
				QWINCRBY(jgbl.max_resync_seqno, seq_num_one);
			)
			UNIX_ONLY(
				assert(REPL_PROTO_VER_UNINITIALIZED != recvpool.gtmrecv_local->last_valid_remote_proto_ver);
				if (REPL_PROTO_VER_DUALSITE == recvpool.gtmrecv_local->last_valid_remote_proto_ver)
					QWINCRBY(jgbl.max_dualsite_resync_seqno, seq_num_one);
			)
		}
	}
	/* Note that only those regions that are actively journaling will appear in the following list: */
	for (csa = jnl_fence_ctl.fence_list;  (sgmnt_addrs *)-1L != csa;  csa = csa->next_fenced)
	{
		jpc = csa->jnl;
		assert(((sgm_info *)(csa->sgm_info_ptr))->update_trans);
		tcom_record.prefix.pini_addr = jpc->pini_addr;
		tcom_record.prefix.tn = csa->ti->curr_tn;
		tcom_record.prefix.checksum = INIT_CHECKSUM_SEED;
		if (REPL_ALLOWED(csa))
		{
			csa->hdr->reg_seqno = tjpl->jnl_seqno;
			if (is_updproc)
			{
				VMS_ONLY(
					QWASSIGN(csa->hdr->resync_seqno, jgbl.max_resync_seqno);
				)
				UNIX_ONLY(
					assert(REPL_PROTO_VER_UNINITIALIZED != recvpool.gtmrecv_local->last_valid_remote_proto_ver);
					if (REPL_PROTO_VER_DUALSITE == recvpool.gtmrecv_local->last_valid_remote_proto_ver)
						QWASSIGN(csa->hdr->dualsite_resync_seqno, jgbl.max_dualsite_resync_seqno);
				)
			}
		}
		TP_CHANGE_REG_IF_NEEDED(jpc->region);
		/* Note tcom_record.jnl_tid was set in op_tstart() or updproc() */
		JNL_WRITE_APPROPRIATE(csa, jpc, JRT_TCOM, (jnl_record *)&tcom_record, NULL, NULL);
	}
	/* the following section is the actual commitment of the changes in the database */
	for (si = first_sgm_info; NULL != si; si = si->next_sgm_info)
	{
		if (!si->update_trans)
			continue;
		assert((NULL == si->first_cw_set) || (0 != si->cw_set_depth));
		sgm_info_ptr = si;
		TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
		csa = cs_addrs;
		csd = cs_data;
		is_mm = (dba_mm == csa->hdr->acc_meth);
		ctn = csd->trans_hist.curr_tn;
		assert((ctn + 1) == csa->ti->early_tn);
		csa->prev_free_blks = csa->ti->free_blocks;
		csa->t_commit_crit = TRUE;
		if (csd->dsid && tp_kill_bitmaps)
			rc_cpt_inval();
		cse = si->first_cw_set;
		if (NULL != cse)
		{
			do
			{
				TRAVERSE_TO_LATEST_CSE(cse);
				assert((n_gds_t_op != cse->mode) && (gds_t_committed != cse->mode));
				assert(n_gds_t_op < kill_t_create);
				assert(n_gds_t_op < kill_t_write);
				cse->old_mode = cse->mode;	/* note down before being reset to gds_t_committed */
				if (n_gds_t_op > cse->mode)
				{
					if (csd->dsid && !tp_kill_bitmaps && (0 == cse->level))
					{
						assert(!is_mm);
						rc_cpt_entry(cse->blk);
					}
					status = is_mm ? mm_update(cse, NULL, ctn, ctn, si) : bg_update(cse, NULL, ctn, ctn, si);
					if (cdb_sc_normal != status)
					{	/* the database is probably in trouble */
						assert(gtm_white_box_test_case_enabled);
						TP_TRACE_HIST(cse->blk, cse->blk_target);
						retvalue = t_commit_cleanup(status, 0);	/* return value of TRUE implies	       */
						assert(retvalue); /* secshr_db_clnup() should have completed the commit for us */
						status = cdb_sc_normal;	/* reset status to normal as transaction is complete */
						goto skip_failed;	/* do not do "failed:" processing as we do not hold crit */
						/* At this time "si->cr_array_index" could be non-zero for one or more regions
						 * and a few cache-records might have their "in_cw_set" field set to TRUE. We
						 * should not reset "in_cw_set" as we don't hold crit at this point and also because
						 * we might still need those buffers pinned until their before-images are backed up
						 * in wcs_recover (in case an online backup was running while secshr_db_clnup
						 * did its job). The local variable "si->cr_array_index" could be reset at this
						 * point but since it is already unconditionally done in tp_clean_up (invoked
						 * before the end of this transaction) we dont do anything here.
						 */
					}
				} else if (!cse->done)
				{	/* This is a block that is needed in the 2nd-phase of KILL. Build a private copy
					 * right now while we hold crit and the update array points to validated buffer contents.
					 */
					gvcst_blk_build(cse, (uchar_ptr_t)cse->new_buff, 0);
					cse->done = TRUE;
					assert(NULL != cse->blk_target);
					CERT_BLK_IF_NEEDED(certify_all_blocks, si->gv_cur_region, cse, cse->new_buff, gv_target);
				}
				cse->mode = gds_t_committed;
				cse = cse->next_cw_set;
			} while (NULL != cse);
		} else
			si->update_trans = T_COMMIT_STARTED; /* tell secshr_db_clnup(), roll-back is no longer possible */
		csa->t_commit_crit = FALSE;
		INCREMENT_CURR_TN(csd);
		/* write out the db header every HEADER_UPDATE_COUNT -1 transactions */
		if (!(csd->trans_hist.curr_tn & (HEADER_UPDATE_COUNT - 1)))
			fileheader_sync(si->gv_cur_region);
		if (NULL != si->kill_set_head)
			INCR_KIP(csd, csa, si->kip_incremented);
	} /* for (si ... ) */
	assert(cdb_sc_normal == status);
	if (replication)
	{
		assert(jgbl.cumul_index == jgbl.cu_jnl_index);
		assert((jpl->write + jgbl.cumul_jnl_rec_len) % jpl->jnlpool_size == tjpl->write);
		assert(QWGT(jpl->early_write_addr, jpl->write_addr));
		jnl_header = (jnldata_hdr_ptr_t)(jnlpool.jnldata_base + jpl->write);	/* Begin atomic stmnts */
		jnl_header->jnldata_len = jgbl.cumul_jnl_rec_len;
		jnl_header->prev_jnldata_len = jpl->lastwrite_len;
		jpl->lastwrite_len = jnl_header->jnldata_len;
		/* For systems with UNORDERED memory access (example, ALPHA, POWER4, PA-RISC 2.0), on a multi
		 * processor system, it is possible that the source server notices the change in write_addr
		 * before seeing the change to jnlheader->jnldata_len, leading it to read an invalid
		 * transaction length. To avoid such conditions, we should commit the order of shared
		 * memory updates before we update write_addr. This ensures that the source server sees all
		 * shared memory updates related to a transaction before the change in write_addr
		 */
		SHM_WRITE_MEMORY_BARRIER;
		jpl->write = tjpl->write;
		/* jpl->write_addr should be updated before updating jpl->jnl_seqno as secshr_db_clnup() relies on this */
		QWINCRBYDW(jpl->write_addr, jnl_header->jnldata_len);
		QWASSIGN(jpl->jnl_seqno, tjpl->jnl_seqno);			/* End atomic stmnts */
		assert(QWEQ(jpl->early_write_addr, jpl->write_addr));
		rel_lock(jnlpool.jnlpool_dummy_reg);
	}
	assert(cdb_sc_normal == status);
	/* Caution: followthrough, cleanup for normal and abnormal "status" */
failed:
	for (si = first_sgm_info;  NULL != si; si = si->next_sgm_info)
	{
		while (si->cr_array_index > 0)
			si->cr_array[--si->cr_array_index]->in_cw_set = FALSE;
		if (history_validated)
		{	/* reset transaction numbers in the gv_target histories so they
			 * will be valid for a future access utilizing the clue field
			 * even if the transaction is failing because of bitmaps or journal issues,
			 * this occurs to improve the performance and chances on restart (retry)
			 */
			valid_thru = ((sgmnt_addrs *)&FILE_INFO(si->gv_cur_region)->s_addrs)->ti->curr_tn;
			for (cse = si->first_cw_set; NULL != cse; cse = cse->next_cw_set)
			{
				TRAVERSE_TO_LATEST_CSE(cse);
				if (cse->blk_target)
					for (t1 = &cse->blk_target->hist.h[0]; t1->blk_num; t1++)
						t1->tn = valid_thru;
			}
		}
	}
	/* If either finished successfully or we are not doing final retry, release all the
	   critical locks we have obtained. Take this moment of non-critness to check if we
	   had an unhandled IO timer pop. */
	if ((cdb_sc_normal == status) || release_crit)
	{	/* Release regions  */
		for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
			rel_crit(tr->reg);
		do_deferred_writes = TRUE;
	}

skip_failed:
	REVERT;
	/* Must be done after REVERT since we are no longer in crit */
	if (do_deferred_writes && unhandled_stale_timer_pop && cdb_sc_normal == status)
		process_deferred_stale();
	if (cdb_sc_normal == status)
	{
		save_gv_cur_region = gv_cur_region;
		/* keep this out of the loop above so crits of all regions are released without delay */
		for (si = first_sgm_info;  NULL != si; si = si->next_sgm_info)
		{
			if (si->update_trans)
			{
				TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
				++cs_data->n_tp_updates;
				if (NULL == si->first_cw_set)
					++cs_data->n_tp_updates_duplicate;
				wcs_timer_start(si->gv_cur_region, TRUE);
			}
			if (si->backup_block_saved)
				backup_buffer_flush(si->gv_cur_region);
		}
		TP_CHANGE_REG_IF_NEEDED(save_gv_cur_region);
		return TRUE;
	} else
	{
		t_fail_hist[t_tries] = status;
		return FALSE;
	}
}

/* --------------------------------------------------------------------------------------------
 * This code is very similar to the code in gvcst_put for the non-block-split case. Any changes
 * in either place should be reflected in the other.
 * --------------------------------------------------------------------------------------------
 */

enum cdb_sc	recompute_upd_array(srch_blk_status *hist1, cw_set_element *cse)
{
	blk_segment		*bs1, *bs_ptr;
	boolean_t		new_rec;
	cache_rec_ptr_t		cr;
	char			*va;
	enum cdb_sc		status;
	gv_key			*pKey;
	int4			blk_size, blk_fill_size, cur_blk_size, blk_seg_cnt, delta ;
        int4                    n, new_rec_size, next_rec_shrink;
	int4			rec_cmpc, target_key_size;
	uint4			segment_update_array_size;
	key_cum_value		*kv, *kvhead;
	mstr			value;
	off_chain		chain1;
	rec_hdr_ptr_t		curr_rec_hdr, next_rec_hdr, rp;
	sm_uc_ptr_t		cp1, buffaddr;
	srch_blk_status		temp_srch_blk_status, *bh;
	unsigned short		rec_size;
	sgmnt_addrs		*csa;
	blk_hdr_ptr_t		old_block;

	csa = cs_addrs;
	assert(csa->now_crit && dollar_tlevel && sgm_info_ptr);
	assert(!cse->level && cse->blk_target && !cse->first_off && !cse->write_type);
	blk_size = cs_data->blk_size;
	blk_fill_size = (blk_size * gv_fillfactor) / 100 - cs_data->reserved_bytes;
	cse->blk_target->clue.end = 0;		/* nullify clues for gv_targets involved in recomputation */
	cse->first_copy = TRUE;
	bh = &temp_srch_blk_status;
	*bh = *hist1;
	if (dba_bg == csa->hdr->acc_meth)
	{	/* For BG method, modify history with uptodate cache-record, buffer and cycle information.
		 * Also modify cse->old_block and cse->ondsk_blkver to reflect the updated buffer.
		 * This is necessary in case history contains an older twin cr or a cr which has since been recycled
		 */
		cr = db_csh_get(bh->blk_num);
		assert(CR_NOTVALID != (sm_long_t)cr);
		if (NULL == cr || CR_NOTVALID == (sm_long_t)cr || 0 <= cr->read_in_progress)
			return cdb_sc_lostcr;
		bh->cr = cr;
		bh->cycle = cr->cycle;
		cse->old_block = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
		cse->ondsk_blkver = cr->ondsk_blkver;
		bh->buffaddr = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
	}
	buffaddr = bh->buffaddr;
	for (kvhead = kv = cse->recompute_list_head; kv; kv = kv->next)
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
			cse->first_copy = FALSE;
		}
		if (cdb_sc_normal != (status = gvcst_search_blk(pKey, bh)))
			return status;
		cur_blk_size = ((blk_hdr_ptr_t)buffaddr)->bsiz;
		new_rec = (target_key_size != bh->curr_rec.match);
		rp = (rec_hdr_ptr_t)(buffaddr + bh->curr_rec.offset);
		if (bh->curr_rec.offset == cur_blk_size)
		{
			if (FALSE == new_rec)
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_mkblk;
			}
			rec_cmpc = 0;
			rec_size = 0;
		} else
		{
			GET_USHORT(rec_size, &rp->rsiz);
			rec_cmpc = rp->cmpc;
			if ((sm_uc_ptr_t)rp + rec_size > (sm_uc_ptr_t)buffaddr + cur_blk_size)
			{
				assert(CDB_STAGNATE > t_tries);
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
			if (0 != rc_set_fragment)
				GTMASSERT;
			BLK_SEG(bs_ptr, buffaddr + sizeof(blk_hdr), bh->curr_rec.offset - sizeof(blk_hdr));
			BLK_ADDR(curr_rec_hdr, sizeof(rec_hdr), rec_hdr);
			curr_rec_hdr->rsiz = new_rec_size;
			curr_rec_hdr->cmpc = bh->prev_rec.match;
			BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, sizeof(rec_hdr));
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
					BLK_ADDR(next_rec_hdr, sizeof(rec_hdr), rec_hdr);
					next_rec_hdr->rsiz = rec_size - next_rec_shrink;
					next_rec_hdr->cmpc = bh->curr_rec.match;
					BLK_SEG(bs_ptr, (sm_uc_ptr_t)next_rec_hdr, sizeof(rec_hdr));
					next_rec_shrink += SIZEOF(rec_hdr);
				}
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)rp + next_rec_shrink, n - next_rec_shrink);
			}
			if (0 == BLK_FINI(bs_ptr, bs1))
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_mkblk;
			}
			cse->upd_addr = (unsigned char *)bs1;
			cse->done = FALSE;
		} else
			return cdb_sc_blksplit;
	}
	if (NULL != cse->old_block && JNL_ENABLED(csa) && csa->jnl_before_image)
	{
		old_block = (blk_hdr_ptr_t)cse->old_block;
		assert(old_block->bsiz <= csa->hdr->blk_size);
		if (old_block->tn < csa->jnl->jnl_buff->epoch_tn)
			cse->blk_checksum = jnl_get_checksum(INIT_CHECKSUM_SEED, (uint4 *)old_block, old_block->bsiz);
		else
			cse->blk_checksum = 0;
	}
	return cdb_sc_normal;
}

boolean_t	reallocate_bitmap(sgm_info *si, cw_set_element *bml_cse)
{
	bool			blk_used;
	block_id_ptr_t		b_ptr;
	block_id		bml, free_bit;
	cache_rec_ptr_t		cr;
	cw_set_element		*cse, *bmp_begin_cse;
	int4			offset;
	uint4			total_blks, map_size;
	boolean_t		read_before_image; /* TRUE if before-image journaling or online backup in progress */
	sgmnt_addrs		*csa;
	boolean_t		is_mm;
	jnl_buffer_ptr_t	jbp; /* jbp is non-NULL only if before-image journaling */
	blk_hdr_ptr_t		old_block;

	csa = cs_addrs;
	is_mm = (dba_mm == csa->hdr->acc_meth);
	bml = bml_cse->blk;
	b_ptr = (block_id_ptr_t)bml_cse->upd_addr;
	offset = 0;
	total_blks = is_mm ? csa->total_blks : csa->ti->total_blks;
	if (ROUND_DOWN2(total_blks, BLKS_PER_LMAP) == bml)
		map_size = total_blks - bml;
	else
		map_size = BLKS_PER_LMAP;
	assert(bml >= 0 && bml < total_blks);
	bmp_begin_cse = si->first_cw_bitmap;	/* stored in a local to avoid pointer de-referencing within the loop below */
	read_before_image = ((JNL_ENABLED(csa) && csa->jnl_before_image) || csa->backup_in_prog);
	jbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
	read_before_image = ((NULL != jbp) || csa->backup_in_prog);
	for (cse = si->first_cw_set;  cse != bmp_begin_cse; cse = cse->next_cw_set)
	{
		TRAVERSE_TO_LATEST_CSE(cse);
		if ((gds_t_acquired != cse->mode) || (ROUND_DOWN2(cse->blk, BLKS_PER_LMAP) != bml))
			continue;
		assert(*b_ptr == cse->blk);
		free_bit = bm_find_blk(offset, (sm_uc_ptr_t)bml_cse->old_block + sizeof(blk_hdr), map_size, &blk_used);
		if (MAP_RD_FAIL == free_bit || NO_FREE_SPACE == free_bit)
			return FALSE;
		cse->blk = bml + free_bit;
		assert(cse->blk < total_blks);
		/* re-point before-images into cse->old_block if necessary; if not available restart by returning FALSE */
		if (!blk_used || !read_before_image)
		{
			cse->old_block = NULL;
			cse->blk_checksum = 0;
		} else if (!is_mm)
		{
			cr = db_csh_get(cse->blk);
			assert(CR_NOTVALID != (sm_long_t)cr);
			if (NULL == cr || CR_NOTVALID == (sm_long_t)cr || 0 <= cr->read_in_progress)
				return FALSE;	/* if one block was freed a long time ago, most probably were; so just give up */
			cse->cr = cr;
			cse->cycle = cr->cycle;
			if (cse->old_block != (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr))
			{
				cse->old_block = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
				old_block = (blk_hdr_ptr_t)cse->old_block;
				if (NULL != jbp)
				{	/* Even though we hold crit at this point we might see invalid bsiz fields
					 * in case it is a reused block that is still in V4 format. Assert this.
					 * In pro dont take chances so treat MIN(bsiz, csa->hdr->blk_size) as the
					 * block size to unconditionally compute the checksum.
					 */
					assert((old_block->bsiz <= csa->hdr->blk_size) || csa->hdr->fully_upgraded
							&& (((v15_blk_hdr_ptr_t)old_block)->bsiz <= csa->hdr->blk_size));
					if (old_block->tn < jbp->epoch_tn)
						cse->blk_checksum = jnl_get_checksum(INIT_CHECKSUM_SEED,
								(uint4 *)old_block, MIN(old_block->bsiz, csa->hdr->blk_size));
					else
						cse->blk_checksum = 0;
				}
			}
			assert(gds_t_acquired == cse->mode);
 			assert(GDSVCURR == cse->ondsk_blkver);
		} else
		{	/* in MM, although mm_update() does not use cse->old_block, tp_tend uses it to write before-images.
			 * therefore, fix it to point to the reallocated block's buffer address
			 */
			cse->old_block = t_qread(cse->blk, (sm_int_ptr_t)&cse->cycle, &cse->cr);
			assert(GDSVCURR == cse->ondsk_blkver);	/* should have been already initialized in t_write_map */
			old_block = (blk_hdr_ptr_t)cse->old_block;
			if (NULL == old_block)
				return FALSE;
			if (NULL != jbp)
			{
				assert(old_block->bsiz <= csa->hdr->blk_size);
				if (old_block->tn < jbp->epoch_tn)
					cse->blk_checksum = jnl_get_checksum(INIT_CHECKSUM_SEED,
										(uint4 *)old_block, old_block->bsiz);
				else
					cse->blk_checksum = 0;
			}
		}
		*b_ptr++ = cse->blk;
		offset = free_bit + 1;
		if (offset >= map_size)
		{	/* If bm_find_blk is passed a hint (first arg) it assumes it is less than map_size
			 * and gives invalid results (like values >= map_size). Instead of changing bm_find_blk
			 * we do the check here and assert that "hint" < "map_size" in bm_find_blk.
			 */
			assert(offset == map_size);
			return FALSE;
		}
	}
	if (cse == bmp_begin_cse)
	{
		assert(0 == *b_ptr);
		/* since bitmap block got modified, copy latest "ondsk_blkver" status from cache-record to bml_cse */
		assert((NULL != bml_cse->cr) || is_mm);
		old_block = (blk_hdr_ptr_t)bml_cse->old_block;
		if (NULL != jbp)
		{	/* recompute CHECKSUM for the modified bitmap block before-image */
			if (old_block->tn < jbp->epoch_tn)
				bml_cse->blk_checksum = jnl_get_checksum(INIT_CHECKSUM_SEED,
									(uint4 *)old_block, old_block->bsiz);
			else
				bml_cse->blk_checksum = 0;
		}
		if (!is_mm)
			bml_cse->ondsk_blkver = bml_cse->cr->ondsk_blkver;
		return TRUE;
	} else
		return FALSE;
}
