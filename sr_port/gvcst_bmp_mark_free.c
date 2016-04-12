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

/* gvcst_bmp_mark_free.c
	This marks all the blocks in kill set list to be marked free.
	Note ks must be already sorted
*/
#include "mdef.h"

#include "gtm_string.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "memcoherency.h"
#include "gdsblkops.h"	/* for CHECK_AND_RESET_UPDATE_ARRAY macro */

/* Include prototypes */
#include "t_qread.h"
#include "t_end.h"
#include "t_retry.h"
#include "t_begin.h"
#include "t_write_map.h"
#include "mm_read.h"
#include "add_inter.h"
#include "gvcst_bmp_mark_free.h"
#include "t_busy2free.h"
#include "t_abort.h"
#ifdef UNIX
#include "db_snapshot.h"
#endif
#include "muextr.h"
#include "mupip_reorg.h"

GBLREF	char			*update_array, *update_array_ptr;
GBLREF	cw_set_element		cw_set[];
GBLREF 	unsigned char    	cw_set_depth;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	unsigned char		rdfail_detail;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	boolean_t		mu_reorg_process;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	inctn_detail_t		inctn_detail;			/* holds detail to fill in to inctn jnl record */
GBLREF	uint4			dollar_tlevel;
#ifdef UNIX
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
#endif
GBLREF	gd_region		*gv_cur_region;

error_def(ERR_GVKILLFAIL);
error_def(ERR_IGNBMPMRKFREE);

