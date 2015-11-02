/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h and cws_insert.h */
#include "tp.h"
#include "tp_change_reg.h"
#include "cws_insert.h"		/* for cw_stagnate_reinitialized */
#include "gdsblkops.h"		/* for RESET_UPDATE_ARRAY macro */
#include "error.h"

GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	sgm_info		*sgm_info_ptr, *first_sgm_info;
GBLREF	sgm_info		*first_tp_si_by_ftok; /* List of participating regions in the TP transaction sorted on ftok order */
GBLREF	ua_list			*curr_ua, *first_ua;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	int			tp_allocation_clue;
GBLREF	uint4			update_array_size, cumul_update_array_size;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	gv_namehead		*gv_target_list;
GBLREF	trans_num		local_tn;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	short			dollar_tlevel;
GBLREF	buddy_list		*global_tlvl_info_list;
GBLREF	global_tlvl_info	*global_tlvl_info_head;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	int			process_exiting;
#ifdef VMS
GBLREF	boolean_t		tp_has_kill_t_cse; /* cse->mode of kill_t_write or kill_t_create got created in this transaction */
#endif

#ifdef DEBUG
GBLREF	uint4			donot_commit;	/* see gdsfhead.h for the purpose of this debug-only global */
#endif

void	tp_clean_up(boolean_t rollback_flag)
{
	gv_namehead	*gvnh, *blk_target;
	sgm_info	*si, *next_si;
	kill_set	*ks, *next_ks;
	cw_set_element	*cse, *cse1;
	int		cw_in_page = 0, hist_in_page = 0, level = 0;
	int4		depth;
	uint4		tmp_update_array_size;
	off_chain	chain1;
	ua_list		*next_ua, *tmp_ua;
	srch_blk_status	*srch_hist;
	boolean_t       is_mm;
	sgmnt_addrs	*csa;

	error_def(ERR_MEMORY);
	error_def(ERR_VMSMEMORY);

	assert((NULL != first_sgm_info) || (0 == cw_stagnate.size) || cw_stagnate_reinitialized);
		/* if no database activity, cw_stagnate should be uninitialized or reinitialized */
	DEBUG_ONLY(
		if (rollback_flag)
			donot_commit = FALSE;
		assert(!donot_commit);
	)
	if (first_sgm_info != NULL)
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
				if (NULL != &curr_ua->update_array[0])
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
			for (gvnh = gv_target_list; NULL != gvnh; gvnh = gvnh->next_gvnh)
			{
				if (gvnh->read_local_tn != local_tn)
					continue;		/* Bypass gv_targets not used in this transaction */
				gvnh->clue.end = 0;
				chain1 = *(off_chain *)&gvnh->root;
				if (chain1.flag)
				{
					DEBUG_ONLY(csa = gvnh->gd_csa;)
					assert(csa->dir_tree != gvnh);
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
							srch_hist[level].cse = NULL;
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
									srch_hist[level].cse = NULL;
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
			si->update_trans = FALSE;
			si->total_jnl_rec_size = cs_addrs->min_total_tpjnl_rec_size;	/* Reinitialize total_jnl_rec_size */
			si->last_tp_hist = si->first_tp_hist;		/* reinitialize the tp history */
			si->fresh_start = TRUE;
			si->tlvl_info_head = NULL;
			next_si = si->next_sgm_info;
			si->next_sgm_info = NULL;
			jnl_fence_ctl.fence_list = JNL_FENCE_LIST_END;
		}	/* for (all segments in the transaction) */
		DEBUG_ONLY(
			if (!process_exiting)
			{	/* Ensure that we did not miss out on clearing any gv_target->root which had chain.flag set.
				 * Dont do this if the process is cleaning up the TP transaction as part of exit handling
				 */
				for (gvnh = gv_target_list; NULL != gvnh; gvnh = gvnh->next_gvnh)
				{
					chain1 = *(off_chain *)&gvnh->root;
					assert(!chain1.flag);
					if (gvnh->root)
					{	/* check that gv_target->root falls within total blocks range */
						csa = gvnh->gd_csa;
						assert(NULL != csa);
						assert(gvnh->root < csa->ti->total_blks);
					}
				}
			}
		)
		jgbl.cumul_jnl_rec_len = 0;
		DEBUG_ONLY(jgbl.cumul_index = jgbl.cu_jnl_index = 0;)
		global_tlvl_info_head = NULL;
		reinitialize_list(global_tlvl_info_list);		/* reinitialize the global_tlvl_info buddy_list */
		CWS_RESET; /* reinitialize the hashtable before restarting/committing the TP transaction */
	}	/* if (any database work in the transaction) */
	VMS_ONLY(tp_has_kill_t_cse = FALSE;)
	tp_allocation_clue = MASTER_MAP_SIZE_MAX * BLKS_PER_LMAP + 1;
	sgm_info_ptr = NULL;
	first_sgm_info = NULL;
	assert((NULL == first_tp_si_by_ftok) || process_exiting);
}
