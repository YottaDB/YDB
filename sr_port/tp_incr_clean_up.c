/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for tp.h and cws_insert.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "copy.h"
#include "longset.h"		/* also needed for cws_insert.h */
#include "cws_insert.h"		/* for cw_stagnate_reinitialized */
#ifdef GTM_TRIGGER
#include <rtnhdr.h>
#include "gv_trigger.h"		/* for TP_INVALIDATE_TRIGGER_CYCLES_IF_NEEDED macro */
#endif

GBLREF	sgm_info		*first_sgm_info;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	gd_region		*gv_cur_region;
GBLREF	global_tlvl_info	*global_tlvl_info_head;
GBLREF	buddy_list		*global_tlvl_info_list;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	trans_num		local_tn;
GBLREF	uint4			update_array_size;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	ua_list			*curr_ua, *first_ua;
GBLREF	gv_namehead		*gvt_tp_list;
DEBUG_ONLY(GBLREF uint4		cumul_update_array_size;)

/* undo all the changes done in transaction levels greater than 'newlevel' */
/* Note that we do not make any effort to release crit grabbed in tlevels beyond the 'newlevel'
 * (rolling back to the crit scenario before the start of this tlevel) because that case arises only
 * on the final t_retry and during this final t_retry we want to finish everything asap.
 * But this has a side affect that we might be holding crit on some region that is not really necessary due
 * to incremental rollback. When the latter becomes a prominent issue with performance,
 * then we should probably revisit the mechanism to release crit incrementally.
 *
 * It is helpful to imagine the cw-set-element array as a matrix with a horizontal list and vertical list.
 * horizontal list --> This is the list of cw-set-elements linked by the "low_tlevel" and "high_tlevel" members.
 * 			All cw-set-elements in this list correspond to the same block and differ only in the dollar_tlevel
 * 				when the update occurred in this block.
 * vertical   list --> This is the list of cw-set-elements linked by the "prev_cw_set" and "next_cw_set" members.
 * 			All cw-set-elements in this list correspond to different blocks and each in turn has a horizontal list.
 */

void restore_next_off(cw_set_element *cse);
void rollbk_gbl_tlvl_info(uint4 newlevel);
void rollbk_sgm_tlvl_info(uint4 newlevel, sgm_info *si);