trans_num gvcst_bmp_mark_free(kill_set *ks)
{
	block_id		bit_map, next_bm, *updptr;
	blk_ident		*blk, *blk_top, *nextblk;
	trans_num		ctn, start_db_fmt_tn;
	unsigned int		len;
#	if defined(UNIX) && defined(DEBUG)
	unsigned int		lcl_t_tries;
#	endif
	int4			blk_prev_version;
	srch_hist		alt_hist;
	trans_num		ret_tn = 0;
	boolean_t		visit_blks;
	srch_blk_status		bmphist;
	cache_rec_ptr_t		cr;
	enum db_ver		ondsk_blkver;
	enum cdb_sc		status;
	boolean_t		mark_level_as_special;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	TREF(in_gvcst_bmp_mark_free) = TRUE;
	assert(inctn_bmp_mark_free_gtm == inctn_opcode || inctn_bmp_mark_free_mu_reorg == inctn_opcode);
	/* Note down the desired_db_format_tn before you start relying on cs_data->fully_upgraded.
	 * If the db is fully_upgraded, take the optimal path that does not need to read each block being freed.
	 * But in order to detect concurrent desired_db_format changes, note down the tn (when the last format change occurred)
	 * 	before the fully_upgraded check	and after having noted down the database current_tn.
	 * If they are the same, then we are guaranteed no concurrent desired_db_format change occurred.
	 * If they are not, then fall through to the non-optimal path where each to-be-killed block has to be visited.
	 * The reason we need to visit every block in case desired_db_format changes is to take care of the case where
	 *	MUPIP REORG DOWNGRADE concurrently changes a block that we are about to free.
	 */
	start_db_fmt_tn = cs_data->desired_db_format_tn;
	visit_blks = (!cs_data->fully_upgraded);	/* Local evaluation */
	assert(!visit_blks || (visit_blks && dba_bg == cs_addrs->hdr->acc_meth)); /* must have blks_to_upgrd == 0 for non-BG */
	assert(!dollar_tlevel); 			/* Should NOT be in TP now */
	blk = &ks->blk[0];
	blk_top = &ks->blk[ks->used];
	if (!visit_blks)
	{	/* Database has been completely upgraded. Free all blocks in one bitmap as part of one transaction. */
		assert(cs_data->db_got_to_v5_once); /* assert all V4 fmt blocks (including RECYCLED) have space for V5 upgrade */
		inctn_detail.blknum_struct.blknum = 0; /* to indicate no adjustment to "blks_to_upgrd" necessary */
		/* If any of the mini transaction below restarts because of an online rollback, we don't want the application
		 * refresh to happen (like $ZONLNRLBK++ or rts_error(DBROLLEDBACK). This is because, although we are currently in	{BYPASSOK}
		 * non-tp (dollar_tleve = 0), we could actually be in a TP transaction and have actually faked dollar_tlevel. In
		 * such a case, we should NOT * be issuing a DBROLLEDBACK error as TP transactions are supposed to just restart in
		 * case of an online rollback. So, set the global variable that gtm_onln_rlbk_clnup can check and skip doing the
		 * application refresh, but will reset the clues. The next update will see the cycle mismatch and will accordingly
		 * take the right action.
		 */
		for ( ; blk < blk_top;  blk = nextblk)
		{
			if (0 != blk->flag)
			{
				nextblk = blk + 1;
				continue;
			}
			assert(0 < blk->block);
			assert((int4)blk->block < cs_addrs->ti->total_blks);
			bit_map = ROUND_DOWN2((int)blk->block, BLKS_PER_LMAP);
			next_bm = bit_map + BLKS_PER_LMAP;
			CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
			/* Scan for the next local bitmap */
			updptr = (block_id *)update_array_ptr;
			for (nextblk = blk;
				(0 == nextblk->flag) && (nextblk < blk_top) && ((block_id)nextblk->block < next_bm);
				++nextblk)
			{
				assert((block_id)nextblk->block - bit_map);
				*updptr++ = (block_id)nextblk->block - bit_map;
			}
			len = (unsigned int)((char *)nextblk - (char *)blk);
			update_array_ptr = (char *)updptr;
			alt_hist.h[0].blk_num = 0;			/* need for calls to T_END for bitmaps */
			alt_hist.h[0].blk_target = NULL;		/* need to initialize for calls to T_END */
			/* the following assumes SIZEOF(blk_ident) == SIZEOF(int) */
			assert(SIZEOF(blk_ident) == SIZEOF(int));
			*(int *)update_array_ptr = 0;
			t_begin(ERR_GVKILLFAIL, UPDTRNS_DB_UPDATED_MASK);
			for (;;)
			{
				ctn = cs_addrs->ti->curr_tn;
				/* Need a read fence before reading fields from cs_data as we are reading outside
				 * of crit and relying on this value to detect desired db format state change.
				 */
				SHM_READ_MEMORY_BARRIER;
				if (start_db_fmt_tn != cs_data->desired_db_format_tn)
				{	/* Concurrent db format change has occurred. Need to visit every block to be killed
					 * to determine its block format. Fall through to the non-optimal path below
					 */
					ret_tn = 0;
					break;
				}
#				ifdef GTM_SNAPSHOT
				/* if this is freeing a level-0 directory tree block, we need to transition the block to free
				 * right away and write its before-image thereby enabling fast integ to avoid writing level-0
				 * block before-images altogether. It is possible the fast integ hasn't started at this stage,
				 * so we cannot use FASTINTEG_IN_PROG in the if condition, but fast integ may already start later
				 * at bg/mm update stage, so we always need to prepare cw_set element
				 */
				if ((MUSWP_FREE_BLK == TREF(in_mu_swap_root_state)) && blk->level)
				{ /* blk->level was set as 1 for level-0 DIR tree block in mu_swap_root */
					/* for mu_swap_root, only one block is freed during bmp_mark_free */
					assert(1 == ks->used);
					ctn = cs_addrs->ti->curr_tn;
					alt_hist.h[0].cse     = NULL;
					alt_hist.h[0].tn      = ctn;
					alt_hist.h[0].blk_num = blk->block;
					alt_hist.h[1].blk_num = 0; /* this is to terminate history reading in t_end */
					if (NULL == (alt_hist.h[0].buffaddr = t_qread(alt_hist.h[0].blk_num,
								      (sm_int_ptr_t)&alt_hist.h[0].cycle,
								      &alt_hist.h[0].cr)))
					{
						t_retry((enum cdb_sc)rdfail_detail);
						continue;
					}
					t_busy2free(&alt_hist.h[0]);
					/* The special level value will be used later in t_end to indicate
					 * before_image of this block will be written to snapshot file
					 */
					cw_set[cw_set_depth-1].level = CSE_LEVEL_DRT_LVL0_FREE;
					mark_level_as_special = TRUE;
				} else
					mark_level_as_special = FALSE;
#				endif
				bmphist.blk_num = bit_map;
				if (NULL == (bmphist.buffaddr = t_qread(bmphist.blk_num, (sm_int_ptr_t)&bmphist.cycle,
									&bmphist.cr)))
				{
					t_retry((enum cdb_sc)rdfail_detail);
					continue;
				}
				t_write_map(&bmphist, (uchar_ptr_t)update_array, ctn, -(int4)(nextblk - blk));
#				ifdef GTM_SNAPSHOT
				if (mark_level_as_special)
				{
					/* The special level value will be used later in gvcst_map_build to set the block to be
					 * freed as free rather than recycled
					 */
					cw_set[cw_set_depth-1].level = CSE_LEVEL_DRT_LVL0_FREE;
				}
#				endif
				UNIX_ONLY(DEBUG_ONLY(lcl_t_tries = t_tries));
				if ((trans_num)0 == (ret_tn = t_end(&alt_hist, NULL, TN_NOT_SPECIFIED)))
				{
#					ifdef UNIX
					assert((CDB_STAGNATE == t_tries) || (lcl_t_tries == t_tries - 1));
					status = LAST_RESTART_CODE;
					if ((cdb_sc_onln_rlbk1 == status) || (cdb_sc_onln_rlbk2 == status)
						|| TREF(rlbk_during_redo_root))
					{	/* t_end restarted due to online rollback. Discard bitmap free-up and return control
						 * to the application. But, before that reset only_reset_clues_if_onln_rlbk to FALSE
						 */
						TREF(in_gvcst_bmp_mark_free) = FALSE;
						send_msg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_IGNBMPMRKFREE, 4,
								REG_LEN_STR(gv_cur_region), DB_LEN_STR(gv_cur_region));
						t_abort(gv_cur_region, cs_addrs);
						return ret_tn; /* actually 0 */
					}
#					endif
					continue;
				}
				break;
			}
			if (0 == ret_tn) /* db format change occurred. Fall through to below for loop to visit each block */
			{
				/* Abort any active transaction to get rid of lingering Non-TP artifacts */
				t_abort(gv_cur_region, cs_addrs);
				break;
			}
		}
	}	/* for all blocks in the kill_set */
	for ( ; blk < blk_top; blk++)
	{	/* Database has NOT been completely upgraded. Have to read every block that is going to be freed
		 * and determine whether it has been upgraded or not. Every block will be freed as part of one
		 * separate update to the bitmap. This will cause as many transactions as the blocks are being freed.
		 * But this overhead will be present only as long as the database is not completely upgraded.
		 * The reason why every block is updated separately is in order to accurately maintain the "blks_to_upgrd"
		 * counter in the database file-header when the block-freeup phase (2nd phase) of the M-kill proceeds
		 * concurrently with a MUPIP REORG UPGRADE/DOWNGRADE. If the bitmap is not updated for every block freeup
		 * then MUPIP REORG UPGRADE/DOWNGRADE should also upgrade/downgrade all blocks in one bitmap as part of
		 * one transaction (only then will we avoid double-decrement of "blks_to_upgrd" counter by the M-kill as
		 * well as the MUPIP REORG UPGRADE/DOWNGRADE). That is a non-trivial task as potentially 512 blocks need
		 * to be modified as part of one non-TP transaction which is unnecessarily making it heavyweight. Compared
		 * to that, incurring a per-block bitmap update overhead in the M-kill is considered acceptable since this
		 * will be the case only as long as we are in compatibility mode which should be hopefully not for long.
		 */
		if (0 != blk->flag)
			continue;
		assert(0 < blk->block);
		assert((int4)blk->block < cs_addrs->ti->total_blks);
		assert(!IS_BITMAP_BLK(blk->block));
		bit_map = ROUND_DOWN2((int)blk->block, BLKS_PER_LMAP);
		assert(dba_bg == cs_addrs->hdr->acc_meth);
		/* We need to check each block we are deleting to see if it is in the format of a previous version.
		 * If it is, then "csd->blks_to_upgrd" needs to be correspondingly adjusted.
		 */
		alt_hist.h[0].level = 0;	/* Initialize for loop below */
		alt_hist.h[1].blk_num = 0;
		alt_hist.h[0].blk_target = NULL;		/* need to initialize for calls to T_END */
		CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
		assert((block_id)blk->block - bit_map);
		assert(SIZEOF(block_id) == SIZEOF(blk_ident));
		*((block_id *)update_array_ptr) = ((block_id)blk->block - bit_map);
		update_array_ptr += SIZEOF(blk_ident);
		/* the following assumes SIZEOF(blk_ident) == SIZEOF(int) */
		assert(SIZEOF(blk_ident) == SIZEOF(int));
		*(int *)update_array_ptr = 0;
		t_begin(ERR_GVKILLFAIL, UPDTRNS_DB_UPDATED_MASK);
		for (;;)
		{
			ctn = cs_addrs->ti->curr_tn;
			alt_hist.h[0].cse     = NULL;
			alt_hist.h[0].tn      = ctn;
			alt_hist.h[0].blk_num = blk->block;
			if (NULL == (alt_hist.h[0].buffaddr = t_qread(alt_hist.h[0].blk_num,
								      (sm_int_ptr_t)&alt_hist.h[0].cycle,
								      &alt_hist.h[0].cr)))
			{
				t_retry((enum cdb_sc)rdfail_detail);
				continue;
			}
			/* IF csd->db_got_to_v5_once is FALSE
			 *	a) mark the block as FREE (not RECYCLED to avoid confusing MUPIP REORG UPGRADE with a
			 *		block that was RECYCLED right at the time of MUPIP UPGRADE from a V4 to V5 version).
			 *		MUPIP REORG UPGRADE will mark all existing RECYCLED blocks as FREE.
			 *	b) need to write PBLK
			 * ELSE
			 *	a) mark this block as RECYCLED
			 *	b) no need to write PBLK (it will be written when the block later gets reused).
			 * ENDIF
			 *
			 * Create a cw-set-element with mode gds_t_busy2free that will cause a PBLK to be written in t_end
			 * (the value csd->db_got_to_v5_once will be checked while holding crit) only in the IF case above.
			 * At the same time bg_update will NOT be invoked for this cw-set-element so this block will not be
			 * touched. But the corresponding bitmap block will be updated as part of the same transaction (see
			 * t_write_map below) to mark this block as FREE or RECYCLED depending on whether csd->db_got_to_v5_once
			 * is FALSE or TRUE (actual check done in gvcst_map_build and sec_shr_map_build).
			 */
			t_busy2free(&alt_hist.h[0]);
			cr = alt_hist.h[0].cr;
			ondsk_blkver = cr->ondsk_blkver;	/* Get local copy in case cr->ondsk_blkver changes between
								 * first and second part of the ||
								 */
			assert((GDSV6 == ondsk_blkver) || (GDSV4 == ondsk_blkver));
			if (GDSVCURR != ondsk_blkver)
				inctn_detail.blknum_struct.blknum = blk->block;
			else
				inctn_detail.blknum_struct.blknum = 0; /* i.e. no adjustment to "blks_to_upgrd" necessary */
			bmphist.blk_num = bit_map;
			if (NULL == (bmphist.buffaddr = t_qread(bmphist.blk_num, (sm_int_ptr_t)&bmphist.cycle,
								&bmphist.cr)))
			{
				t_retry((enum cdb_sc)rdfail_detail);
				continue;
			}
			t_write_map(&bmphist, (uchar_ptr_t)update_array, ctn, -1);
#			ifdef GTM_SNAPSHOT
			if ((MUSWP_FREE_BLK == TREF(in_mu_swap_root_state)) && blk->level)
			{
				assert(1 == ks->used);
				cw_set[cw_set_depth-1].level = CSE_LEVEL_DRT_LVL0_FREE; /* special level for gvcst_map_build */
				cw_set[cw_set_depth-2].level = CSE_LEVEL_DRT_LVL0_FREE; /* special level for t_end */
				/* Here we do not need to do BIT_SET_DIR_TREE because later the block will be always written to
				 * snapshot file without checking whether it belongs to DIR or GV tree
				 */
			}
#			endif
			UNIX_ONLY(DEBUG_ONLY(lcl_t_tries = t_tries));
			if ((trans_num)0 == (ret_tn = t_end(&alt_hist, NULL, TN_NOT_SPECIFIED)))
			{
#				ifdef UNIX
				assert((CDB_STAGNATE == t_tries) || (lcl_t_tries == t_tries - 1));
				assert(0 < t_tries);
				DEBUG_ONLY(status = LAST_RESTART_CODE); /* get the recent restart code */
				/* We don't expect online rollback related retries because we are here with the database NOT fully
				 * upgraded. This means, online rollback cannot even start (it issues ORLBKNOV4BLK). Assert that.
				 */
				assert((cdb_sc_onln_rlbk1 != status) && (cdb_sc_onln_rlbk2 != status));
#				endif
				continue;
			}
			break;
		}
	}	/* for all blocks in the kill_set */
	TREF(in_gvcst_bmp_mark_free) = FALSE;
	return ret_tn;
}

