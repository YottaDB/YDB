/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef UNIX
#include "gtm_stdio.h"
#endif
#include "gtm_time.h"
#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif
#include "gtm_string.h"

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
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "interlock.h"
#include "gdsbgtr.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "t_commit_cleanup.h"
#include "mupipbckup.h"
#include "gvcst_blk_build.h"
#include "gvcst_search_blk.h"
#include "cache.h"
#include "rc_cpt_ops.h"
#include "wcs_flu.h"
#include "jnl_write_pblk.h"
#include "jnl_write.h"
#ifdef UNIX
#include "process_deferred_stale.h"
#endif
#include "wcs_backoff.h"
#include "mm_update.h"
#include "bg_update.h"
#include "wcs_get_space.h"
#include "wcs_timer_start.h"
#include "send_msg.h"
#include "add_inter.h"

GBLREF	short			dollar_tlevel;
GBLREF	bool			update_trans;
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
GBLREF	uint4			rec_seqno, cumul_jnl_rec_len;
GBLREF	boolean_t		is_updproc;
GBLREF	seq_num			seq_num_zero;
GBLREF	seq_num			seq_num_one;
GBLREF	int			gv_fillfactor;
GBLREF	ua_list			*first_ua, *curr_ua;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	int			rc_set_fragment, update_array_size, cumul_update_array_size;
GBLREF	boolean_t		unhandled_stale_timer_pop;
GBLREF	uint4			gbl_jrec_time;	/* see comment in gbldefs.c for usage */
GBLREF	seq_num			max_resync_seqno;
DEBUG_ONLY(GBLREF uint4		cumul_index;
	   GBLREF uint4		cu_jnl_index;
	  )
GBLREF	boolean_t		copy_jnl_record;
GBLREF	struct_jrec_tcom 	mur_jrec_fixed_tcom;
GBLREF	int4			tprestart_syslog_delta;

boolean_t	reallocate_bitmap(sgm_info *si, cw_set_element *bml_cse);
enum cdb_sc	recompute_upd_array(srch_blk_status *hist1, cw_set_element *cse);

