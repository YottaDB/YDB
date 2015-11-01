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
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "copy.h"

GBLREF  sgm_info        	*first_sgm_info;
GBLREF  sgm_info        	*sgm_info_ptr;
GBLREF  short        		dollar_tlevel;
GBLREF 	uint4			cumul_jnl_rec_len;
GBLREF 	sgmnt_addrs		*cs_addrs;
GBLREF 	sgmnt_data_ptr_t	cs_data;
GBLREF  gd_region		*gv_cur_region;
GBLREF  global_tlvl_info	*global_tlvl_info_head;
GBLREF  buddy_list		*global_tlvl_info_list;
GBLREF	jnl_fence_control	jnl_fence_ctl;
DEBUG_ONLY(
GBLREF  uint4			cumul_index;
)

/* undo all the changes done in transaction levels greater than 'newlevel' */
/* Note that we do not make any effort to release crit grabbed in tlevels beyond
 * the 'newlevel' (rolling back to the crit scenario before the start of this tlevel)
 * because that case arises only on the final t_retry and during
 * this final t_retry we want to finish everything asap. But this has a side affect
 * that we might be holding crit on some region that is not really necessary due
 * to incremental rollback. When the latter becomes a prominent issue with performance,
 * then we should probably revisit the mechanism to release crit incrementally.
 */

void restore_next_off(cw_set_element *cse);
void rollbk_gbl_tlvl_info(short newlevel);
void rollbk_sgm_tlvl_info(short newlevel, sgm_info *si);

void tp_incr_clean_up(short newlevel)
{
	uint4		duint4, num_free;
	boolean_t	freed;
	sgm_info 	*si;
	cw_set_element 	*cse, *next_cse, *cse_tlvl, *high_cse, *cse_newlvl, *tmp_cse;
	srch_blk_status	*tp_srch_status;

	assert(newlevel > 0);
	if (jnl_fence_ctl.fence_list)		/* currently global_tlvl_info struct holds only jnl related info */
		rollbk_gbl_tlvl_info(newlevel);
	for (si = first_sgm_info;  si != NULL;  si = si->next_sgm_info)
	{
		num_free = 0;
		sgm_info_ptr = si;	/* maintain sgm_info_ptr & gv_cur_region binding whenever doing TP_CHANGE_REG */
		TP_CHANGE_REG_IF_NEEDED(si->gv_cur_region);
		rollbk_sgm_tlvl_info(newlevel, si);			/* rollback all the tlvl specific info */
		cse = si->first_cw_set;
		while (cse)
		{
			assert(!cse->low_tlevel);
			next_cse = cse->next_cw_set;
			tp_srch_status = (srch_blk_status *)lookup_hashtab_ent(si->blks_in_use, (void *)cse->blk, &duint4);
#ifdef DEBUG
			tmp_cse = cse;
			TRAVERSE_TO_LATEST_CSE(tmp_cse);
			assert(!tp_srch_status || tp_srch_status->ptr == tmp_cse);
#endif
			if (newlevel < cse->t_level)
			{
				INVALIDATE_CLUE(cse);
				cse_tlvl = cse->high_tlevel;
				while (cse_tlvl)			/* destroy the horizontal list except for the first link */
				{
					high_cse = cse_tlvl->high_tlevel;
					if (cse_tlvl->new_buff)
						free_element(si->new_buff_list, (char *)cse_tlvl->new_buff - sizeof(que_ent));
					free_element(si->tlvl_cw_set_list, (char *)cse_tlvl);
					cse_tlvl = high_cse;
				}
				cse->high_tlevel = NULL;
									/* destroy the first link in the horizontal list */
				if (si->first_cw_set == si->last_cw_set)
				{
					assert(cse == si->first_cw_set && !next_cse);
					si->first_cw_set = NULL;
					si->last_cw_set = NULL;
				} else if (cse == si->first_cw_set)
				{
					assert(next_cse);
					si->first_cw_set = next_cse;
					si->first_cw_set->prev_cw_set = NULL;
				} else if (cse == si->last_cw_set)
				{
					assert(!next_cse);
					si->last_cw_set = cse->prev_cw_set;
					assert(si->last_cw_set);
					si->last_cw_set->next_cw_set = NULL;
				} else
				{
					cse->prev_cw_set->next_cw_set = next_cse;
					next_cse->prev_cw_set = cse->prev_cw_set;
				}

				if (cse->new_buff)
					free_element(si->new_buff_list, (char *)cse->new_buff - sizeof(que_ent));
									/* update first_srch_status->ptr */
				if (tp_srch_status)
					tp_srch_status->ptr = NULL;
				num_free++;				/* count of number of elements to be freed from the
									 * vertical list, which are removed at the end */
			}else
			{
				for (cse_tlvl = cse; cse_tlvl && (newlevel >= cse_tlvl->t_level) ; cse_tlvl = cse_tlvl->high_tlevel)
					;
				if (cse_tlvl)	/* truncate from this link to the end of the horizontal list */
				{
					INVALIDATE_CLUE(cse_tlvl);
					cse_newlvl = cse_tlvl->low_tlevel;
					assert(cse_newlvl);
					assert(cse_newlvl->t_level <= newlevel);
					assert(cse_newlvl->done);
					cse_newlvl->high_tlevel = NULL;
					while(cse_tlvl)
					{
						high_cse = cse_tlvl->high_tlevel;
						if (cse_tlvl->new_buff)
							free_element(si->new_buff_list,
								(char *)cse_tlvl->new_buff - sizeof(que_ent));
						free_element(si->tlvl_cw_set_list, (char *)cse_tlvl);
						cse_tlvl = high_cse;
					}
						/* if either an index block or root of GVT's and next_off has been disturbed */
					if (cse_newlvl->undo_offset[0])
						restore_next_off(cse_newlvl);

					if (tp_srch_status)
						tp_srch_status->ptr = (void *)cse_newlvl;
				}
			}
			cse = next_cse;
		}
		assert(num_free <= si->cw_set_depth);
		si->cw_set_depth -= num_free;
		freed = free_last_n_elements(si->cw_set_list, num_free);
		assert(freed);
	}
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
		}else
			assert(!cse->undo_next_off[iter]);
	}
}