void tp_incr_clean_up(uint4 newlevel)
{
	boolean_t		freed;
	cw_set_element		*cse_newlvl;	/* pointer to that cse in a given horizontal list closest to "newlevel" */
	cw_set_element 		*cse, *next_cse, *tmp_cse;
	gv_namehead		*gvnh;
	ht_ent_int4		*tabent;
	int			min_t_level;	/* t_level of the head of the horizontal-list of a given cw-set-element */
	int4			upd_trans;
	sgm_info 		*si;
	sgmnt_addrs		*csa;
	srch_blk_status		*tp_srch_status;
	uint4			num_free;

	assert(newlevel);
	if (NULL != first_sgm_info)
	{
		assert(NULL != global_tlvl_info_list);
		rollbk_gbl_tlvl_info(newlevel);
	}
	for (si = first_sgm_info;  si != NULL;  si = si->next_sgm_info)
	{
		num_free = 0;
		upd_trans = si->update_trans;	/* Note down before rollback if there were any updates in this region */
		rollbk_sgm_tlvl_info(newlevel, si);	/* rollback all the tlvl specific info; may reset si->update_trans */
		cse = si->first_cw_set;
		DEBUG_ONLY(min_t_level = 1);
		/* A property that will help a lot in understanding this algorithm is the following.
		 * All cse's in a given horizontal list will have their "next_cw_set" pointing to the same cse
		 * 	which is guaranteed to be the head of the horizontal list of the next cw-set-element in the vertical list.
		 */
		while (NULL != cse)
		{
			assert(NULL == cse->low_tlevel);
			next_cse = cse->next_cw_set;
			/* Note down tp_srch_status corresponding to cse (in case it exists). Need to later reset "->cse" field
			 * of this structure to point to the new cse for this block. Note that if cse->mode is gds_t_create,
			 * there will be no tp_srch_status entry allotted for cse->blk (one will be there only for the chain.flag
			 * representation of this to-be-created block). Same case with mode of kill_t_create as it also corresponds
			 * to a non-existent block#. Therefore dont try looking up the hashtable for this block in those cases.
			 */
			tp_srch_status = NULL;
			assert((gds_t_create == cse->mode) || (kill_t_create == cse->mode)
				|| (gds_t_write == cse->mode) || (kill_t_write == cse->mode));
			if ((gds_t_create != cse->mode) && (kill_t_create != cse->mode)
					&& (NULL != (tabent = lookup_hashtab_int4(si->blks_in_use, (uint4 *)&cse->blk))))
				tp_srch_status = tabent->value;
			DEBUG_ONLY(
				tmp_cse = cse;
				TRAVERSE_TO_LATEST_CSE(tmp_cse);
				assert((NULL == tp_srch_status) || (tp_srch_status->cse == tmp_cse));
			)
			if (newlevel < cse->t_level)
			{	/* delete the entire horizontal list for this cw-set-element.
				 * And because of the following assert, we will be deleting the entire horizontal list for
				 * 	all cw-set-elements following the current one in the vertical list.
				 */
				assert(min_t_level <= cse->t_level);
				DEBUG_ONLY(min_t_level = cse->t_level;)
				if (!num_free)
				{	/* first time an entire cw-set-element's horizontal-list needs to be removed.
					 * reset si->first_cw_set or si->last_cw_set pointers as appropriate.
					 * the actual free up of the cw-set-elements will occur later in this loop
					 */
					tmp_cse = cse->prev_cw_set;
					assert(((NULL == tmp_cse) && (cse == si->first_cw_set))
							|| ((NULL != tmp_cse) && (cse != si->first_cw_set)));
					if (cse == si->first_cw_set)
						si->first_cw_set = NULL;
					si->last_cw_set = tmp_cse;
					while (NULL != tmp_cse)
					{	/* reset forward-link of horizontal-list of the previous cw_set_element */
						assert(tmp_cse->next_cw_set == cse);
						tmp_cse->next_cw_set = NULL;
						tmp_cse = tmp_cse->high_tlevel;
					}
				}
				num_free++;	/* count of number of elements whose vertical list has been completely removed */
				cse_newlvl = NULL;
			} else
			{
				assert(!num_free);
				for ( ; (NULL != cse) && (newlevel >= cse->t_level); cse = cse->high_tlevel)
					;
				cse_newlvl = (NULL != cse) ? cse->low_tlevel : NULL;
				if (NULL != cse_newlvl)
				{
					assert(cse_newlvl->t_level <= newlevel);
					assert(cse_newlvl->done || (n_gds_t_op < cse->mode));
					cse_newlvl->high_tlevel = NULL;
					/* if either an index block or root of GVT's and next_off has been disturbed */
					if (cse_newlvl->undo_offset[0])
						restore_next_off(cse_newlvl);
				}
			}
			if (NULL != cse)	/* free up cw-set-elements from this link to the end of the horizontal list */
			{
				INVALIDATE_CLUE(cse);
				/* note that the head of each horizontal list is actually part of si->cw_set_list buddy_list.
				 * while every other member of each horizontal list is part of si->tlvl_cw_set_list buddy_list.
				 * free up only the latter category in the loop below. free up of the head of the horizontal
				 * 	list is done later below in the call to free_last_n_elements(si->cw_set_list, num_free);
				 */
				while (NULL != cse)
				{
					tmp_cse = cse->high_tlevel;
					if (cse->new_buff)
						free_element(si->new_buff_list, (char *)cse->new_buff);
					if (NULL != cse->low_tlevel) /* do not free up the head of the horizontal list */
						free_element(si->tlvl_cw_set_list, (char *)cse);
					cse = tmp_cse;
				}
				if (NULL != tp_srch_status)
					tp_srch_status->cse = (void *)cse_newlvl;
			}
			cse = next_cse;
		}
		assert(num_free <= si->cw_set_depth);
		si->cw_set_depth -= num_free;
		freed = free_last_n_elements(si->cw_set_list, num_free);
		assert(freed);
		if (upd_trans && !si->update_trans)
		{	/* si had updates before the rollback but none after. Do buddylist cleanup so tp_clean_up dont need to */
			csa = si->tp_csa;
			if (JNL_ALLOWED(csa))
			{
				REINITIALIZE_LIST(si->jnl_list);
				REINITIALIZE_LIST(si->format_buff_list);
				si->total_jnl_rec_size = csa->min_total_tpjnl_rec_size;
			}
			REINITIALIZE_LIST(si->recompute_list);
			REINITIALIZE_LIST(si->cw_set_list);	/* reinitialize the cw_set buddy_list */
			REINITIALIZE_LIST(si->new_buff_list);	/* reinitialize the new_buff buddy_list */
			REINITIALIZE_LIST(si->tlvl_cw_set_list);	/* reinitialize the tlvl_cw_set buddy_list */
		}
		DEBUG_ONLY(if (!si->update_trans) DBG_CHECK_SI_BUDDY_LIST_IS_REINITIALIZED(si);)
	}
	GTMTRIG_ONLY(TP_INVALIDATE_TRIGGER_CYCLES_IF_NEEDED(TRUE, FALSE);)
	/* After an incremental rollback, it is possible that some gv_targets now have a block-split history that reflects
	 * a created block number that is no longer relevant due to the rollback. Fix those as needed.
	 */
	for (gvnh = gvt_tp_list; NULL != gvnh; gvnh = gvnh->next_tp_gvnh)
		TP_CLEANUP_GVNH_SPLIT_IF_NEEDED(gvnh, gvnh->gd_csa->sgm_info_ptr->cw_set_depth);
	assert((NULL != first_sgm_info) || 0 == cw_stagnate.size || cw_stagnate_reinitialized);
		/* if no database activity, cw_stagnate should be uninitialized or reinitialized */
	if (NULL != first_sgm_info)
		CWS_RESET;
}