boolean_t	tp_tend(boolean_t crit_only)
{
	block_id		tp_blk;
	boolean_t		history_validated, is_mm, no_sets, was_crit, x_lock, do_deferred_writes, replication = FALSE;
	bt_rec_ptr_t		bt;
	cache_rec_ptr_t		cr;
	cw_set_element		*cse;
	file_control		*fc;
	jnl_buffer_ptr_t	jbp;
	jnl_format_buffer	*jfb;
	sgm_info		*si, *si1;
	tp_region		*tr, *tr_last;
	sgmnt_addrs		*csa, *tcsa;
	sgmnt_data_ptr_t	csd;
	srch_blk_status		*t1;
	trans_num		ctn, tnque_earliest_tn;
	trans_num		valid_thru;	/* buffers touched by this transaction will be valid thru this tn */
	enum cdb_sc		status;
	gd_region		*save_gv_cur_region;
	int			cw_depth, lcnt;
	struct_jrec_tcom	tcom_record;
	jnldata_hdr_ptr_t	jnl_header;
	int			repl_tp_region_count = 0;
	boolean_t		yes_jnl_no_repl, first_time, release_crit;
	uint4			jnl_status, leafmods, indexmods;
	uint4			total_jnl_rec_size;

	error_def(ERR_DLCKAVOIDANCE);
	error_def(ERR_JNLTRANS2BIG);

	assert(dollar_tlevel > 0);
	assert(0 == jnl_fence_ctl.level);
	status = cdb_sc_normal;
	no_sets = TRUE;
	jnl_status = 0;
	for (si = first_sgm_info;  NULL != si; si = si->next_sgm_info)
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
		if (crit_only)
			continue;
		if (NULL == si->first_cw_set  &&  si->start_tn == csa->ti->early_tn)
		{	/* read with no change to the transaction history */
			/* assure that we haven't overrun our history buffer and we have reasonable values for first and last */
			assert(si->last_tp_hist - si->first_tp_hist <= si->tp_hist_size);
			continue;
		} else
			no_sets = FALSE;
		if (JNL_ENABLED(csa))
		{	/* compute the total journal record size requirements before grab_crit().
			 * there is code later that will check for state changes from now to then
			 */
			TOTAL_TPJNL_REC_SIZE(total_jnl_rec_size, si, csa);
			/* compute current transaction's maximum journal space needs in number of disk blocks */
			si->tot_jrec_size = MAX_REQD_JNL_FILE_SIZE(total_jnl_rec_size);
		}
	}	/* for (si ... ) */
	if (no_sets && (FALSE == crit_only))
	{
		if (CDB_STAGNATE <= t_tries)
		{
			for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
				rel_crit(tr->reg);
		}
		UNIX_ONLY(
			/* Must be done after REVERT since we are no longer in crit */
			if (unhandled_stale_timer_pop)
				process_deferred_stale();
		)
		return TRUE;
	}
	/* We are still out of crit if this is not our last attempt. If so, run the region
	   list and check that we have sufficient free blocks for our update. If not, get
	   them now while we can. We will repeat this check later in crit but it will
	   hopefully have little or nothing to do. */

	/* Only unix for now -- VMS can't deal yet */
	UNIX_ONLY(
		for (si = first_sgm_info;  (cdb_sc_normal == status) && (NULL != si);  si = si->next_sgm_info)
		{
			TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
			csa = cs_addrs;
			if (csa->now_crit)  /* bypass 1st check if already in crit -- check later */
				continue;
			csd = cs_data;
			is_mm = (dba_mm == csd->acc_meth);
			if (!is_mm && (NULL != si->first_cw_set))
			{
				if (csa->nl->wc_in_free < si->cw_set_depth + 1)
				{
					if (!wcs_get_space(si->gv_cur_region, si->cw_set_depth + 1, NULL))
					{
						SET_TRACEABLE_VAR(csd->wc_blocked,TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_wcsgetspace);
						status = cdb_sc_cacheprob;
						TP_TRACE_HIST(CR_BLKEMPTY, NULL);
						t_fail_hist[t_tries] = status;
						TP_RETRY_ACCOUNTING(csd, status);
						return FALSE;
					}
				}
			}
		}
	)
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
			DEBUG_ONLY(
				if (0 != lcnt)
				{
					tcsa = &FILE_INFO(tr->reg)->s_addrs;
					BG_TRACE_ANY(tcsa, tp_crit_retries);
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
				tcsa = &FILE_INFO(tr->reg)->s_addrs;
				if ((CDB_STAGNATE > t_tries) ? tcsa->now_crit :
								(!tcsa->now_crit || (update_trans && tcsa->hdr->freeze)))
				{
					send_msg(VARLSTCNT(8) ERR_DLCKAVOIDANCE, 6, DB_LEN_STR(tr->reg),
								tcsa->ti->curr_tn, t_tries, dollar_trestart, tcsa->now_crit);
					assert(FALSE);
					status = cdb_sc_needcrit;	/* break the possible deadlock by signalling a restart */
					t_fail_hist[t_tries] = status;
					TP_RETRY_ACCOUNTING(csd, status);
					TP_TRACE_HIST(CR_BLKEMPTY, NULL);
					return FALSE;
				}
			}
			if (update_trans)
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
	 * But 2nd to 3rd retry transition doesn't occur if in the 2nd retry we get jnlstatemod or jnlclose retry code.
	 * Hence the variable release_crit to track the above.
	 */
	release_crit = (CDB_STAGNATE - 1) > t_tries;
	for (si = first_sgm_info;  (cdb_sc_normal == status) && (NULL != si);  si = si->next_sgm_info)
	{
		leafmods = indexmods = 0;
		sgm_info_ptr = si;
		TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
		csa = cs_addrs;
		csd = cs_data;
		if (JNL_ALLOWED(csa))
		{
			if ((csa->jnl_state != csd->jnl_state) || (csa->jnl_before_image != csd->jnl_before_image))
			{
				for (si = first_sgm_info;  NULL != si;  si = si->next_sgm_info)
				{
					TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
					cs_addrs->jnl_state = cs_data->jnl_state;
					cs_addrs->jnl_before_image = cs_data->jnl_before_image;
				}
				status = cdb_sc_jnlstatemod;
				if ((CDB_STAGNATE - 1) == t_tries)
					release_crit = TRUE;
				goto failed;
			}
			if (JNL_ENABLED(csa))
			{	/* check if current transaction's journal size needs are greater than max jnl file size */
				if (si->tot_jrec_size > csd->autoswitchlimit)
				{	/* can't fit in current transaction's journal records into one journal file */
					rts_error(VARLSTCNT(6) ERR_JNLTRANS2BIG, 4, si->tot_jrec_size,
							JNL_LEN_STR(csd), csd->autoswitchlimit);
				}
			}
		}
		is_mm = (dba_mm == csd->acc_meth);
		if (!is_mm)
			tnque_earliest_tn = ((th_rec_ptr_t)((sm_uc_ptr_t)csa->th_base + csa->th_base->tnque.fl))->tn;
		if (!is_mm && (NULL != si->first_cw_set))
		{
			if (csa->nl->wc_in_free < si->cw_set_depth + 1)
			{
				if (!wcs_get_space(si->gv_cur_region, si->cw_set_depth + 1, NULL))
				{
					assert(FALSE);
					SET_TRACEABLE_VAR(csd->wc_blocked,TRUE);
					BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_wcsgetspace);
					status = cdb_sc_cacheprob;
					TP_RETRY_ACCOUNTING(csd, status);
					TP_TRACE_HIST(CR_BLKEMPTY, NULL);
					history_validated = FALSE;
					continue;
				}
			}
			VMS_ONLY(
				if (csd->clustered  && !CCP_SEGMENT_STATE(csa->nl, CCST_MASK_HAVE_DIRTY_BUFFERS))
				{
					CCP_FID_MSG(si->gv_cur_region, CCTR_FLUSHLK);
					ccp_userwait(si->gv_cur_region, CCST_MASK_HAVE_DIRTY_BUFFERS, NULL, csa->nl->ccp_cycle);
				}
			)
		}
		/* the following section verifies that the optimistic concurrency was justified */
		history_validated = TRUE;
		if (tprestart_syslog_delta)
			n_blkmods = n_pvtmods = 0;
		for (t1 = si->first_tp_hist;  t1 != si->last_tp_hist; t1++)
		{
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
						TP_RETRY_ACCOUNTING(csd, status);
						TP_TRACE_HIST(t1->blk_num, tp_get_target(t1->buffaddr));
						BREAK_IN_PRO__CONTINUE_IN_DBG;
					}
				}
			} else
			{
				bt = bt_get(t1->blk_num);
				if (NULL != bt)
				{
					assert(t1->blk_target);
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
							TP_RETRY_ACCOUNTING(csd, status);
							TP_TRACE_HIST_MOD(t1->blk_num, tp_get_target(t1->buffaddr),
									tp_blkmod_tp_tend, csd, t1->tn, bt->tn, t1->level);
							BREAK_IN_PRO__CONTINUE_IN_DBG;
						}
					}
				} else if (t1->tn <= tnque_earliest_tn)
				{
					assert(CDB_STAGNATE > t_tries);
					status = cdb_sc_losthist;
					TP_RETRY_ACCOUNTING(csd, status);
					TP_TRACE_HIST(t1->blk_num, tp_get_target(t1->buffaddr));
					BREAK_IN_PRO__CONTINUE_IN_DBG;
				}
				assert(CYCLE_PVT_COPY != t1->cycle);
				if (t1->ptr)
				{	/* do cycle check only if blk has cse and hasn't been built or we have BI journaling.
					 * The BI journaling check is to ensure that the PBLK we write hasn't been recycled.
					 */
					if ((NULL == bt) || (CR_NOTVALID == bt->cache_index))
						cr = db_csh_get(t1->blk_num);
					else
					{
						cr = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
						if (cr && (cr->blk != bt->blk))
						{
							SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
							GTMASSERT;
						}
					}
					if ((cache_rec_ptr_t)CR_NOTVALID == cr)
					{
						SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_t1);
						status = cdb_sc_cacheprob;
						TP_RETRY_ACCOUNTING(csd, status);
						TP_TRACE_HIST(t1->blk_num, tp_get_target(t1->buffaddr));
						break;
					}
					if (!t1->ptr->new_buff || (JNL_ENABLED(csa) && csa->jnl_before_image))
					{
						if ((NULL == cr) || (cr->cycle != t1->cycle) ||
							((sm_long_t)GDS_ANY_REL2ABS(csa, cr->buffaddr) != (sm_long_t)t1->buffaddr))
						{
							if (cr && bt &&(cr->blk != bt->blk))
							{
								SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
								GTMASSERT;
							}
							assert(CDB_STAGNATE > t_tries);
							status = cdb_sc_lostcr;
							TP_RETRY_ACCOUNTING(csd, status);
							TP_TRACE_HIST(t1->blk_num, NULL == cr ? NULL : tp_get_target(t1->buffaddr));
							BREAK_IN_PRO__CONTINUE_IN_DBG;
						}
					}
					/* The only case cr can be NULL at this point of code is when
					 *	t1->ptr->new_buff is non-NULL and the block is not in the cache AND
					 *	we don't have before-image journaling. In this case bg_update will
					 *	do a db_csh_getn() and appropriately set in_cw_set field to be TRUE
					 *	so we shouldn't be manipulating those fields in that case.
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
		if (DIVIDE_ROUND_UP(si->num_of_blks, 4) < leafmods)	/* if status == cdb_sc_normal, then leafmods  */
			status = cdb_sc_toomanyrecompute;		/* is exactly the number of recomputed blocks */
		/* Check bit maps for usage */
		if (cdb_sc_normal == status)
		{
			if (NULL == si->first_cw_set)
				continue;
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
						TP_RETRY_ACCOUNTING(csd, status);
						TP_TRACE_HIST(cse->blk, NULL);
						break;
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
							TP_RETRY_ACCOUNTING(csd, status);
							TP_TRACE_HIST(cse->blk, NULL);
							break;
						}
					} else if (cse->tn <= tnque_earliest_tn)
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_lostbmlhist;
						TP_RETRY_ACCOUNTING(csd, status);
						TP_TRACE_HIST(cse->blk, NULL);
						break;
					}
					TRAVERSE_TO_LATEST_CSE(cse);
					assert((NULL == cse) || (NULL == cse->new_buff));
					if ((NULL == bt) || (CR_NOTVALID ==  bt->cache_index))
					{
						cr = db_csh_get(tp_blk);
						if ((cache_rec_ptr_t)CR_NOTVALID == cr)
						{
							status = cdb_sc_cacheprob;
							TP_RETRY_ACCOUNTING(csd, status);
							TP_TRACE_HIST(cse->blk, cse->blk_target);
							SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_bitmap);
							break;
						}
					} else
					{
						cr = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
						if (cr->blk != bt->blk)
						{
							SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
							GTMASSERT;
						}
					}
					if ((NULL == cr) || (cr->cycle != cse->cycle) ||
						((sm_long_t)GDS_ANY_REL2ABS(csa, cr->buffaddr) != (sm_long_t)cse->old_block))
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_lostbmlcr;
						TP_RETRY_ACCOUNTING(csd, status);
						TP_TRACE_HIST(cse->blk, NULL);
						break;
					}
					assert(si->cr_array_index < si->cr_array_size);
					assert(0 <= si->cr_array_index);
					si->cr_array[si->cr_array_index++] = cr;
					assert(FALSE == cr->in_cw_set);
					cr->in_cw_set = TRUE;
					cr->refer = TRUE;
				}
			}	/* for (all bitmaps written) */
		} else		/* if (cdb_sc_normal == status) */
			history_validated = FALSE;
	} /* for (si ... ) */
	for (si = first_sgm_info; (cdb_sc_normal == status) && (NULL != si); si = si->next_sgm_info)
	{
		if (NULL == si->first_cw_set)
			continue;
		assert(0 != si->cw_set_depth);
		sgm_info_ptr = si;
		TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
		csa = cs_addrs;
		csd = cs_data;
		is_mm = dba_mm == csa->hdr->acc_meth;
		ctn = csa->ti->curr_tn;
		yes_jnl_no_repl = FALSE;
		if (JNL_ENABLED(csa))
		{
			if (csa->jnl_before_image)
			{
				for (cse = si->first_cw_set;  NULL != cse;  cse = cse->next_cw_set)
				{
					TRAVERSE_TO_LATEST_CSE(cse);
					if (!is_mm && gds_t_acquired == cse->mode && (NULL != cse->old_block))
					{
						assert(CYCLE_PVT_COPY != cse->cycle);
						cr = db_csh_get(cse->blk);
						if ((NULL == cr) || ((cache_rec_ptr_t)CR_NOTVALID == cr)
							|| (cr->cycle != cse->cycle))
						{	/* lost old block copy (read before crit) */
							TP_TRACE_HIST(cse->blk, cse->blk_target);
							if ((cache_rec_ptr_t)CR_NOTVALID == cr)
							{
								status = cdb_sc_cacheprob;
								SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
								BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_jnl_cwset);
							} else
							{
								assert(CDB_STAGNATE > t_tries);
								status = cdb_sc_lostbefor;
							}
							TP_RETRY_ACCOUNTING(csd, status);
							break;
						}
						assert(si->cr_array_index < si->cr_array_size);
						assert(0 <= si->cr_array_index);
						si->cr_array[si->cr_array_index++] = cr;
						assert(FALSE == cr->in_cw_set);
						cr->in_cw_set = TRUE;
						cr->refer = TRUE;
					}
				}
			}
			if (REPL_ENABLED(csa))
			{
				replication = TRUE;
				repl_tp_region_count++;
			} else
				yes_jnl_no_repl = TRUE;
		}
		if (repl_tp_region_count && yes_jnl_no_repl)
			GTMASSERT;
	}
	for (si = first_sgm_info; NULL != si; si = si->next_sgm_info)
		si->backup_block_saved = FALSE;
	/* the following section is the actual commitment of the changes */
	if (cdb_sc_normal == status)
	{
		if (TRUE == replication)
		{
			grab_lock(jnlpool.jnlpool_dummy_reg);
			QWASSIGN(temp_jnlpool_ctl->write_addr, jnlpool_ctl->write_addr);
			temp_jnlpool_ctl->write = jnlpool_ctl->write;
			QWASSIGN(temp_jnlpool_ctl->jnl_seqno, jnlpool_ctl->jnl_seqno);
			INT8_ONLY(assert(temp_jnlpool_ctl->write == temp_jnlpool_ctl->write_addr % temp_jnlpool_ctl->jnlpool_size);)
			temp_jnlpool_ctl->write += sizeof(jnldata_hdr_struct);
			if (temp_jnlpool_ctl->write >= temp_jnlpool_ctl->jnlpool_size)
			{
				assert(temp_jnlpool_ctl->write == temp_jnlpool_ctl->jnlpool_size);
				temp_jnlpool_ctl->write = 0;
			}
			cumul_jnl_rec_len += TCOM_RECLEN * repl_tp_region_count + sizeof(jnldata_hdr_struct);
			DEBUG_ONLY(cumul_index += repl_tp_region_count;)
			assert(cumul_jnl_rec_len % JNL_REC_START_BNDRY == 0);
			assert(QWEQ(jnlpool_ctl->early_write_addr, jnlpool_ctl->write_addr));
			QWADDDW(jnlpool_ctl->early_write_addr, jnlpool_ctl->write_addr, cumul_jnl_rec_len);
		}
		first_time = TRUE;
		for (si = first_sgm_info; NULL != si; si = si->next_sgm_info)
		{
			if (NULL == si->first_cw_set)
				continue;
			assert(0 != si->cw_set_depth);
			sgm_info_ptr = si;
			TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
			csa = cs_addrs;
			csd = cs_data;
			is_mm = dba_mm == csa->hdr->acc_meth;
			ctn = csa->ti->curr_tn;
			assert(ctn == csa->ti->early_tn);
			csa->ti->early_tn = ctn + 1;
			if (JNL_ENABLED(csa))
			{
				if (first_time)
				{
					JNL_SHORT_TIME(gbl_jrec_time);
					first_time = FALSE;
				}
				jnl_status = jnl_ensure_open();
				if (jnl_status == 0)
				{
					jbp = csa->jnl->jnl_buff;
					/* si->tmp_cw_set_depth is a copy of si->cw_set_depth at the time of
					 * 	TOTAL_TPJNL_REC_SIZE calculation;
					 * ensure it has not changed until now when the actual jnl record write occurs.
					 * same case with csa->jnl_before_images & jbp->before_images.
					 */
					assert(si->cw_set_depth == si->tmp_cw_set_depth);
					assert(jbp->before_images == csa->jnl_before_image);
					if (DISK_BLOCKS_SUM(jbp->freeaddr, si->total_jnl_rec_size) > jbp->filesize)
					{	/* Moved here to prevent jnlrecs split across multiple generation journal files. */
						jnl_flush(csa->jnl->region);
						if (-1 == jnl_file_extend(csa->jnl, si->total_jnl_rec_size))
						{
							assert((!JNL_ENABLED(csd)) && JNL_ENABLED(csa));
							status = t_fail_hist[t_tries] = cdb_sc_jnlclose;
							TP_TRACE_HIST(CR_BLKEMPTY, NULL);
							t_commit_cleanup(status, 0);
							if ((CDB_STAGNATE - 1) == t_tries)
								release_crit = TRUE;
							goto failed;
						}
					}
					/* jnl_put_jrt_pini will use gbl_jrec_time to fill in the time of the pini record.
					 * see comment in jnl_put_pini.c about difference between tp_tend and t_end in this regard.
					 */
					if (0 == csa->jnl->pini_addr)
						jnl_put_jrt_pini(csa);
					if (jbp->before_images)
					{
						if ((cdb_sc_normal == status) && (jbp->next_epoch_time <= gbl_jrec_time))
						{
							/* Flush the cache. Since we are in crit, defer syncing the epoch */
							if (!wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH))
							{
								SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
								BG_TRACE_PRO_ANY(csa, wc_blocked_tp_tend_jnl_wcsflu);
								status = t_fail_hist[t_tries] = cdb_sc_cacheprob;
								TP_RETRY_ACCOUNTING(csd, status);
								TP_TRACE_HIST(CR_BLKEMPTY, NULL);
								t_commit_cleanup(status, 0);
								goto failed;
							}
						}
						for (cse = si->first_cw_set;  NULL != cse;  cse = cse->next_cw_set)
						{	/* Write out before-update journal image records */
							TRAVERSE_TO_LATEST_CSE(cse);
							ASSERT_IS_WITHIN_SHM_BOUNDS(cse->old_block, csa);
							DBG_ENSURE_OLD_BLOCK_IS_VALID(cse, is_mm, csa, csd);
							if ((kill_t_create == cse->mode) || (kill_t_write == cse->mode) ||
								(kill_t_write_root == cse->mode) || (kill_t_writemap == cse->mode))
								continue;
							if ((NULL != cse->old_block) &&
								(((blk_hdr_ptr_t)(cse->old_block))->tn < jbp->epoch_tn))
							{
								jnl_write_pblk(csa, cse->blk, (blk_hdr_ptr_t)cse->old_block);
								cse->jnl_freeaddr = jbp->freeaddr;
							} else
								cse->jnl_freeaddr = 0;
						}
					}
					if (jnl_fence_ctl.region_count != 0)
						++jnl_fence_ctl.region_count;
					for (jfb = si->jnl_head;  NULL != jfb; jfb = jfb->next)
							jnl_write_logical(csa, jfb);
				} else
				{
					csa->ti->early_tn = ctn;
					rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
				}
			}	/* if (journaling) */
			csa->prev_free_blks = csa->ti->free_blocks;
			csa->t_commit_crit = TRUE;
			if (csd->dsid && tp_kill_bitmaps)
				rc_cpt_inval();
			for (cse = si->first_cw_set;  NULL != cse;  cse = cse->next_cw_set)
			{
				TRAVERSE_TO_LATEST_CSE(cse);
				if ((kill_t_create == cse->mode) || (kill_t_write == cse->mode) ||
					(kill_t_write_root == cse->mode) || (kill_t_writemap == cse->mode))
					continue;
				if (csd->dsid && !tp_kill_bitmaps && (0 == cse->level))
				{
					assert(!is_mm);
					rc_cpt_entry(cse->blk);
				}
				status = is_mm ? mm_update(cse, NULL, ctn, ctn, si)
					       : bg_update(cse, NULL, ctn, ctn, si);
				if (cdb_sc_normal != status)
				{
					assert(FALSE);		/* the database is probably in trouble */
					if (TRUE == replication)
					{
						QWINCRBYDW(jnlpool_ctl->write_addr, jnlpool_ctl->jnlpool_size); /* refresh hist */
						rel_lock(jnlpool.jnlpool_dummy_reg);
					}
					TP_TRACE_HIST(cse->blk, cse->blk_target);
					if (cdb_sc_cacheprob != status)
						t_commit_cleanup(status, 0);
					goto failed;
				}
				cse->mode = gds_t_committed;
			}
			csa->t_commit_crit = FALSE;
			++csa->ti->curr_tn;
			assert(csa->ti->curr_tn == csa->ti->early_tn);
			/* write out the db header every HEADER_UPDATE_COUNT -1 transactions */
			if (!(csa->ti->curr_tn & (HEADER_UPDATE_COUNT - 1)))
				fileheader_sync(si->gv_cur_region);
			if (NULL != si->kill_set_head)
				INCR_KIP(csd, csa, si->kip_incremented);
		} /* for (si ... ) */
		/* the next section marks the transaction complete in the journal */
		if (!copy_jnl_record) /* Update process should operate on GLD, and can be different */
		{
			tcom_record.participants = jnl_fence_ctl.region_count;
			QWASSIGN(tcom_record.jnl_seqno, seq_num_zero);
		} else
		{
			tcom_record.participants = mur_jrec_fixed_tcom.participants;
			QWASSIGN(tcom_record.jnl_seqno, mur_jrec_fixed_tcom.jnl_seqno);
		}
		QWASSIGN(tcom_record.token, jnl_fence_ctl.token);
		if (replication)
		{
			QWINCRBY(temp_jnlpool_ctl->jnl_seqno, seq_num_one);
			if (is_updproc)
				QWINCRBY(max_resync_seqno, seq_num_one);
		}
		/* Note that only those regions that are actively journaling will appear in the following list: */
		for (csa = jnl_fence_ctl.fence_list;  (sgmnt_addrs *) - 1 != csa;  csa = csa->next_fenced)
		{
			assert(((sgm_info *)(csa->sgm_info_ptr))->first_cw_set);
			tcom_record.tn = csa->ti->curr_tn - 1;
			if (is_updproc)
			{	/* recov_short_time for update process means the time at which update was done on primary */
				tcom_record.tc_recov_short_time = mur_jrec_fixed_tcom.tc_recov_short_time;
				tcom_record.ts_recov_short_time = mur_jrec_fixed_tcom.ts_recov_short_time;
			} else
			{
				tcom_record.tc_recov_short_time = gbl_jrec_time;
				tcom_record.ts_recov_short_time = gbl_jrec_time;
			}
			tcom_record.pini_addr = csa->jnl->pini_addr;
			if (!copy_jnl_record)
			{
				tcom_record.ts_short_time = tcom_record.tc_short_time = gbl_jrec_time;
				tcom_record.rec_seqno = rec_seqno;
			} else
			{
				tcom_record.tc_short_time = mur_jrec_fixed_tcom.tc_short_time;
				tcom_record.ts_short_time = mur_jrec_fixed_tcom.ts_short_time;
				tcom_record.rec_seqno = mur_jrec_fixed_tcom.rec_seqno;
			}
			if (REPL_ENABLED(csa->hdr))
			{
				QWASSIGN(csa->hdr->reg_seqno, temp_jnlpool_ctl->jnl_seqno);
				if (is_updproc)
					QWASSIGN(csa->hdr->resync_seqno, max_resync_seqno);
				QWASSIGN(tcom_record.jnl_seqno, jnlpool_ctl->jnl_seqno);
			}
			TP_CHANGE_REG_IF_NEEDED(csa->jnl->region);
			jnl_write(csa->jnl, JRT_TCOM, (jrec_union *)&tcom_record, NULL, NULL);
		}
		if (replication)
		{
			assert(cumul_index == cu_jnl_index);
			assert((jnlpool_ctl->write + cumul_jnl_rec_len) % jnlpool_ctl->jnlpool_size == temp_jnlpool_ctl->write);
			assert(QWGT(jnlpool_ctl->early_write_addr, jnlpool_ctl->write_addr));
			jnl_header = (jnldata_hdr_ptr_t)(jnlpool.jnldata_base + jnlpool_ctl->write);	/* Begin atomic stmnts */
			jnl_header->jnldata_len = cumul_jnl_rec_len;
			jnl_header->prev_jnldata_len = jnlpool_ctl->lastwrite_len;
			jnlpool_ctl->lastwrite_len = jnl_header->jnldata_len;
			QWINCRBYDW(jnlpool_ctl->write_addr, jnl_header->jnldata_len);
			jnlpool_ctl->write = temp_jnlpool_ctl->write;
			QWASSIGN(jnlpool_ctl->jnl_seqno, temp_jnlpool_ctl->jnl_seqno);			/* End atomic stmnts */
			assert(QWEQ(jnlpool_ctl->early_write_addr, jnlpool_ctl->write_addr));
			rel_lock(jnlpool.jnlpool_dummy_reg);
		}
	}	/* if (cdb_sc_normal == status) */
	/* Caution: followthrough, cleanup for normal and abnormal "status" */
