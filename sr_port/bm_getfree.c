/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/**************************************************************************************
*
*		MODULE NAME:			BM_GETFREE.C
*
*		CALLING SEQUENCE:		block_id bm_getfree(hint, blk_used, cw_work, cs, cw_depth_ptr)
*
*		DESCRIPTION:	Takes a block id as a hint and tries to find a
*		free block, looking first at the hint, then in the same local
*		bitmap, and finally in every local map.  If it finds a free block,
*		it looks for the bitmap in the working set, puts it in the working
*		set if it is not there, and updates the map used, and marks it with
*		the transaction number, and updates the boolean
*		pointed to by blk_used to indicate if the free block had been used previously.
*
*		HISTORY:
*
***************************************************************************************/
#include "mdef.h"

#include "gtm_string.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "min_max.h"
#include "gdsblkops.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdskill.h"	/* needed for tp.h */
#include "jnl.h"	/* needed for tp.h */
#include "buddy_list.h"	/* needed for tp.h */
#include "tp.h"		/* needed for ua_list for ENSURE_UPDATE_ARRAY_SPACE macro */
#include "iosp.h"
#include "bmm_find_free.h"

/* Include prototypes */
#include "wcs_mm_recover.h"
#include "t_qread.h"
#include "t_write_map.h"
#include "bit_clear.h"
#include "send_msg.h"
#include "bm_getfree.h"
#include "gdsfilext.h"
#include "anticipatory_freeze.h"
#include "interlock.h"

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF char		*update_array, *update_array_ptr;
GBLREF gd_region	*gv_cur_region;
GBLREF unsigned char	rdfail_detail;
GBLREF uint4		dollar_tlevel;
GBLREF uint4		update_array_size, cumul_update_array_size;
GBLREF unsigned int	t_tries;
GBLREF jnlpool_addrs_ptr_t	jnlpool;

#ifdef DEBUG
GBLREF	block_id	ydb_skip_bml_num;
#endif

error_def(ERR_DBBADFREEBLKCTR);
error_def(ERR_DBMBMINCFREFIXED);

