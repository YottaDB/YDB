/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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

#include "change_reg.h"		/* prototypes */

GBLREF  sgm_info        	*first_sgm_info;
GBLREF  sgm_info        	*sgm_info_ptr;
GBLREF  short        		dollar_tlevel;
GBLREF 	sgmnt_addrs		*cs_addrs;
GBLREF 	sgmnt_data_ptr_t	cs_data;
GBLREF  gd_region		*gv_cur_region;
GBLREF  global_tlvl_info	*global_tlvl_info_head;
GBLREF  buddy_list		*global_tlvl_info_list;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF jnl_gbls_t		jgbl;
GBLREF	trans_num		local_tn;

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
void rollbk_gbl_tlvl_info(short newlevel);
void rollbk_sgm_tlvl_info(short newlevel, sgm_info *si);

void tp_incr_clean_up(short newlevel)
{
	uint4			num_free;
	boolean_t		freed;
	sgm_info 		*si;
	cw_set_element 		*cse, *next_cse, *tmp_cse;
	cw_set_element		*cse_newlvl;	/* pointer to that cse in a given horizontal list closest to "newlevel" */
	srch_blk_status		*tp_srch_status;
	int			min_t_level;	/* t_level of the head of the horizontal-list of a given cw-set-element */
	gd_region		*tmp_gv_cur_region;
	ht_ent_int4		*tabent;

	assert(newlevel > 0);
	if ((sgmnt_addrs *)-1 != jnl_fence_ctl.fence_list)	/* currently global_tlvl_info struct holds only jnl related info */
		rollbk_gbl_tlvl_info(newlevel);
	tmp_gv_cur_region = gv_cur_region;	/* save region and associated pointers to restore them later */
	for (si = first_sgm_info;  si != NULL;  si = si->next_sgm_info)
	{
		num_free = 0;
		sgm_info_ptr = si;	/* maintain sgm_info_ptr & gv_cur_region binding whenever doing TP_CHANGE_REG */
		TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
		rollbk_sgm_tlvl_info(newlevel, si);			/* rollback all the tlvl specific info */
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
			if (NULL != (tabent = lookup_hashtab_int4(si->blks_in_use, (uint4 *)&cse->blk)))
				tp_srch_status = tabent->value;
			else
				tp_srch_status = NULL;
			DEBUG_ONLY(
				tmp_cse = cse;
				TRAVERSE_TO_LATEST_CSE(tmp_cse);
				assert(!tp_srch_status || tp_srch_status->ptr == tmp_cse);
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
					assert(cse_newlvl->done);
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
				if (NULL == cse->low_tlevel)
					cse = cse->high_tlevel;	/* do not free up the head of the horizontal list */
				while (NULL != cse)
				{
					tmp_cse = cse->high_tlevel;
					if (cse->new_buff)
						free_element(si->new_buff_list, (char *)cse->new_buff - sizeof(que_ent));
					free_element(si->tlvl_cw_set_list, (char *)cse);
					cse = tmp_cse;
				}
				if (NULL != tp_srch_status)
					tp_srch_status->ptr = (void *)cse_newlvl;
			}
			cse = next_cse;
		}
		assert(num_free <= si->cw_set_depth);
		si->cw_set_depth -= num_free;
		freed = free_last_n_elements(si->cw_set_list, num_free);
		assert(freed);
	}
	assert((NULL != first_sgm_info) || 0 == cw_stagnate.size || cw_stagnate_reinitialized);
		/* if no database activity, cw_stagnate should be uninitialized or reinitialized */
	if (NULL != first_sgm_info)
		CWS_RESET;
	gv_cur_region = tmp_gv_cur_region;	/* restore gv_cur_region and associated pointers */
	change_reg();
	if (NULL == gv_cur_region)		/* change_reg() does not set sgm_info_ptr() in case of NULL gv_cur_region */
		sgm_info_ptr = NULL;
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
	assert(2 == (sizeof(cse->undo_offset) / sizeof(cse->undo_offset[0])));
	assert(2 == (sizeof(cse->undo_next_off) / sizeof(cse->undo_next_off[0])));
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