failed:
	for (si = first_sgm_info;  NULL != si; si = si->next_sgm_info)
	{
		while (si->cr_array_index > 0)
			si->cr_array[--si->cr_array_index]->in_cw_set = FALSE;
		if (history_validated)
		{
			/* reset transaction numbers in the gv_target histories so they
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
	do_deferred_writes = FALSE;
	if ((cdb_sc_normal == status) || release_crit)
	{
		/* Release regions  */
		for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
			rel_crit(tr->reg);
		for (si = first_sgm_info;  NULL != si; si = si->next_sgm_info)
		{
			if (si->backup_block_saved)
				backup_buffer_flush(si->gv_cur_region);
		}
		do_deferred_writes = TRUE;
	}
	REVERT;
	UNIX_ONLY(
		/* Must be done after REVERT since we are no longer in crit */
		if (do_deferred_writes && unhandled_stale_timer_pop && cdb_sc_normal == status)
			process_deferred_stale();
	)
	if (cdb_sc_normal == status)
	{
		save_gv_cur_region = gv_cur_region;
		/* keep this out of the loop above so crits of all regions are released without delay */
		for (si = first_sgm_info;  NULL != si; si = si->next_sgm_info)
		{
			if (NULL != si->first_cw_set)
			{
				TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
				wcs_timer_start(si->gv_cur_region, TRUE);
			}
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
	blk_segment	*bs1, *bs_ptr;
	boolean_t	new_rec;
	cache_rec_ptr_t cr;
	char		*va;
	enum cdb_sc	status;
	gv_key		*pKey;
	int4		blk_size, blk_fill_size, cur_blk_size, blk_seg_cnt, delta, n, new_rec_size, next_rec_shrink;
	int4		rec_cmpc, segment_update_array_size, target_key_size;
	key_cum_value	*kv, *kvhead;
	mstr		value;
	off_chain	chain1;
	rec_hdr_ptr_t	curr_rec_hdr, next_rec_hdr, rp;
	sm_uc_ptr_t	cp1, buffaddr;
	srch_blk_status temp_srch_blk_status, *bh;
	unsigned short	rec_size;

	assert(cs_addrs->now_crit && dollar_tlevel && sgm_info_ptr);
	assert(!cse->level && cse->blk_target && !cse->first_off && !cse->write_type);
	blk_size = cs_data->blk_size;
	blk_fill_size = (blk_size * gv_fillfactor) / 100 - cs_data->reserved_bytes;
	cse->blk_target->clue.end = 0;		/* nullify clues for gv_targets involved in recomputation */
	cse->first_copy = TRUE;
	bh = &temp_srch_blk_status;
	*bh = *hist1;
	if (dba_bg == cs_addrs->hdr->acc_meth)
	{	/* For BG method, modify history with uptodate cache-record, buffer and cycle information.
		 * This is necessary in case history contains an older twin cr or a cr which has since been recycled
		 */
		cr = db_csh_get(bh->blk_num);
		assert(CR_NOTVALID != (sm_long_t)cr);
		if (NULL == cr || CR_NOTVALID == (sm_long_t)cr || 0 <= cr->read_in_progress)
			return cdb_sc_lostcr;
		bh->cr = cr;
		bh->cycle = cr->cycle;
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
			new_rec_size = sizeof(rec_hdr) + target_key_size - bh->prev_rec.match + value.len;
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
			new_rec_size = sizeof(rec_hdr) + (target_key_size - rec_cmpc) + value.len;
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
			n = cur_blk_size - ((sm_uc_ptr_t)rp - buffaddr);
			if (n > 0)
			{
				if (new_rec)
				{
					BLK_ADDR(next_rec_hdr, sizeof(rec_hdr), rec_hdr);
					next_rec_hdr->rsiz = rec_size - next_rec_shrink;
					next_rec_hdr->cmpc = bh->curr_rec.match;
					BLK_SEG(bs_ptr, (sm_uc_ptr_t)next_rec_hdr, sizeof(rec_hdr));
					next_rec_shrink += sizeof(rec_hdr);
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
	return cdb_sc_normal;
}

boolean_t	reallocate_bitmap(sgm_info *si, cw_set_element *bml_cse)
{
	bool		blk_used;
	block_id_ptr_t	b_ptr;
	block_id	bml, free_bit;
	cache_rec_ptr_t cr;
	cw_set_element	*cse;
	int4		offset;
	uint4		total_blks, map_size;

	bml = bml_cse->blk;
	b_ptr = (block_id_ptr_t)bml_cse->upd_addr;
	offset = 0;
	total_blks = (dba_mm == cs_addrs->hdr->acc_meth) ? cs_addrs->total_blks : cs_addrs->ti->total_blks;
	if (ROUND_DOWN2(total_blks, BLKS_PER_LMAP) == bml)
		map_size = total_blks - bml;
	else
		map_size = BLKS_PER_LMAP;
	assert(bml >= 0 && bml < total_blks);
	for (cse = si->first_cw_set;  cse != si->first_cw_bitmap;  cse = cse->next_cw_set)
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
		if (!blk_used || !JNL_ENABLED(cs_addrs) || !cs_addrs->jnl_before_image)
			cse->old_block = NULL;
		else
		{
			cr = db_csh_get(cse->blk);
			assert(CR_NOTVALID != (sm_long_t)cr);
			if (NULL == cr || CR_NOTVALID == (sm_long_t)cr || 0 <= cr->read_in_progress)
				return FALSE;	/* if one block was freed a long time ago, most probably were; so just give up */
			cse->old_block = (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr);
			cse->cr = cr;
			cse->cycle = cr->cycle;
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
	if (cse == si->first_cw_bitmap)
	{
		assert(0 == *b_ptr);
		return TRUE;
	} else
		return FALSE;
}
