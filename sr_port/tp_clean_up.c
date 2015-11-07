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

#include <signal.h>             /* for VSIG_ATOMIC_T type */

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "copy.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h and cws_insert.h */
#include "tp.h"
#include "tp_change_reg.h"
#include "cws_insert.h"		/* for cw_stagnate_reinitialized */
#include "gdsblkops.h"		/* for RESET_UPDATE_ARRAY macro */
#include "error.h"
#include "have_crit.h"
#include "min_max.h"
#ifdef GTM_TRIGGER
#include <rtnhdr.h>
#include "gv_trigger.h"		/* for TP_INVALIDATE_TRIGGER_CYCLES_IF_NEEDED macro */
#endif

GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	ua_list			*curr_ua, *first_ua;
GBLREF	uint4			dollar_trestart;
GBLREF	sgm_info		*first_tp_si_by_ftok; /* List of participating regions in the TP transaction sorted on ftok order */
GBLREF	jnl_gbls_t		jgbl;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	trans_num		local_tn;
GBLREF	global_tlvl_info	*global_tlvl_info_head;
GBLREF	buddy_list		*global_tlvl_info_list;
GBLREF	block_id		gtm_tp_allocation_clue;	/* block# hint to start allocation for created blocks in TP */
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_namehead		*gv_target_list, *gvt_tp_list;
GBLREF	sgm_info		*sgm_info_ptr, *first_sgm_info;
GBLREF	int			process_exiting;
GBLREF	block_id		tp_allocation_clue;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	uint4			update_array_size, cumul_update_array_size;
#ifdef VMS
GBLREF	boolean_t		tp_has_kill_t_cse; /* cse->mode of kill_t_write or kill_t_create got created in this transaction */
#endif
#ifdef DEBUG
GBLREF	unsigned int		t_tries;
#endif

error_def(ERR_MEMORY);
error_def(ERR_VMSMEMORY);