/* correct the next_off field in cse which was overwritten due to next level
 * transaction that is now being rolled back
 */

void restore_next_off(cw_set_element *cse)
{
	sm_uc_ptr_t 	ptr;
	int		cur_blk_size, iter;
	off_chain	chain;

	assert(cse->done);
	assert(cse->new_buff);
	assert(cse->first_off);
	assert(cse->undo_offset[0]);	/* caller should ensure this */

#ifdef DEBUG
	ptr = cse->new_buff;
	cur_blk_size = ((blk_hdr_ptr_t)ptr)->bsiz;
	assert(2 == (SIZEOF(cse->undo_offset) / SIZEOF(cse->undo_offset[0])));
	assert(2 == (SIZEOF(cse->undo_next_off) / SIZEOF(cse->undo_next_off[0])));
	assert(cse->undo_offset[0] < cur_blk_size);
	assert(cse->undo_offset[1] < cur_blk_size);
#endif
	for (iter=0; iter<2; iter++)
	{
		if (cse->undo_offset[iter])
		{
			ptr = cse->new_buff + cse->undo_offset[iter];
			GET_LONGP(&chain, ptr);
			chain.next_off = cse->undo_next_off[iter];
			GET_LONGP(ptr, &chain);
			cse->undo_offset[iter] = cse->undo_next_off[iter] = 0;
		} else
			assert(!cse->undo_next_off[iter]);
	}
}

/* Rollback global (across all segments) tlvl specific info to the beginning of (newlevel + 1) tlevel.  */