/* Rollback global (across all segments) tlvl specific info to the beginning of (newlevel + 1) tlevel.
 */

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
	for (; old_csa != jnl_fence_ctl.fence_list; old_csa = tmp_next_csa)
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
	boolean_t		dummy_ret, invalidate;
	void			*dummy = NULL;
	block_id		blk;
	kill_set        	*ks, *temp_kill_set;
	jnl_format_buffer	*jfb, *temp_jfb;
	tlevel_info		*tli, *next_tli, *prev_tli;
	srch_blk_status		*th, *tp_srch_status;

	for (prev_tli = NULL, tli = si->tlvl_info_head; tli; tli = tli->next_tlevel_info)
	{
		if (newlevel < tli->t_level)
			break;
		prev_tli = tli;
	}
	if (tli && tli->t_level == newlevel + 1)
	{
								/* freeup the kill set used in tlevels > newlevel */
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
			cumul_jnl_rec_len = tli->tlvl_cumul_jrec_len;
			DEBUG_ONLY(cumul_index = tli->tlvl_cumul_index;)
		}
		invalidate = FALSE;
		for (th = tli->tlvl_tp_hist_info; th != si->last_tp_hist; th++)
		{
			dummy_ret = del_hashtab_ent(&si->blks_in_use, (void *)th->blk_num, dummy);
			assert(dummy_ret);
			si->num_of_blks--;
			if (!invalidate)
			{
				for (tp_srch_status = cs_addrs->dir_tree->hist.h;
					HIST_TERMINATOR != (blk = tp_srch_status->blk_num); tp_srch_status++)
				{
					ASSERT_IS_WITHIN_TP_HIST_ARRAY_BOUNDS(tp_srch_status->first_tp_srch_status, si);
					if (tp_srch_status->first_tp_srch_status == th)
					{
						invalidate = TRUE;
						break;
					}
				}
			}
		}
		if (invalidate)					/* invalidate dir_tree history, if necessary */
			cs_addrs->dir_tree->clue.end = 0;
		si->last_tp_hist = tli->tlvl_tp_hist_info;
	} else							/* there was nothing at the beginning of
								 * transaction level (newlevel + 1) */
	{
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
			cumul_jnl_rec_len = 0;
			DEBUG_ONLY(cumul_index = 0;)
		}
		reinit_hashtab(&si->blks_in_use);
		si->num_of_blks = 0;
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
