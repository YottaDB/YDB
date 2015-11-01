/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

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
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "longset.h"
#include "tp_change_reg.h"

GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	sgm_info		*sgm_info_ptr, *first_sgm_info;
GBLREF	ua_list			*curr_ua, *first_ua;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	int			update_array_size, tp_allocation_clue;
GBLREF	int			cumul_update_array_size;
GBLREF  gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	gv_namehead		*gv_target_list;
GBLREF	trans_num		local_tn;
GBLREF  sgmnt_data_ptr_t        cs_data;
GBLREF  short			dollar_tlevel;
GBLREF  buddy_list		*global_tlvl_info_list;
GBLREF  global_tlvl_info	*global_tlvl_info_head;
GBLREF jnl_gbls_t		jgbl;

void	tp_clean_up(boolean_t rollback_flag)
{
	gv_namehead	*gvnh, *blk_target;
	sgm_info	*si, *next_si;
	kill_set	*ks, *next_ks;
	cw_set_element	*cse, *cse1;
	int		cw_in_page = 0, hist_in_page = 0, level = 0;
	int4		depth;
	off_chain	chain1;
	ua_list		*next_ua;
	srch_blk_status	*srch_hist;
	boolean_t       is_mm;

	if (first_sgm_info != NULL)
	{
		assert(first_ua != NULL);
		if (first_ua->next_ua != NULL)
		{	/* if the original update array was too small, make a new larger one */
			for (curr_ua = first_ua, update_array_size = 0; curr_ua != NULL; curr_ua = next_ua)
			{
				update_array_size += curr_ua->update_array_size;
				next_ua = curr_ua->next_ua;
				free(curr_ua->update_array);
				if (curr_ua != first_ua)
					free(curr_ua);
			}
			assert(update_array_size == cumul_update_array_size);
			curr_ua = first_ua;
			first_ua->next_ua = NULL;
			if (BIG_UA < update_array_size)
				cumul_update_array_size = update_array_size = BIG_UA;
			first_ua->update_array_size = update_array_size;
			first_ua->update_array = update_array
						= (char *)malloc(update_array_size);
		}
		update_array_ptr = update_array;
		if (rollback_flag) 		/* Rollback invalidates clues in all targets used by this transaction */
		{
			for (gvnh = gv_target_list; NULL != gvnh; gvnh = gvnh->next_gvnh)
			{
				if (gvnh->read_local_tn != local_tn)
					continue;		/* Bypass gv_targets not used in this transaction */
				assert(NULL != first_sgm_info->next_sgm_info || cs_addrs->dir_tree->root == 1);
				gvnh->clue.end = 0;
				chain1 = *(off_chain *)&gvnh->root;
				if (chain1.flag)
				{
					assert(NULL != first_sgm_info->next_sgm_info || cs_addrs->dir_tree != gvnh);
					gvnh->root = 0;
				}
			}
			local_tn++;	/* to effectively invalidate first_tp_srch_status of all gv_targets */
		}
		for (si = first_sgm_info;  si != NULL;  si = next_si)
		{
			gv_cur_region = si->gv_cur_region;
			tp_change_reg();
			if (si->num_of_blks)
			{
				assert(si->num_of_blks == si->blks_in_use->count);
				longset((uchar_ptr_t)si->blks_in_use->tbl, sizeof(hashtab_ent) * si->blks_in_use->size, 0);
				si->blks_in_use->first = NULL;
				si->blks_in_use->last = NULL;
				si->blks_in_use->count = 0;
				si->num_of_blks = 0;
			}
			si->cr_array_index = 0;			/* reinitialize si->cr_array */
			si->backup_block_saved = FALSE;
			for (ks = si->kill_set_head;  ks != NULL;  ks = next_ks)
			{
				next_ks = ks->next_kill_set;
				free(ks);
			}
			si->kill_set_head = si->kill_set_tail = NULL;
			if (si->jnl_head)
			{
				reinitialize_list(si->format_buff_list);
				reinitialize_list(si->jnl_list);		/* reinitialize the jnl buddy_list */
				si->jnl_head = NULL;
				si->jnl_tail = &si->jnl_head;
				cs_addrs->next_fenced = NULL;
			}
			reinitialize_list(si->recompute_list);
			sgm_info_ptr = si;	/* for tp_get_cw to work */
			if (FALSE == rollback_flag)
			{
				is_mm = (dba_mm == gv_cur_region->dyn.addr->acc_meth);
				for (cse = si->first_cw_set; cse != si->first_cw_bitmap; cse = cse->next_cw_set)
				{
					TRAVERSE_TO_LATEST_CSE(cse);
					assert(NULL == cse->new_buff || NULL != cse->blk_target);
					if (NULL == (blk_target = cse->blk_target))
						continue;
					blk_target->clue.end = 0;
					if (0 == blk_target->clue.end)
					{
						chain1 = *(off_chain *)&blk_target->root;
						if (chain1.flag)
						{
							assert(blk_target != cs_addrs->dir_tree);
							blk_target->root = 0;
						}
						continue;
					}
					/* Non-rollback case (op_tcommit) validates clues in the targets we are updating. */
					srch_hist = &blk_target->hist.h[0];
					depth = blk_target->hist.depth;
					if ((level = (int)cse->level) > depth)
						continue;
					if (NULL == srch_hist[level].cr)
					{
						if (cse->blk == srch_hist[level].blk_num)
						{
							assert(!((off_chain *)&srch_hist[level].blk_num)->flag);
							if (is_mm)
								srch_hist[level].buffaddr = cs_addrs->acc_meth.mm.base_addr +
									(sm_off_t)cs_data->blk_size * cse->blk;
							else
							{
								assert(CYCLE_PVT_COPY == srch_hist[level].cycle);
								srch_hist[level].cr = cse->cr;
								srch_hist[level].cycle = cse->cycle;
								srch_hist[level].buffaddr = GDS_REL2ABS(cse->cr->buffaddr);
							}
							srch_hist[level].ptr = NULL;
						} else
						{
							chain1 = *(off_chain *)&srch_hist[level].blk_num;
							if (chain1.flag)
							{
								tp_get_cw(si->first_cw_set, (int)chain1.cw_index, &cse1);
								if (cse == cse1)
								{
									if (blk_target->root == srch_hist[level].blk_num)
										blk_target->root = cse->blk;
									srch_hist[level].blk_num = cse->blk;
									if (is_mm)
										srch_hist[level].buffaddr =
											cs_addrs->acc_meth.mm.base_addr +
											  (sm_off_t)cs_data->blk_size * cse->blk;
									else
									{
										srch_hist[level].cr = cse->cr;
										srch_hist[level].cycle = cse->cycle;
										srch_hist[level].buffaddr =
											GDS_REL2ABS(cse->cr->buffaddr);
									}
									srch_hist[level].ptr = NULL;
								}
							}
						}
					} else if (cse->blk == srch_hist[level].blk_num)
					{
						assert(cse->cr == srch_hist[level].cr);
						assert(cse->cycle == srch_hist[level].cycle);
					}
				}
			}
			reinitialize_list(si->cw_set_list);	/* reinitialize the cw_set buddy_list */
			reinitialize_list(si->new_buff_list);	/* reinitialize the new_buff buddy_list */
      			reinitialize_list(si->tlvl_cw_set_list);	/* reinitialize the tlvl_cw_set buddy_list */
      			reinitialize_list(si->tlvl_info_list);		/* reinitialize the tlvl_info buddy_list */
			si->first_cw_set = si->last_cw_set = si->first_cw_bitmap = NULL;
			si->cw_set_depth = 0;
			si->total_jnl_rec_size = cs_addrs->min_total_tpjnl_rec_size;	/* Reinitialize total_jnl_rec_size */
			si->last_tp_hist = si->first_tp_hist;		/* reinitialize the tp history */
			si->fresh_start = TRUE;
			si->tlvl_info_head = NULL;
			next_si = si->next_sgm_info;
			si->next_sgm_info = NULL;
			jnl_fence_ctl.fence_list = (sgmnt_addrs *)-1;
		}	/* for (all segments in the transaction) */
		jgbl.cumul_jnl_rec_len = 0;
		DEBUG_ONLY(jgbl.cumul_index = jgbl.cu_jnl_index = 0;)
		global_tlvl_info_head = NULL;
		reinitialize_list(global_tlvl_info_list);		/* reinitialize the global_tlvl_info buddy_list */
	}	/* if (any database work in the transaction) */

	tp_allocation_clue = MASTER_MAP_SIZE * BLKS_PER_LMAP + 1;
	sgm_info_ptr = NULL;
	first_sgm_info = NULL;
}