block_id bm_getfree(block_id hint_arg, boolean_t *blk_used, unsigned int cw_work, cw_set_element *cs, int *cw_depth_ptr)
{
	cw_set_element	*cs1;
	sm_uc_ptr_t	bmp = NULL;
	block_id	bml, extension_size, hint, hint_cycled, hint_limit, lcnt, local_maps, offset;
	block_id_ptr_t	b_ptr;
	gd_region	*baseDBreg;
	int		cw_set_top, depth = -1;
	unsigned int	n_decrements = 0;
	trans_num	ctn = 0;
	int4		free_bit, map_size = 0, new_allocation, status;
	ublock_id	total_blks;
	uint4		space_needed, was_crit;
	sgmnt_addrs	*baseDBcsa;
	srch_blk_status	blkhist;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	total_blks = (dba_mm == cs_data->acc_meth) ? cs_addrs->total_blks : cs_addrs->ti->total_blks;
	hint = (((ublock_id)hint_arg >= total_blks) ? 1 : hint_arg);	/* avoid any chance of treating TP chain.flag as signed */
#	ifdef DEBUG
	if ((0 != ydb_skip_bml_num) && (BLKS_PER_LMAP <= hint) && (hint < ydb_skip_bml_num))
		hint = ydb_skip_bml_num;
#	endif
	hint_cycled = DIVIDE_ROUND_UP(total_blks, BLKS_PER_LMAP);
	hint_limit = DIVIDE_ROUND_DOWN(hint, BLKS_PER_LMAP);
	local_maps = hint_cycled + 2;	/* for (up to) 2 wraps */
	lcnt = local_maps + 1;
	do
	{
		bml = bmm_find_free(hint / BLKS_PER_LMAP, (sm_uc_ptr_t)MM_ADDR(cs_data), local_maps);
		if ((NO_FREE_SPACE == bml) || (bml >= hint_cycled))
		{	/* if no free space or might have looped to original map, extend */
			if ((NO_FREE_SPACE != bml) && (hint_limit < hint_cycled))
			{
				hint_cycled = hint_limit;
				hint = 1;
#				ifdef DEBUG
				if ((0 != ydb_skip_bml_num) && (BLKS_PER_LMAP <= hint) && (hint < ydb_skip_bml_num))
					hint = ydb_skip_bml_num;
#				endif
				continue;
			}
			was_crit = cs_addrs->now_crit;
			if (!was_crit)
			{	/* We are working up to a file extension, which requires crit anyway, we need a consistent
				 * view of our last minute checks, and in case we are updating statsdb_allocation we want
				 * that to be atomic with the extension itself. Strictly speaking, statsdb_allocation isn't
				 * associated with the current (statsdb) region, but barring a concurrent MUPIP SET this should
				 * be sufficient.
				 */
#				ifdef DEBUG
				if ((WBTEST_ENABLED(WBTEST_MM_CONCURRENT_FILE_EXTEND) && dollar_tlevel
						&& !MEMCMP_LIT(gv_cur_region->rname, "DEFAULT"))
					|| (WBTEST_ENABLED(WBTEST_WSSTATS_PAUSE) && (10 == ydb_white_box_test_case_count)
						&& !MEMCMP_LIT(gv_cur_region->rname, "DEFAULT")))
				{	/* Sync with copy in gdsfilext()
					 * Unset the env shouldn't affect the parent, it reads env just once at process startup.
					 */
					if(WBTEST_ENABLED(WBTEST_WSSTATS_PAUSE))
						unsetenv("gtm_white_box_test_case_enable");
					SYSTEM("$gtm_dist/mumps -run $gtm_wbox_mrtn");
					assert(1 == cs_addrs->nl->wbox_test_seq_num);	/* should have been set by mubfilcpy */
					/* signal mupip backup to stop sleeping in mubfilcpy */
					cs_addrs->nl->wbox_test_seq_num = 2;
				}
#				endif
				while (!cs_addrs->now_crit)
				{
					grab_crit(gv_cur_region, WS_12);
					if (FROZEN_CHILLED(cs_addrs))
						DO_CHILLED_AUTORELEASE(cs_addrs, cs_data);
					assert(FROZEN(cs_data) || !cs_addrs->jnlpool || (cs_addrs->jnlpool == jnlpool));
					if (FROZEN(cs_data) || IS_REPL_INST_FROZEN)
					{
						rel_crit(gv_cur_region);
						while (FROZEN(cs_data) || IS_REPL_INST_FROZEN)
						{
							hiber_start(1000);
							if (FROZEN_CHILLED(cs_addrs) && CHILLED_AUTORELEASE(cs_addrs))
								break;
						}
					}
				}
			}
			if (total_blks != cs_addrs->ti->total_blks)
			{	/* File extension or MM switch detected. Rather than try to reset the loop, just recurse. */
				CHECK_MM_DBFILEXT_REMAP_IF_NEEDED(cs_addrs, gv_cur_region);
				if (!was_crit)
					rel_crit(gv_cur_region);
				return (dba_mm == cs_data->acc_meth)
						? FILE_EXTENDED
						: bm_getfree(hint_arg, blk_used, cw_work, cs, cw_depth_ptr);
			}
			if (IS_STATSDB_CSA(cs_addrs))
			{	/* Double the allocation size for statsdb regions to reduce tp_restart calls */
				assert(cs_addrs->total_blks == cs_addrs->ti->total_blks);
				extension_size = cs_addrs->total_blks;
			} else
				extension_size = cs_data->extension_size;
#			ifdef DEBUG
			TREF(in_bm_getfree_gdsfilext) = TRUE;
#			endif
			status = GDSFILEXT(extension_size, total_blks, TRANS_IN_PROG_TRUE);
#			ifdef DEBUG
			TREF(in_bm_getfree_gdsfilext) = FALSE;
#			endif
			if (SS_NORMAL != status)
			{
				if (!was_crit)
					rel_crit(gv_cur_region);
				if (EXTEND_SUSPECT == status)
					continue;
				return status;
			}
			if (IS_STATSDB_CSA(cs_addrs))
			{	/* Save the new allocation size to the database file header.
				 * Use this allocation size the next time we create statsdb for the region.
				 */
				new_allocation = (int4)(total_blks * 2);
				STATSDBREG_TO_BASEDBREG(gv_cur_region, baseDBreg);
				baseDBcsa = &FILE_INFO(baseDBreg)->s_addrs;
				assert(new_allocation > baseDBcsa->hdr->statsdb_allocation);
				baseDBcsa->hdr->statsdb_allocation = new_allocation;
			}
			if (!was_crit)
				rel_crit(gv_cur_region);
			if (dba_mm == cs_data->acc_meth)
				return (FILE_EXTENDED);
			hint = total_blks;
			total_blks = cs_addrs->ti->total_blks;
			hint_cycled = DIVIDE_ROUND_UP(total_blks, BLKS_PER_LMAP);
			local_maps = hint_cycled + 2;	/* for (up to) 2 wraps */
			/*
			 * note that you can make an optimization of not going back over the whole database and going over
			 * only the extended section. but since it is very unlikely that a free block won't be found
			 * in the extended section and the fact that we are starting from the extended section in either
			 * approach and the fact that we have an assertpro to check that we don't have a lot of
			 * free blocks while doing an extend and the fact that it is very easy to make the change to do
			 * a full-pass, the full-pass solution is currently being implemented
			 */
			lcnt = local_maps + 2;	/* allow it one extra pass to ensure that it can take advantage of the entension */
			n_decrements++;	/* used only for debugging purposes */
			continue;
		}
		bml *= BLKS_PER_LMAP;
#		ifdef DEBUG
		if ((0 != ydb_skip_bml_num) && (BLKS_PER_LMAP <= bml) && (bml < ydb_skip_bml_num))
		{
			hint = ydb_skip_bml_num + 1;
			continue;
		}
#		endif
		if (ROUND_DOWN2(hint, BLKS_PER_LMAP) != bml)
		{	/* not within requested map */
			if ((bml < hint) && (hint_cycled))	/* wrap? - second one should force an extend for sure */
				hint_cycled = (hint_limit < hint_cycled) ? hint_limit: 0;
			hint = bml + 1;				/* start at beginning */
		}
		if (ROUND_DOWN2(total_blks, BLKS_PER_LMAP) == bml)
		{	/* Can be cast because result of (total_blks - bml) should never be larger then BLKS_PER_LMAP */
			assert(BLKS_PER_LMAP >= (total_blks - bml));
			map_size = (int4)(total_blks - bml);
		} else
			map_size = BLKS_PER_LMAP;
		if (dollar_tlevel)
		{
			depth = cw_work;
			cw_set_top = *cw_depth_ptr;
			if (depth < cw_set_top)
				tp_get_cw(cs, cw_work, &cs1);
			for (; depth < cw_set_top;  depth++, cs1 = cs1->next_cw_set)
			{	/* do tp front to back because list is more efficient than tp_get_cw and forward pointers exist */
				if (bml == cs1->blk)
				{
					TRAVERSE_TO_LATEST_CSE(cs1);
					break;
				}
			}
			if (depth >= cw_set_top)
			{
				assert(cw_set_top == depth);
				depth = 0;
			}
		} else
		{
			for (depth = *cw_depth_ptr - 1; depth >= cw_work; depth--)
			{	/* do non-tp back to front, because of adjacency */
				if (bml == (cs + depth)->blk)
				{
					cs1 = cs + depth;
					break;
				}
			}
			if (depth < cw_work)
			{
				assert(cw_work - 1 == depth);
				depth = 0;
			}
		}
		if (0 == depth)
		{
			ctn = cs_addrs->ti->curr_tn;
			if (!(bmp = t_qread(bml, (sm_int_ptr_t)&blkhist.cycle, &blkhist.cr)))
				return MAP_RD_FAIL;
			if ((BM_SIZE(BLKS_PER_LMAP) != ((blk_hdr_ptr_t)bmp)->bsiz) || (LCL_MAP_LEVL != ((blk_hdr_ptr_t)bmp)->levl))
			{
				assert(CDB_STAGNATE > t_tries);
				rdfail_detail = cdb_sc_badbitmap;
				return MAP_RD_FAIL;
			}
			offset = 0;
		} else
		{
			bmp = cs1->old_block;
			b_ptr = cs1->upd_addr.map;
			b_ptr += cs1->reference_cnt - 1;
			offset = *b_ptr + 1;
		}
		if (offset < map_size)
		{	/* offset can be downcast because to get here it must be less then map_size
			 * which is constrained to never be larger then BLKS_PER_LMAP
			 */
			assert(offset == (int4)offset);
			free_bit = bm_find_blk((int4)offset, (sm_uc_ptr_t)bmp + SIZEOF(blk_hdr), map_size, blk_used);
			if (MAP_RD_FAIL == free_bit)
				return MAP_RD_FAIL;
		} else
			free_bit = NO_FREE_SPACE;
		if (NO_FREE_SPACE != free_bit)
			break;
		if ((hint = (bml + BLKS_PER_LMAP)) >= total_blks)	/* if map is full, start at 1st blk in next map */
		{	/* wrap - second one should force an extend for sure */
			hint = 1;
			if (hint_cycled)
				hint_cycled = (hint_limit < hint_cycled) ? hint_limit: 0;
		}
		if ((0 == depth) && cs_addrs->now_crit)	/* if it's from the cw_set, its state is murky */
		{
			assert(FALSE);
			send_msg_csa(CSA_ARG(cs_addrs) VARLSTCNT(3) ERR_DBMBMINCFREFIXED, 1, bml);
			bit_clear(bml / BLKS_PER_LMAP, MM_ADDR(cs_data)); /* repair master map error */
			if (bml > cs_addrs->nl->highest_lbm_blk_changed)
				cs_addrs->nl->highest_lbm_blk_changed = bml;	    /* Retain high-water mark */
		}
	} while (0 < --lcnt);
	/* If not in the final retry, it is possible that free_bit is >= map_size, e.g., if the buffer holding the bitmap block
	 * gets recycled with a non-bitmap block in which case the bit that bm_find_blk returns could be greater than map_size.
	 * But, this should never happen in final retry.
	 */
	assert(map_size);
	if ((map_size <= free_bit) && (CDB_STAGNATE <= t_tries))
	{	/* Bad free bit. */
		assert((NO_FREE_SPACE == free_bit) && (!lcnt));	/* All maps full, should have extended */
		assertpro(FALSE);
	}
	assert(0 <= free_bit);	/* or else we would be allocating a block in the previous bit map */
	assert(0 <= depth);
	if (0 != depth)
	{
		b_ptr = cs1->upd_addr.map;
		b_ptr += cs1->reference_cnt++;
		*b_ptr = free_bit;
	} else
	{
		space_needed = (BLKS_PER_LMAP + 1) * SIZEOF(block_id);
		if (dollar_tlevel)
		{
			ENSURE_UPDATE_ARRAY_SPACE(space_needed);	/* have brackets for "if" for macros */
		}
		BLK_ADDR(b_ptr, space_needed, block_id);
		memset(b_ptr, 0, space_needed);
		*b_ptr = (block_id)free_bit;
		assert(bmp);
		blkhist.blk_num = bml;
		blkhist.buffaddr = bmp;	/* cycle and cr have already been assigned from t_qread */
		t_write_map(&blkhist, b_ptr, ctn, 1); /* last parameter 1 is what cs->reference_cnt gets set to */
	}
	return (bml + free_bit);
}