void	tp_clean_up(boolean_t rollback_flag)
{
	gv_namehead	*gvnh, *blk_target;
	sgm_info	*si, *next_si;
	kill_set	*ks;
	cw_set_element	*cse, *cse1;
	int		level;
	int4		depth;
	uint4		tmp_update_array_size;
	off_chain	chain1;
	ua_list		*next_ua, *tmp_ua;
	srch_blk_status	*t1;
	boolean_t       is_mm;
	sgmnt_addrs	*csa;
	block_id	cseblk, histblk;
	cache_rec_ptr_t	cr;
	int4		upd_trans;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* We are about to clean up structures. Defer MUPIP STOP/signal handling until function end. */
	DEFER_INTERRUPTS(INTRPT_IN_TP_CLEAN_UP);

	assert((NULL != first_sgm_info) || (0 == cw_stagnate.size) || cw_stagnate_reinitialized);
		/* if no database activity, cw_stagnate should be uninitialized or reinitialized */
	DEBUG_ONLY(
		if (rollback_flag)
			TREF(donot_commit) = FALSE;
		assert(!TREF(donot_commit));
	)
	if (NULL != first_sgm_info)
	{	/* It is possible that first_ua is NULL at this point due to a prior call to tp_clean_up() that failed in
		 * malloc() of tmp_ua->update_array. This is possible because we might have originally had two chunks of
		 * update_arrays each x-bytes in size and we freed them up and requested 2x-bytes of contiguous storage
		 * and we might error out on that malloc attempt (though this is very improbable).
		 */
		if ((NULL != first_ua) && (NULL != first_ua->next_ua)
		    && !process_exiting && (UNIX_ONLY(ERR_MEMORY) VMS_ONLY(ERR_VMSMEMORY) != error_condition))
		{	/* if the original update array was too small, make a new larger one */
			/* tmp_update_array_size is used below instead of the global variables (update_array_size,
			 * first_ua->update_array_size or cumul_update_array_size) to handle error returns from	malloc()
			 * The global variables are reset to represent a NULL update_array before the malloc. If the malloc
			 * succeeds, they will be assigned the value of tmp_update_array_size and otherwise (if malloc fails
			 * due to memory exhausted situation) they stay NULL which is the right thing to do.
			 */
			update_array_size = 0;
			for (curr_ua = first_ua, tmp_update_array_size = 0; curr_ua != NULL; curr_ua = next_ua)
			{
				next_ua = curr_ua->next_ua;
				/* curr_ua->update_array can be NULL in case we got an error in the ENSURE_UPDATE_ARRAY_SPACE
				 * macro while trying to do the malloc of the update array. Since tp_clean_up() is called in
				 * most exit handling code, it has to be very careful, hence the checks for non-NULLness below.
				 */
				if (NULL != curr_ua->update_array)
				{
					free(curr_ua->update_array);
					tmp_update_array_size += curr_ua->update_array_size;
						/* add up only those update arrays that have been successfully malloced */
				}
				if (curr_ua != first_ua)
					free(curr_ua);
			}
			assert(tmp_update_array_size == cumul_update_array_size);
			tmp_ua = first_ua;
			curr_ua = first_ua = NULL;	/* reset to indicate no update-array temporarily */
			if (NULL != tmp_ua)
			{
				tmp_ua->next_ua = NULL;
				tmp_ua->update_array = update_array = update_array_ptr = NULL;
				tmp_ua->update_array_size = cumul_update_array_size = 0;
				if (BIG_UA < tmp_update_array_size)
					tmp_update_array_size = BIG_UA;
				tmp_ua->update_array = (char *)malloc(tmp_update_array_size);
				/* assign global variables only after malloc() succeeds */
				update_array = tmp_ua->update_array;
				cumul_update_array_size = update_array_size = tmp_ua->update_array_size = tmp_update_array_size;
				curr_ua = first_ua = tmp_ua; /* set first_ua to non-NULL value once all mallocs are successful */
			}
		}
		RESET_UPDATE_ARRAY; /* do not use CHECK_AND_RESET_UPDATE_ARRAY since we are in TP and will fail the check there */
		if (rollback_flag) 		/* Rollback invalidates clues in all targets used by this transaction */
		{
			for (gvnh = gvt_tp_list; NULL != gvnh; gvnh = gvnh->next_tp_gvnh)
			{
				assert(gvnh->read_local_tn == local_tn);
				gvnh->clue.end = 0;
				chain1 = *(off_chain *)&gvnh->root;
				if (chain1.flag)
				{
					DEBUG_ONLY(csa = gvnh->gd_csa;)
					assert(csa->dir_tree != gvnh);
					gvnh->root = 0;
				}
				/* Cleanup any block-split info (of created block #) in gvtarget histories */
				TP_CLEANUP_GVNH_SPLIT_IF_NEEDED(gvnh, 0);
			}
			GTMTRIG_ONLY(TP_INVALIDATE_TRIGGER_CYCLES_IF_NEEDED(FALSE, FALSE));
#			ifdef DEBUG
			if (!process_exiting)
			{	/* Ensure that we did not miss out on resetting clue for any gvtarget.
				 * Dont do this if the process is cleaning up the TP transaction as part of exit handling
				 * as the tp_clean_up invocation could be due to an interrupt (MUPIP STOP etc.) and we cannot
				 * be sure what state the mainline code was when it was interrupted. Thankfully, the clue
				 * will be used only as part of the next transaction. Since the process is in the process of
				 * exiting, the clue will never be used so it is ok for it to be non-zero in that case.
				 */
				for (gvnh = gv_target_list; NULL != gvnh; gvnh = gvnh->next_gvnh)
				{
					assert((gvnh->read_local_tn != local_tn) || (0 == gvnh->clue.end));
					chain1 = *(off_chain *)&gvnh->root;
					assert(!chain1.flag);	/* Also assert that all gvts in this process have valid root blk */
				}
			}
#			endif
			local_tn++;	/* to effectively invalidate first_tp_srch_status of all gv_targets */
			tp_allocation_clue = gtm_tp_allocation_clue;	/* Reset clue to what it was at beginning of transaction */
		} else
		{
			GTMTRIG_ONLY(TP_INVALIDATE_TRIGGER_CYCLES_IF_NEEDED(FALSE, TRUE));
			gtm_tp_allocation_clue = tp_allocation_clue;	/* Update tp allocation clue for next transaction to skip
									 * past values used in this transaction now that this one
									 * is successfully committed.
									 */
		}
		GTMTRIG_ONLY(assert(!TREF(gvt_triggers_read_this_tn));)
		GTMTRIG_ONLY(TP_ASSERT_ZTRIGGER_CYCLE_RESET;) /* for all regions, we better have csa->db_dztrigger_cycle = 0*/
		for (si = first_sgm_info;  si != NULL;  si = next_si)
		{
			TP_TEND_CHANGE_REG(si);
			upd_trans = si->update_trans;	/* copy in local for debugging purposes in case later asserts fail */
			if (upd_trans)
			{
				if (NULL != (ks = si->kill_set_head))
				{
					FREE_KILL_SET(ks);
					si->kill_set_tail = NULL;
					si->kill_set_head = NULL;
				}
				if (NULL != si->jnl_head)
				{
					REINITIALIZE_LIST(si->format_buff_list);
					REINITIALIZE_LIST(si->jnl_list);		/* reinitialize the jnl buddy_list */
					si->jnl_tail = &si->jnl_head;
					si->jnl_head = NULL;
				}
				/* Note that cs_addrs->next_fenced could be non-NULL not just for those regions with a non-NULL
				 * value of si->jnl_head but also for those regions where an INCTN record (with opcode
				 * inctn_tp_upd_no_logical_rec) was written. So reset cs_addrs->next_fenced unconditionally.
				 */
				cs_addrs->next_fenced = NULL;
				if (FALSE == rollback_flag)
				{	/* Non-rollback case (op_tcommit) validates clues in the targets we are updating */
					sgm_info_ptr = si;	/* for tp_get_cw to work */
					is_mm = (dba_mm == gv_cur_region->dyn.addr->acc_meth);
					for (cse = si->first_cw_set; cse != si->first_cw_bitmap; cse = cse->next_cw_set)
					{
						assert(0 < cse->old_mode); /* assert that phase2 is complete on this block */
						if (n_gds_t_op < cse->old_mode)
						{	/* cse's block no longer exists in db so no clue can/should point to it */
							assert((kill_t_create == cse->old_mode) || (kill_t_write == cse->old_mode));
							continue;
						}
						TRAVERSE_TO_LATEST_CSE(cse);
						assert(NULL == cse->new_buff || NULL != cse->blk_target);
						if (NULL == (blk_target = cse->blk_target))
							continue;
						if (blk_target->split_cleanup_needed)
						{
							for (level = 0; level < ARRAYSIZE(blk_target->last_split_blk_num); level++)
							{
								chain1 = *(off_chain *)&blk_target->last_split_blk_num[level];
								if (chain1.flag)
								{
									if (chain1.cw_index < si->cw_set_depth)
									{
										tp_get_cw(si->first_cw_set,
												(int)chain1.cw_index, &cse1);
										assert(NULL != cse1);
										histblk = cse1->blk;
									} else
									{	/* out of design situation. fix & proceed in pro */
										assert(FALSE);
										histblk = 0;
									}
									blk_target->last_split_blk_num[level] = histblk;
								}
							}
							blk_target->split_cleanup_needed = FALSE;
						}
						if (0 == blk_target->clue.end)
						{
							chain1 = *(off_chain *)&blk_target->root;
							if (chain1.flag)
							{
								assert(blk_target != cs_addrs->dir_tree);
								tp_get_cw(si->first_cw_set, (int)chain1.cw_index, &cse1);
								assert(NULL != cse1);
								blk_target->root = cse1->blk;
							}
							continue;
						}
						depth = blk_target->hist.depth;
						level = (int)cse->level;
						if (level > depth)
							continue;
						t1 = &blk_target->hist.h[level];
						cseblk = cse->blk;
						histblk = t1->blk_num;
						if (cseblk == histblk)
						{
							assert(!((off_chain *)&histblk)->flag);
							if (!is_mm)
							{
								cr = cse->cr;
								assert(NULL != cr);
								UNIX_ONLY(assert((NULL == t1->cr) || (t1->cr == cr)));
								if (cr != t1->cr)
								{
									t1->cr = cr;
									t1->cycle = cse->cycle;
									t1->buffaddr = GDS_REL2ABS(cr->buffaddr);
								} else
								{
									assert(t1->cr == cr);
									assert(t1->cycle == cse->cycle);
									assert(t1->buffaddr == GDS_REL2ABS(cr->buffaddr));
								}
							} else
							{
								t1->buffaddr = MM_BASE_ADDR(cs_addrs)
											+ (sm_off_t)cs_data->blk_size * cseblk;
								assert(NULL == t1->cr);
							}
							t1->cse = NULL;
						} else
						{
							chain1 = *(off_chain *)&histblk;
							if (chain1.flag)
							{
								tp_get_cw(si->first_cw_set, (int)chain1.cw_index, &cse1);
								if (cse == cse1)
								{
									if (blk_target->root == histblk)
										blk_target->root = cseblk;
									t1->blk_num = cseblk;
									if (is_mm)
										t1->buffaddr = MM_BASE_ADDR(cs_addrs)
											+ (sm_off_t)cs_data->blk_size * cseblk;
									else
									{
										cr = cse->cr;
										assert(NULL != cr);
										t1->cr = cr;
										t1->cycle = cse->cycle;
										t1->buffaddr = GDS_REL2ABS(cr->buffaddr);
									}
									t1->cse = NULL;
								}
							}
						}
					}
				}
				si->total_jnl_rec_size = cs_addrs->min_total_tpjnl_rec_size; /* Reinitialize total_jnl_rec_size */
				REINITIALIZE_LIST(si->recompute_list);
				REINITIALIZE_LIST(si->cw_set_list);	/* reinitialize the cw_set buddy_list */
				REINITIALIZE_LIST(si->new_buff_list);	/* reinitialize the new_buff buddy_list */
				REINITIALIZE_LIST(si->tlvl_cw_set_list);	/* reinitialize the tlvl_cw_set buddy_list */
				REINITIALIZE_LIST(si->tlvl_info_list);		/* reinitialize the tlvl_info buddy_list */
				si->first_cw_set = si->last_cw_set = si->first_cw_bitmap = NULL;
				si->cw_set_depth = 0;
				si->update_trans = 0;
			} else if (rollback_flag)
				REINITIALIZE_LIST(si->tlvl_info_list);		/* reinitialize the tlvl_info buddy_list */
#			ifdef DEBUG
			/* Verify that all fields that were reset in the if code above are already at the reset value.
			 * There are NO exceptions to this rule. If this transaction had si->update_trans non-zero at some
			 * point but later did rollbacks which caused it to become FALSE, the incremental rollback would
			 * have taken care to reset these fields explicitly.
			 */
			assert(si->tp_csa == cs_addrs);
			DBG_CHECK_SI_BUDDY_LIST_IS_REINITIALIZED(si);
			VERIFY_LIST_IS_REINITIALIZED(si->tlvl_info_list);
#			endif
			if (si->num_of_blks)
			{	/* Check that it is the same as the # of used entries in the hashtable.
				 * The only exception is if we got interrupted by a signal right after updating one
				 * but before updating the other which triggered exit handling for this process.
				 */
				assert(si->num_of_blks == si->blks_in_use->count
					|| process_exiting && (si->num_of_blks == (si->blks_in_use->count - 1)));
				reinitialize_hashtab_int4(si->blks_in_use);
				si->num_of_blks = 0;
			}
			si->cr_array_index = 0;			/* reinitialize si->cr_array */
			si->last_tp_hist = si->first_tp_hist;		/* reinitialize the tp history */
			si->tp_set_sgm_done = FALSE;
			si->tlvl_info_head = NULL;
			next_si = si->next_sgm_info;
			si->next_sgm_info = NULL;
		}	/* for (all segments in the transaction) */
		jnl_fence_ctl.fence_list = JNL_FENCE_LIST_END;
		/* No need to clean up jnl_fence_ctl.inctn_fence_list as it is used only by tp_tend and op_tcommit (after
		 * tp_tend is invoked) and is initialized to JNL_FENCE_LIST_END before both those usages. If any more
		 * usages of jnl_fence_ctl.inctn_fence_list occur, then this comment needs to be revisited.
		 */
#		ifdef DEBUG
		if (!process_exiting)
		{	/* Ensure that we did not miss out on clearing any gv_target->root which had chain.flag set.
			 * Dont do this if the process is cleaning up the TP transaction as part of exit handling
			 * Also use this opportunity to check that non-zero clues for BG contain non-null cr in histories.
			 * In addition, check that the list of multi-level block numbers (involved in the most recent split
			 * operations) stored in the gv_target are valid block #s.
			 */
			for (gvnh = gv_target_list; NULL != gvnh; gvnh = gvnh->next_gvnh)
			{
				chain1 = *(off_chain *)&gvnh->root;
				assert(!chain1.flag);
				for (level = 0; level < ARRAYSIZE(gvnh->last_split_blk_num); level++)
				{
					chain1 = *(off_chain *)&gvnh->last_split_blk_num[level];
					assert(!chain1.flag);
				}
				/* If there was a gvnh->write_local_tn, we could assert that if ever that field was updated
				 * in this transaction, then gvnh->root better be non-zero. Otherwise gvnh could have been
				 * used only for reads in this TP and in that case it is ok for the root to be 0.
				 */
				if (gvnh->root)
				{	/* check that gv_target->root falls within total blocks range */
					csa = gvnh->gd_csa;
					assert(NULL != csa);
					NON_GTM_TRUNCATE_ONLY(assert(gvnh->root < csa->ti->total_blks));
					assert(!IS_BITMAP_BLK(gvnh->root));
				}
				if (gvnh->clue.end)
				{
					is_mm = (dba_mm == gvnh->gd_csa->hdr->acc_meth);
					for (t1 = gvnh->hist.h; t1->blk_num; t1++)
					{
						assert(is_mm || (NULL != t1->cr));
						assert(NULL == t1->cse);
					}
					/* Now that we know the clue is non-zero, validate first_rec, clue & last_rec fields
					 * (BEFORE this clue could be used in a future transaction).
					 */
					DEBUG_GVT_CLUE_VALIDATE(gvnh);
				}
			}
		}
#		endif
		jgbl.cumul_jnl_rec_len = 0;
		jgbl.tp_ztp_jnl_upd_num = 0;
		GTMTRIG_ONLY(
			/* reset jgbl.prev_ztworm_ptr as we are now ready to start a new transaction
			 * and thus need to write new ztwormhole records if needed
			 */
			jgbl.prev_ztworm_ptr = NULL;
		)
		DEBUG_ONLY(jgbl.cumul_index = jgbl.cu_jnl_index = 0;)
		global_tlvl_info_head = NULL;
		REINITIALIZE_LIST(global_tlvl_info_list);		/* reinitialize the global_tlvl_info buddy_list */
		gvt_tp_list = NULL;
		CWS_RESET; /* reinitialize the hashtable before restarting/committing the TP transaction */
	}	/* if (any database work in the transaction) */
	VMS_ONLY(tp_has_kill_t_cse = FALSE;)
	sgm_info_ptr = NULL;
	first_sgm_info = NULL;
	/* ensure that we don't have crit on any region at the end of a TP transaction (be it GT.M or MUPIP). The only exception
	 * is ONLINE ROLLBACK which holds crit for the entire duration
	 */
	assert((CDB_STAGNATE == t_tries) || (0 == have_crit(CRIT_HAVE_ANY_REG)) UNIX_ONLY(|| jgbl.onlnrlbk));
	/* Now that this transaction try is done (need to start a fresh try in case of a restart; in case of commit the entire
	 * transaction is done) ensure first_tp_si_by_ftok is NULL at end of tp_clean_up as this field is relied upon by
	 * secshr_db_clnup and t_commit_cleanup to determine if we have an ongoing transaction. In case of a successfully
	 * committing transaction (rollback_flag == FALSE), this should be guaranteed already. So we might need to do the reset
	 * only in case rollback_flag == TRUE but since that is an if condition which involves a pipeline break we avoid it by
	 * doing the set to NULL unconditionally.
	 */
	assert(rollback_flag || (NULL == first_tp_si_by_ftok));
	first_tp_si_by_ftok = NULL;
	ENABLE_INTERRUPTS(INTRPT_IN_TP_CLEAN_UP);	/* check if any MUPIP STOP/signals were deferred while in this function */
}