void rollbk_gbl_tlvl_info(uint4 newlevel)
{
	global_tlvl_info	*gtli, *next_gtli, *tmp_gtli, *prev_gtli;
	sgmnt_addrs		*old_csa, *tmp_next_csa;
	ua_list			*ua_ptr;
	DEBUG_ONLY(uint4	dbg_upd_array_size;)

	old_csa = jnl_fence_ctl.fence_list;

	for (prev_gtli = NULL, gtli = global_tlvl_info_head; gtli; gtli = gtli->next_global_tlvl_info)
	{
		if (newlevel < gtli->t_level)
			break;
		prev_gtli = gtli;
	}
	assert(!global_tlvl_info_head || gtli);
	assert(!prev_gtli || (gtli && ((newlevel + 1) == gtli->t_level)));
	if (gtli && ((newlevel + 1) == gtli->t_level))
	{
		jnl_fence_ctl.fence_list = gtli->global_tlvl_fence_info;
		GTMTRIG_ONLY(
			/* Restore the ztwormhole pointer to the value at the start of the rollback'ed level */
			jgbl.prev_ztworm_ptr = gtli->tlvl_prev_ztworm_ptr;
		)
		jgbl.cumul_jnl_rec_len = gtli->tlvl_cumul_jrec_len;
		jgbl.tp_ztp_jnl_upd_num = gtli->tlvl_tp_ztp_jnl_upd_num;
		DEBUG_ONLY(jgbl.cumul_index = gtli->tlvl_cumul_index;)
		assert(NULL != gtli->curr_ua);
		curr_ua = (ua_list *)(gtli->curr_ua);
		update_array_ptr = gtli->upd_array_ptr;
	} else
	{
		jnl_fence_ctl.fence_list = JNL_FENCE_LIST_END;
		GTMTRIG_ONLY(
			/* Fresh start, so reset ztwormhole pointer. */
			jgbl.prev_ztworm_ptr = NULL;
		)
		jgbl.cumul_jnl_rec_len = 0;
		jgbl.tp_ztp_jnl_upd_num = 0;
		DEBUG_ONLY(jgbl.cumul_index = 0;)
		curr_ua = first_ua;
		update_array_ptr = curr_ua->update_array;
	}
	/* Restore update array related global variables. It is possible that more ua_list structures could
	 * have been created and chained in transactions ($TLEVEL >= newlevel + 1). Do not reclaim that memory
	 * as the subsequent transactions could use that memory (if needed)
	 */
	assert((NULL != first_ua) && (NULL != curr_ua)); /* A prior database activity should have created at least one ua_list */
	update_array = curr_ua->update_array;
	update_array_size = curr_ua->update_array_size;
	assert((update_array_ptr >= update_array) && (update_array_ptr <= (update_array + curr_ua->update_array_size)));
	DEBUG_ONLY(
		dbg_upd_array_size = 0;
		for (ua_ptr = first_ua; ua_ptr; ua_ptr = ua_ptr->next_ua)
			dbg_upd_array_size += ua_ptr->update_array_size;
		assert(dbg_upd_array_size == cumul_update_array_size);
	)
	/* No need to reset cumul_update_array_size since we are not free'ing up any ua_list structures */
	FREE_GBL_TLVL_INFO(gtli);
	if (prev_gtli)
		prev_gtli->next_global_tlvl_info = NULL;
	else
		global_tlvl_info_head = NULL;
	for ( ; old_csa != jnl_fence_ctl.fence_list; old_csa = tmp_next_csa)
	{
		tmp_next_csa = old_csa->next_fenced;
		old_csa->next_fenced = NULL;
	}
	/* No need to clean up jnl_fence_ctl.inctn_fence_list. See similar comment in tp_clean_up for details on why */
}

/* Rollback the tlvl specific info (per segment) stored in tlevel_info list.
 * Rollback to the beginning state of (newlevel + 1) for the sgm_info 'si'
 */