/* This routine returns whether the free_blocks counter in the file-header is ok (TRUE) or not (FALSE).
 * If not, it corrects it. This assumes cs_addrs, cs_data and gv_cur_region to point to the region of interest.
 * It also assumes that the master-map is correct and finds out non-full local bitmaps and counts the number of
 * free blocks in each of them and sums them up to determine the perceived correct free_blocks count.
 * The reason why this is ok is that even if the master-map incorrectly reports a local bitmap as full, our new free_blocks
 * count will effectively make the free space in that local-bitmap invisible and make a gdsfilext necessary and valid.
 * A later mupip integ will scavenge that invisible space for us. The worst that can therefore happen is that we will transiently
 * not be using up existing space. But we will always ensure that the free_blocks counter goes in sync with the master-map.
 */
boolean_t	is_free_blks_ctr_ok(void)
{
	block_id	bml, free_bml, local_maps, total_blks, free_blocks;
	boolean_t	blk_used;
	cache_rec_ptr_t	cr;
	int4		free_bit, maxbitsthismap, cycle;
	sm_uc_ptr_t	bmp;

	assert(&FILE_INFO(gv_cur_region)->s_addrs == cs_addrs && cs_addrs->hdr == cs_data && cs_addrs->now_crit);
	total_blks = (dba_mm == cs_data->acc_meth) ? cs_addrs->total_blks : cs_addrs->ti->total_blks;
	local_maps = DIVIDE_ROUND_UP(total_blks, BLKS_PER_LMAP);
	for (free_blocks = 0, free_bml = 0; free_bml < local_maps; free_bml++)
	{
		bml = bmm_find_free((uint4)free_bml, (sm_uc_ptr_t)MM_ADDR(cs_data), local_maps);
		if (bml < free_bml)
			break;
		free_bml = bml;
		bml *= BLKS_PER_LMAP;
		if (!(bmp = t_qread(bml, (sm_int_ptr_t)&cycle, &cr))
				|| (BM_SIZE(BLKS_PER_LMAP) != ((blk_hdr_ptr_t)bmp)->bsiz)
				|| (LCL_MAP_LEVL != ((blk_hdr_ptr_t)bmp)->levl))
		{
			assert(FALSE);	/* In pro, we will simply skip counting this local bitmap. */
			continue;
		}
		assert(free_bml <= (local_maps - 1));
		/* Can be cast because result of (total_blks - bml) should never be larger then BLKS_PER_LMAP */
		DEBUG_ONLY(if(free_bml == (local_maps - 1)) assert(BLKS_PER_LMAP >= (total_blks - bml)));
		maxbitsthismap = (free_bml != (local_maps - 1)) ? BLKS_PER_LMAP : (int4)(total_blks - bml);
		for (free_bit = 0; free_bit < maxbitsthismap; free_bit++)
		{
			free_bit = bm_find_blk(free_bit, (sm_uc_ptr_t)bmp + SIZEOF(blk_hdr), maxbitsthismap, &blk_used);
			assert(NO_FREE_SPACE <= free_bit);
			if (0 > free_bit)
				break;
			free_blocks++;
		}
	}
#	ifdef DEBUG
	if (0 != ydb_skip_bml_num)
		free_blocks += (ydb_skip_bml_num - BLKS_PER_LMAP) / BLKS_PER_LMAP * (BLKS_PER_LMAP - 1);
#	endif
	assert(cs_addrs->ti->free_blocks == free_blocks);
	if (cs_addrs->ti->free_blocks != free_blocks)
	{
		assert(FALSE);
		send_msg_csa(CSA_ARG(cs_addrs) VARLSTCNT(6) ERR_DBBADFREEBLKCTR, 4, DB_LEN_STR(gv_cur_region),
				&(cs_addrs->ti->free_blocks), &free_blocks);
		cs_addrs->ti->free_blocks = free_blocks;
		return FALSE;
	}
	return TRUE;
}