void rollbk_gbl_tlvl_info(short newlevel)
{
	global_tlvl_info	*gtli, *next_gtli, *tmp_gtli, *prev_gtli;
	sgmnt_addrs		*old_csa, *tmp_next_csa;

	old_csa = jnl_fence_ctl.fence_list;

	for (prev_gtli = NULL, gtli = global_tlvl_info_head; gtli; gtli = gtli->next_global_tlvl_info)
	{
		if (newlevel < gtli->t_level)
			break;
		prev_gtli = gtli;
	}
	assert(!global_tlvl_info_head || gtli);
	assert(!prev_gtli || (gtli && (newlevel + 1 == gtli->t_level)));
	if (gtli && newlevel + 1 == gtli->t_level)
		jnl_fence_ctl.fence_list = gtli->global_tlvl_fence_info;
	else
		jnl_fence_ctl.fence_list = (sgmnt_addrs *)-1;

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
}

/* Rollback the tlvl specific info (per segment) stored in tlevel_info list.
 * Rollback to the beginning state of (newlevel + 1) for the sgm_info 'si'
 */

void rollbk_sgm_tlvl_info(short newlevel, sgm_info *si)
{
	int			tli_cnt;
	boolean_t		deleted, invalidate;
	void			*dummy = NULL;
	block_id		blk;
	kill_set        	*ks, *temp_kill_set;
	jnl_format_buffer	*jfb, *temp_jfb;
	tlevel_info		*tli, *next_tli, *prev_tli;
	srch_blk_status		*th, *tp_srch_status;

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
	if (tli && tli->t_level == newlevel + 1)
	{	/* freeup the kill set used in tlevels > newlevel */
		if (tli->tlvl_kill_set)
		{
			for (ks = si->kill_set_head; ks != tli->tlvl_kill_set; ks = ks->next_kill_set)
				;
			assert(ks);
			ks->used = tli->tlvl_kill_used;
			si->kill_set_tail = ks;
			temp_kill_set = ks->next_kill_set;
			FREE_KILL_SET(si, temp_kill_set);
			ks->next_kill_set = NULL;
		} else
		{
			temp_kill_set = si->kill_set_head;
			FREE_KILL_SET(si, temp_kill_set);
			si->kill_set_head = si->kill_set_tail = NULL;
		}
		if (JNL_ENABLED(cs_addrs))
		{
			if (tli->tlvl_jfb_info)
			{
				for (jfb = si->jnl_head; jfb != tli->tlvl_jfb_info; jfb = jfb->next)
					;
				assert(jfb);
				temp_jfb = jfb->next;
				FREE_JFB_INFO(si, temp_jfb);
				jfb->next = NULL;
				si->jnl_tail = &jfb->next;
			} else
			{
				temp_jfb = si->jnl_head;
				FREE_JFB_INFO(si, temp_jfb);
				si->jnl_head = NULL;
				si->jnl_tail = &si->jnl_head;
			}
			jgbl.cumul_jnl_rec_len = tli->tlvl_cumul_jrec_len;
			DEBUG_ONLY(jgbl.cumul_index = tli->tlvl_cumul_index;)
		}
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
				if ((cs_addrs->dir_tree->read_local_tn == local_tn) && !invalidate)
				{
					for (tp_srch_status = cs_addrs->dir_tree->hist.h;
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
		assert(!invalidate || (0 == cs_addrs->dir_tree->clue.end));
		si->last_tp_hist = tli->tlvl_tp_hist_info;
		si->update_trans = tli->update_trans;
	} else
	{	/* there was nothing at the beginning of transaction level (newlevel + 1) */
		assert(tli == si->tlvl_info_head);
		temp_kill_set = si->kill_set_head;
		FREE_KILL_SET(si, temp_kill_set);
		si->kill_set_head = si->kill_set_tail = NULL;
		if (JNL_ENABLED(cs_addrs))
		{
			temp_jfb = si->jnl_head;
			FREE_JFB_INFO(si, temp_jfb);
			si->jnl_head = NULL;
			si->jnl_tail = &si->jnl_head;
			jgbl.cumul_jnl_rec_len = 0;
			DEBUG_ONLY(jgbl.cumul_index = 0;)
		}
		reinitialize_hashtab_int4(si->blks_in_use);
		si->num_of_blks = 0;
		si->update_trans = FALSE;
		cs_addrs->dir_tree->clue.end = 0;
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