void rollbk_sgm_tlvl_info(uint4 newlevel, sgm_info *si)
{
	int			tli_cnt;
	boolean_t		deleted, invalidate;
	void			*dummy = NULL;
	block_id		blk;
	kill_set        	*ks, *temp_kill_set;
	tlevel_info		*tli, *next_tli, *prev_tli;
	srch_blk_status		*th, *tp_srch_status;
	sgmnt_addrs		*csa;

	for (prev_tli = NULL, tli = si->tlvl_info_head; tli; tli = tli->next_tlevel_info)
	{
		assert((NULL == prev_tli) || (tli->t_level == (prev_tli->t_level + 1)));
		if (newlevel < tli->t_level)
			break;
		prev_tli = tli;
	}
	/* Invalidate clues of all gv_targets corresponding to tp_hist array entries added at newlevel + 1 or greater */
	for (th = ((NULL != prev_tli) ? tli->tlvl_tp_hist_info : si->first_tp_hist); th != si->last_tp_hist; th++)
	{	/* note that it is very likely that more than one tp_hist array entry has the same blk_target value
		 * i.e. corresponds to the same global variable and hence invalidation of the clue might occur more
		 * than once per global. since the invalidation operation is a simple assignment of clue.end to 0,
		 * this duplication is assumed to not be a performance overhead.  In any case there is no other easy way
		 * currently of determining the list of globals whose gv_targets need to be invalidated on a rollback.
		 */
		assert(NULL != th->blk_target);
		th->blk_target->clue.end = 0;
	}
	csa = si->tp_csa;
	if (tli && tli->t_level == newlevel + 1)
	{	/* freeup the kill set used in tlevels > newlevel */
		if (tli->tlvl_kill_set)
		{
			ks = tli->tlvl_kill_set;
			assert(ks);
			ks->used = tli->tlvl_kill_used;
			si->kill_set_tail = ks;
			temp_kill_set = ks->next_kill_set;
			FREE_KILL_SET(temp_kill_set);
			ks->next_kill_set = NULL;
		} else
		{
			temp_kill_set = si->kill_set_head;
			FREE_KILL_SET(temp_kill_set);
			si->kill_set_head = si->kill_set_tail = NULL;
		}
		FREE_JFB_INFO_IF_NEEDED(csa, si, tli, FALSE);
		DEBUG_ONLY(invalidate = FALSE;)
		for (th = tli->tlvl_tp_hist_info; th != si->last_tp_hist; th++)
		{
			deleted = delete_hashtab_int4(si->blks_in_use, (uint4 *)&th->blk_num);
			assert(deleted);
			si->num_of_blks--;
			DEBUG_ONLY(
				/* this is prior code which is no longer deemed necessary since invalidating clues of all
				 * blk_targets is now done above and the directory tree should also be covered by that.
				 * hence the DEBUG_ONLY surrounding for the statements below. --- nars -- 2002/07/26
				 */
				if ((csa->dir_tree->read_local_tn == local_tn) && !invalidate)
				{
					for (tp_srch_status = csa->dir_tree->hist.h;
						HIST_TERMINATOR != (blk = tp_srch_status->blk_num); tp_srch_status++)
					{
						if (tp_srch_status->first_tp_srch_status == th)
						{
							invalidate = TRUE;
							break;
						}
					}
				}
			)
		}
		assert(!invalidate || (0 == csa->dir_tree->clue.end));
		si->last_tp_hist = tli->tlvl_tp_hist_info;
		si->update_trans = tli->update_trans;
	} else
	{	/* there was nothing at the beginning of transaction level (newlevel + 1) */
		assert(tli == si->tlvl_info_head);
		temp_kill_set = si->kill_set_head;
		FREE_KILL_SET(temp_kill_set);
		si->kill_set_head = si->kill_set_tail = NULL;
		FREE_JFB_INFO_IF_NEEDED(csa, si, tli, TRUE);
		reinitialize_hashtab_int4(si->blks_in_use);
		si->num_of_blks = 0;
		si->update_trans = 0;
		csa->dir_tree->clue.end = 0;
		si->last_tp_hist = si->first_tp_hist;
	}
	/* delete all the tli's starting from this tli */
	for (tli_cnt = 0; tli; tli = tli->next_tlevel_info)
		tli_cnt++;
	free_last_n_elements(si->tlvl_info_list, tli_cnt);
	if (prev_tli)
		prev_tli->next_tlevel_info = NULL;
	else
		si->tlvl_info_head = NULL;
}
