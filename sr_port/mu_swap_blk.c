/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*********************************************************************************
mu_swap_blk.c:
	This program will swap the working block with a destination block (dest_blk_id).
	The destination block is the block id, where a block should go for
	better performance of database.   This destination block Id is picked
	sequntially starting from block number 3.
	It will NOT swap a block with with
		a) root block of the GVT and
		b) a bitmap block,
		c) a block from other GVT which should be unchnaged as
		   a result of mentioning the global in EXCLUDE option
		d) parent block (because it is against the pre-order traversal)
	This module :
		Reads dest_blk_id;
		Reads first key from it or, its descendent;
		Checks if it is part of a dir_tree;
		call gvcst_search for the GVT under which it belongs
		Finally calls t_writes to create blocks.
************************************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblkops.h"
#include "gdskill.h"
#include "gdscc.h"
#include "gdsbml.h"
#include "jnl.h"
#include "copy.h"
#include "muextr.h"
#include "mu_reorg.h"
#include "cws_insert.h"
#include "gvt_inline.h"
#include "tp.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_create.h"
#include "t_write_map.h"
#include "mupip_reorg.h"
#include "gvcst_protos.h"	/* for gvcst_search prototype */
#include "jnl_get_checksum.h"

GBLREF gv_namehead	*gv_target;
GBLREF gv_namehead	*reset_gv_target;
GBLREF gv_namehead	*reorg_gv_target;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF char		*update_array, *update_array_ptr;
GBLREF uint4		update_array_size;	/* for the BLK_* macros */
GBLREF uint4		t_err;
GBLREF cw_set_element	cw_set[];
GBLREF unsigned char	cw_map_depth;
GBLREF unsigned char	cw_set_depth;
GBLREF unsigned char	rdfail_detail;
GBLREF unsigned int	t_tries;
GBLREF gv_key		*gv_currkey;
GBLREF hash_table_int8	cw_stagnate;

#ifdef DEBUG
GBLREF	block_id	ydb_skip_bml_num;
#endif

/******************************************************************************************
Input Parameters:
	level: level of working block
	dest_blk_id: last destination used for swap
Output Parameters:
	kill_set_ptr: Kill set to be freed
	*exclude_glist_ptr: List of globals not to be moved for a swap destination
Input/Output Parameters:
	gv_target : as working block's history
	reorg_gv_target->hist : as destination block's history
 ******************************************************************************************/
enum cdb_sc mu_swap_blk(int level, block_id *pdest_blk_id, kill_set *kill_set_ptr, glist *exclude_glist_ptr, block_id upg_mv_block)
{
	blk_segment		*bs1, *bs_ptr;
	block_id		child1, child2, dest_blk_id, work_blk_id;
	boolean_t		blk_was_free, deleted, gbl_target_was_set, temp_long_blk_id;
	boolean_t		dest_long_blk_id = FALSE, dest_long_parent_id, work_long_blk_id, work_long_parent_id;
	cache_rec_ptr_t		dest_child_cr;
	cw_set_element		*tmpcse;
	enum cdb_sc		status;
	enum db_ver		dest_blk_ver, work_blk_ver;
	gv_namehead		*save_targ;
	int			blk_seg_cnt, blk_size, first_offset, key_len, key_len_dir, nslevel, piece_len1, piece_len2,
				rec_size1, rec_size2, second_offset, wlevel;
	int			dest_blk_id_sz = 0, dest_blk_level = -1, dest_blk_size, dest_child_cycle;
	int			dest_parent_id_sz, dest_parent_size = 0;
	int			work_blk_id_sz, work_blk_size;
	int			work_parent_id_sz, work_parent_size;
	jnl_buffer_ptr_t	jbbp;					/* jbbp is non-NULL only if before-image journaling */
	sgmnt_addrs		*csa;
	sm_uc_ptr_t		bmp_buff, bn_ptr, dest_blk_ptr = NULL, dest_parent_ptr = NULL, key_base, rec_base, saved_blk,
				tblk_ptr, work_blk_ptr, work_parent_ptr;
	srch_blk_status		bmlhist, destblkhist, *hist_ptr, *work_blk_stat;
	srch_hist		*dest_hist_ptr, *dir_hist_ptr;
	trans_num		ctn;
	unsigned char		save_cw_set_depth, x_blk_lmap;
	unsigned int		bsiz;
	unsigned short		temp_ushort;
	boolean_t		is_bg;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	dest_blk_id = *pdest_blk_id;
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	if (NULL == TREF(gv_reorgkey))
		GVKEY_INIT(TREF(gv_reorgkey), DBKEYSIZE(MAX_KEY_SZ));
	dest_hist_ptr = &(reorg_gv_target->hist);
	dir_hist_ptr = reorg_gv_target->alt_hist;
	blk_size = cs_data->blk_size;
	work_parent_ptr = gv_target->hist.h[level+1].buffaddr;
	work_parent_size = ((blk_hdr_ptr_t)work_parent_ptr)->bsiz;
	work_long_parent_id = IS_64_BLK_ID(work_parent_ptr);
	work_parent_id_sz = SIZEOF_BLK_ID(work_long_parent_id);
	work_blk_stat = &gv_target->hist.h[level];
	work_blk_ptr = work_blk_stat->buffaddr;
	work_blk_size = ((blk_hdr_ptr_t)work_blk_ptr)->bsiz;
	work_blk_ver = ((blk_hdr_ptr_t)work_blk_ptr)->bver;
	work_blk_id = work_blk_stat->blk_num;
	assert(!upg_mv_block || (upg_mv_block == work_blk_id));
	work_long_blk_id = IS_64_BLK_ID(work_blk_ptr);
	work_blk_id_sz = SIZEOF_BLK_ID(work_long_blk_id);
	if (blk_size < work_blk_size)
	{
		assert(CDB_STAGNATE > t_tries);
		NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
		return cdb_sc_blkmod;
	}
	cws_reorg_remove_index = 0;
	csa = cs_addrs;
	/*===== Infinite loop to find the destination block =====*/
	do
	{
		blk_was_free = FALSE;
		INCR_BLK_NUM(dest_blk_id);
#		ifdef DEBUG
		if ((0 != ydb_skip_bml_num) && (BLKS_PER_LMAP < dest_blk_id) && (dest_blk_id < ydb_skip_bml_num))
		{
			dest_blk_id = ydb_skip_bml_num;
			continue;
		}
#		endif
		/* A Pre-order traversal should not cause a child block to go to its parent.
		 * However, in case it happens because already the organization was like that or for any other reason, skip swap.
		 * If we decide to swap, code below should be changed to take care of the special case.
		 * Still a grand-child can go to its grand-parent. This is rare and following code can handle it.
		 */
		if (dest_blk_id == gv_target->hist.h[level+1].blk_num)
		{
			assert(0 == upg_mv_block);
			continue;
		}
		if (cs_data->trans_hist.total_blks <= dest_blk_id || dest_blk_id == work_blk_id)
		{
			assert(0 == upg_mv_block);
			*pdest_blk_id = dest_blk_id;
			return cdb_sc_oprnotneeded;
		}
		ctn = csa->ti->curr_tn;
		/* We need to save the block numbers that were NEWLY ADDED (since entering this function "mu_swap_blk")
		 * through the CWS_INSERT macro (in db_csh_get/db_csh_getn which can be called by t_qread or gvcst_search below).
		 * This is so that we can delete these blocks from the "cw_stagnate" hashtable in case we determine the need to
		 * choose a different "dest_blk_id" in this for loop (i.e. come to the next iteration). If these blocks are not
		 * deleted, then the hashtable will keep growing (a good example will be if -EXCLUDE qualifier is specified and
		 * a lot of prospective dest_blk_ids get skipped because they contain EXCLUDEd global variables) and very soon
		 * the hashtable will contain more entries than there are global buffers and at that point db_csh_getn will not
		 * be able to get a free global buffer for a new block (since it checks the "cw_stagnate" hashtable before reusing
		 * a buffer in case of MUPIP REORG). To delete these previous iteration blocks, we use the "cws_reorg_remove_array"
		 * variable. This array should have enough entries to accommodate the maximum number of blocks that can be t_qread
		 * in one iteration down below. And that number is the sum of
		 *	+     MAX_BT_DEPTH : for the t_qread while loop down the tree done below
		 *	+ 2 * MAX_BT_DEPTH : for the two calls to gvcst_search done below
		 *	+ 2                : 1 for the t_qread of dest_blk_id and 1 more for the t_qread of a
		 *			     bitmap block done inside the call to get_lmap below
		 *	= 3 * MAX_BT_DEPTH + 2
		 * To be safe, we give a buffer of MAX_BT_DEPTH elements i.e. (4 * MAX_BT_DEPTH) + 2.
		 * This is defined in the macro CWS_REMOVE_ARRAYSIZE in cws_insert.h
		 */
		/* reset whatever blocks the previous iteration of this for loop had filled in the cw_stagnate hashtable */
		for ( ; cws_reorg_remove_index > 0; cws_reorg_remove_index--)
		{
			deleted = delete_hashtab_int8(&cw_stagnate, (ublock_id *)&cws_reorg_remove_array[cws_reorg_remove_index]);
			assert(deleted);
		}
		/* read corresponding bitmap block before attempting to read destination  block.
		 * if bitmap indicates block is free, we will not read the destination block
		 */
		bmp_buff = get_lmap(dest_blk_id, &x_blk_lmap, (sm_int_ptr_t)&bmlhist.cycle, &bmlhist.cr);
		if (!bmp_buff || BLK_MAPINVALID == x_blk_lmap ||
			((blk_hdr_ptr_t)bmp_buff)->bsiz != BM_SIZE(BLKS_PER_LMAP) ||
			((blk_hdr_ptr_t)bmp_buff)->levl != LCL_MAP_LEVL)
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_badbitmap;
		}
		if ((BLK_FREE != x_blk_lmap) || (0 != upg_mv_block))
		{	/* x_blk_lmap is either BLK_BUSY or BLK_RECYCLED. In either case, we need to read destination block
			 * in case we later detect that the before-image needs to be written.
			 */
			if (!(dest_blk_ptr = t_qread(dest_blk_id, (sm_int_ptr_t)&destblkhist.cycle, &destblkhist.cr)))
			{
				assert(0 == upg_mv_block);
				assert(CDB_STAGNATE > t_tries);
				return (enum cdb_sc)rdfail_detail;
			}
			destblkhist.blk_num = dest_blk_id;
			destblkhist.buffaddr = dest_blk_ptr;
			destblkhist.level = dest_blk_level = (upg_mv_block) ? level : ((blk_hdr_ptr_t)dest_blk_ptr)->levl;
			if (csa->now_crit)
				destblkhist.tn = ctn;
			dest_long_blk_id = IS_64_BLK_ID(dest_blk_ptr);
			dest_blk_id_sz = SIZEOF_BLK_ID(dest_long_blk_id);
		}
		if (BLK_BUSY != x_blk_lmap)
		{	/* x_blk_map is either BLK_FREE or BLK_RECYCLED both of which mean the block is not used in the bitmap */
			blk_was_free = TRUE;
			dest_parent_id_sz = 0;
			dest_long_parent_id = FALSE;
			break;
		} else
			assert(0 == upg_mv_block);
		/* dest_blk_id might contain a *-record only.
		 * So follow the pointer to go to the data/index block, which has a non-* key to search.
		 */
		assert(0 <= dest_blk_level);
		nslevel = dest_blk_level;
		if (MAX_BT_DEPTH <= nslevel)
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_maxlvl;
		}
		assert(dest_blk_ptr);
		rec_base = dest_blk_ptr + SIZEOF(blk_hdr);
		GET_RSIZ(rec_size1, rec_base);
		tblk_ptr = dest_blk_ptr;
		temp_long_blk_id = dest_long_blk_id;
		while ((bstar_rec_size(temp_long_blk_id) == rec_size1) && (0 != nslevel))
		{
			READ_BLK_ID(temp_long_blk_id, &child1, rec_base + SIZEOF(rec_hdr));
			if (0 == child1 || child1 > cs_data->trans_hist.total_blks - 1)
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_rdfail;
			}
			if (!(tblk_ptr = t_qread(child1, (sm_int_ptr_t)&dest_child_cycle, &dest_child_cr)))
			{
				assert(CDB_STAGNATE > t_tries);
				return (enum cdb_sc)rdfail_detail;
			}
			temp_long_blk_id = IS_64_BLK_ID(tblk_ptr);
			/* leaf of a killed GVT can have block header only.   Skip those blocks */
			if (SIZEOF(blk_hdr) >= ((blk_hdr_ptr_t)tblk_ptr)->bsiz)
				break;
			nslevel--;
			rec_base = tblk_ptr + SIZEOF(blk_hdr);
			GET_RSIZ(rec_size1, rec_base);
		}
		/* leaf of a killed GVT can have block header only.   Skip those blocks */
		if (SIZEOF(blk_hdr) >= ((blk_hdr_ptr_t)tblk_ptr)->bsiz)
		{
			assert((0 == upg_mv_block) || (SIZEOF(blk_hdr) == ((blk_hdr_ptr_t)tblk_ptr)->bsiz));
			continue;
		}
		/* get length of global variable name (do not read subscript) for dest_blk_id */
		key_len_dir = get_gblname_len(tblk_ptr, rec_base + SIZEOF(rec_hdr));
		/* key_len = length of 1st key value (including subscript) for dest_blk_id */
		key_len = get_key_len(tblk_ptr, rec_base + SIZEOF(rec_hdr));
		if ((1 >= key_len_dir || MAX_MIDENT_LEN + 1 < key_len_dir) || (2 >= key_len || MAX_KEY_SZ < key_len))
		{	/* Earlier used to restart here always. But dest_blk_id can be a block,
			 * which is just killed and still marked busy.  Skip it, if we are in last retry.
			 */
			if (CDB_STAGNATE <= t_tries)
				continue;
			else
			{
				NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
				return cdb_sc_blkmod;
			}
		}
		memcpy(&((TREF(gv_reorgkey))->base[0]), rec_base + SIZEOF(rec_hdr), key_len_dir);
		(TREF(gv_reorgkey))->base[key_len_dir] = 0;
		(TREF(gv_reorgkey))->end = key_len_dir;
		if ((NULL != exclude_glist_ptr) && exclude_glist_ptr->next)
		{	/* exclude blocks for globals in the list of EXCLUDE option */
			assert(0 == upg_mv_block);
			if  (in_exclude_list(&((TREF(gv_reorgkey))->base[0]), key_len_dir - 1, exclude_glist_ptr))
				continue;
		}
		save_targ = gv_target;
		if (INVALID_GV_TARGET != reset_gv_target)
			gbl_target_was_set = TRUE;
		else
		{
			gbl_target_was_set = FALSE;
			reset_gv_target = save_targ;
		}
		gv_target = reorg_gv_target;
		gv_target->root = csa->dir_tree->root;
		gv_target->clue.end = 0;
		/* assign Directory tree path to find dest_blk_id in dir_hist_ptr */
		status = gvcst_search(TREF(gv_reorgkey), dir_hist_ptr);
		if (cdb_sc_normal != status)
		{
			assert(CDB_STAGNATE > t_tries);
			RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ, DO_GVT_GVKEY_CHECK);
			return status;
		}
		if (dir_hist_ptr->h[0].curr_rec.match != (TREF(gv_reorgkey))->end + 1)
		{	/* may be in a kill_set of another process */
			assert((0 == upg_mv_block) || (dir_hist_ptr->h[0].curr_rec.match == (TREF(gv_reorgkey))->end + 1));
			RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ, DO_GVT_GVKEY_CHECK);
			continue;
		}
		for (wlevel = 0; (wlevel <= dir_hist_ptr->depth) && (dir_hist_ptr->h[wlevel].blk_num != dest_blk_id); wlevel++)
			;
		if (dir_hist_ptr->h[wlevel].blk_num == dest_blk_id)
		{	/* do not use a dir_tree block as the destination of a swap */
			assert(0 == upg_mv_block);
			RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ, DO_GVT_GVKEY_CHECK);
			continue;
		}
		/* gv_reorgkey gets the first key from dest_blk_id, or if that's a *-key, from some descendent block */
		memcpy(&((TREF(gv_reorgkey))->base[0]), rec_base + SIZEOF(rec_hdr), key_len);
		(TREF(gv_reorgkey))->end = key_len - 1;
		key_base = dir_hist_ptr->h[0].buffaddr + dir_hist_ptr->h[0].curr_rec.offset + SIZEOF(rec_hdr);
		key_len_dir = get_key_len(dir_hist_ptr->h[0].buffaddr, key_base);
		/* Get root of GVT for dest_blk_id */
		READ_BLK_ID(IS_64_BLK_ID(dir_hist_ptr->h[0].buffaddr), &(gv_target->root), key_base + key_len_dir);
		if ((0 == gv_target->root) || (gv_target->root > (cs_data->trans_hist.total_blks - 1)))
		{
			assert(CDB_STAGNATE > t_tries);
			RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ, DO_GVT_GVKEY_CHECK);
			NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
			return cdb_sc_blkmod;
		}
		/* Assign Global Variable Tree path to find dest_blk_id in dest_hist_ptr */
		gv_target->clue.end = 0;
		status = gvcst_search(TREF(gv_reorgkey), dest_hist_ptr);
		RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ, DO_GVT_GVKEY_CHECK);
		if (cdb_sc_normal != status)
		{
			assert(CDB_STAGNATE > t_tries);
			return status;
		}
		is_bg = (dba_bg == cs_data->acc_meth);
		if  (is_bg && (destblkhist.cr->cycle != destblkhist.cycle))
		{	/* cr that the first t_qread() done above returned got reused for a different block
			 * by later calls to t_qread() or gvcst_search() before this point. Possible for
			 * example if we are running with a small "gtm_poollimit" env var setting.
			 */
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_lostcr;
		}
		if (dest_blk_level >= dest_hist_ptr->depth			/* do not swap in root level */
			|| dest_hist_ptr->h[dest_blk_level].blk_num != dest_blk_id) /* must be in a kill set of another process. */
		{
			assert(0 == upg_mv_block);
			continue;
		}
		if (dest_hist_ptr->h[nslevel].curr_rec.match != ((TREF(gv_reorgkey))->end + 1))
		{
			assert(CDB_STAGNATE > t_tries);
			NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
			return cdb_sc_blkmod;
		}
		for (wlevel = nslevel; wlevel <= dest_blk_level; wlevel++)
			dest_hist_ptr->h[wlevel].tn = ctn;
		dest_blk_ptr = dest_hist_ptr->h[dest_blk_level].buffaddr;
		dest_blk_size = ((blk_hdr_ptr_t)dest_blk_ptr)->bsiz;
		dest_blk_ver = ((blk_hdr_ptr_t)dest_blk_ptr)->bver;
		dest_long_blk_id = IS_64_BLK_ID(dest_blk_ptr);
		dest_blk_id_sz = SIZEOF_BLK_ID(dest_long_blk_id);
		dest_parent_ptr = dest_hist_ptr->h[dest_blk_level+1].buffaddr;
		dest_parent_size = ((blk_hdr_ptr_t)dest_parent_ptr)->bsiz;
		dest_long_parent_id = IS_64_BLK_ID(dest_parent_ptr);
		dest_parent_id_sz = SIZEOF_BLK_ID(dest_long_parent_id);
		if (0 >= dest_parent_size)
		{
			assert(CDB_STAGNATE > t_tries);
			NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
			return cdb_sc_blkmod;
		}
		break;
	} while(TRUE);
	/*===== End of indefinite loop to find the destination block =====*/
	/*-----------------------------------------------------
	   Now modify blocks for swapping. Maximum of 4 blocks.
	   -----------------------------------------------------*/
	if (!blk_was_free)
	{	/* 1: dest_blk_id into work_blk_id */
		assert(dest_blk_ptr);
		assert(0 == upg_mv_block);
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, dest_blk_ptr + SIZEOF(blk_hdr), dest_blk_size - SIZEOF(blk_hdr));
		if (!BLK_FINI (bs_ptr,bs1))
		{
			assert(CDB_STAGNATE > t_tries);
			NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
			return cdb_sc_blkmod;
		}
		assert(gv_target->hist.h[level].blk_num == work_blk_id);
		assert(gv_target->hist.h[level].buffaddr == work_blk_ptr);
		tmpcse = t_write(&gv_target->hist.h[level], bs1, 0, 0,
					dest_blk_level, TRUE, TRUE, GDS_WRITE_KILLTN);
		assert(!cs_data->fully_upgraded || (tmpcse->ondsk_blkver == dest_blk_ver) || (0 == level));
		tmpcse->ondsk_blkver = dest_blk_ver;
	}
	/* 2: work_blk_id into dest_blk_id */
	if (!blk_was_free && (work_blk_id == dest_hist_ptr->h[dest_blk_level+1].blk_num))
	{	/* work_blk_id will be swapped with its child.
		 * This is the only vertical swap.  Here working block goes to its child.
		 * Working block cannot goto its parent because of traversal
		 */
		assert((0 == upg_mv_block));
		if ((dest_blk_level + 1 != level) || (dest_parent_size != work_blk_size))
		{
			assert(CDB_STAGNATE > t_tries);
			NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
			return cdb_sc_blkmod;
		}
		assert(dest_parent_size);
		assert(dest_parent_ptr);
		BLK_INIT(bs_ptr, bs1);
		BLK_ADDR(saved_blk, dest_parent_size, unsigned char);
		memcpy(saved_blk, dest_parent_ptr, dest_parent_size);
		first_offset = dest_hist_ptr->h[dest_blk_level+1].curr_rec.offset;
		GET_RSIZ(rec_size1, saved_blk + first_offset);
		if (work_blk_size < first_offset + rec_size1)
		{
			assert(CDB_STAGNATE > t_tries);
			NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
			return cdb_sc_blkmod;
		}
		piece_len1 =  first_offset + rec_size1;
		BLK_SEG(bs_ptr, saved_blk + SIZEOF(blk_hdr), piece_len1 - work_blk_id_sz - SIZEOF(blk_hdr));
		BLK_ADDR(bn_ptr, work_blk_id_sz, unsigned char);
		/* since work_blk_id will now be the child of dest_blk_id */
		WRITE_BLK_ID(work_long_blk_id, work_blk_id, bn_ptr);
		BLK_SEG(bs_ptr, bn_ptr, work_blk_id_sz);
		BLK_SEG(bs_ptr, saved_blk + piece_len1, dest_parent_size - piece_len1);
		if (!BLK_FINI(bs_ptr, bs1))
		{
			assert(CDB_STAGNATE > t_tries);
			NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
			return cdb_sc_blkmod;
		}
		assert(dest_blk_id == dest_hist_ptr->h[dest_blk_level].blk_num);
		assert(dest_blk_ptr == dest_hist_ptr->h[dest_blk_level].buffaddr);
		tmpcse = t_write(&dest_hist_ptr->h[dest_blk_level], bs1, 0, 0,
				level, TRUE, TRUE, GDS_WRITE_KILLTN);
		assert(!cs_data->fully_upgraded || (tmpcse->ondsk_blkver == work_blk_ver) || (0 == level));
		tmpcse->ondsk_blkver = work_blk_ver;
	} else /* free block or, when working block does not move vertically (swap with parent/child) */
	{
		BLK_INIT(bs_ptr, bs1);
		BLK_ADDR(saved_blk, work_blk_size, unsigned char);
		memcpy(saved_blk, work_blk_ptr, work_blk_size);
		BLK_SEG(bs_ptr, saved_blk + SIZEOF(blk_hdr), work_blk_size - SIZEOF(blk_hdr));
		if (!BLK_FINI(bs_ptr, bs1))
		{
			assert(CDB_STAGNATE > t_tries);
			NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
			return cdb_sc_blkmod;
		}
		if (blk_was_free)
		{
			tmpcse = &cw_set[cw_set_depth];
			t_create(dest_blk_id, bs1, 0, 0, level);
			/* Although we invoked t_create, we do not want t_end to allocate the block (i.e. change mode
			 * from gds_t_create to gds_t_acquired). Instead we do that and a little more (that t_end does) all here.
			 */
			assert(dest_blk_id == tmpcse->blk);
			SET_CSE_MODE_TO_GDS_T_ACQUIRED(tmpcse);
			/* If snapshots are in progress, we might want to read the before images of the FREE blocks also.
			 * Since mu_swap_blk mimics a small part of t_end, it sets cse->mode to gds_t_acquired and hence
			 * will not read the before images of the FREE blocks in t_end. To workaround this, set
			 * cse->was_free to TRUE so that in t_end, this condition can be used to read the before images of
			 * the FREE blocks if needed.
			 */
			(BLK_FREE == x_blk_lmap) ? BIT_SET_FREE(tmpcse->blk_prior_state) : BIT_CLEAR_FREE(tmpcse->blk_prior_state);
			/* No need to write before-image in case the block is FREE. */
			if (BLK_FREE == x_blk_lmap)
				tmpcse->old_block = NULL;
			else
			{	/* Destination is a recycled block that needs a before image */
				tmpcse->old_block = destblkhist.buffaddr;
				/* Record cr,cycle. This is used later in t_end to determine if checksums need to be recomputed */
				tmpcse->cr = destblkhist.cr;
				tmpcse->cycle = destblkhist.cycle;
				jbbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
				if ((NULL != jbbp) && (((blk_hdr_ptr_t)tmpcse->old_block)->tn < jbbp->epoch_tn))
				{	/* Compute CHECKSUM for writing PBLK record before getting crit.
					 * It is possible that we are reading a block that is actually marked free in
					 * the bitmap (due to concurrency issues at this point). Therefore we might be
					 * actually reading uninitialized block headers and in turn a bad value of
					 * "old_block->bsiz". Restart if we ever access a buffer whose size is greater
					 * than the db block size.
					 */
					 assert(0 == upg_mv_block);
					bsiz = ((blk_hdr_ptr_t)(tmpcse->old_block))->bsiz;
					if (bsiz > blk_size)
					{
						assert(CDB_STAGNATE > t_tries);
						return cdb_sc_lostbmlcr;
					}
					JNL_GET_CHECKSUM_ACQUIRED_BLK(tmpcse, cs_data, csa, tmpcse->old_block, bsiz);
				}
			}
			assert(cs_data->desired_db_format == tmpcse->ondsk_blkver);	/* should have been set by t_create above */
		} else
		{
			assert(0 == upg_mv_block);
			hist_ptr = &dest_hist_ptr->h[dest_blk_level];
			assert(dest_blk_id == hist_ptr->blk_num);
			assert(dest_blk_ptr == hist_ptr->buffaddr);
			tmpcse = t_write(hist_ptr, bs1, 0, 0, level, TRUE, TRUE, GDS_WRITE_KILLTN);
		}
		assert(!cs_data->fully_upgraded || (tmpcse->ondsk_blkver == work_blk_ver) || (0 == level));
		tmpcse->ondsk_blkver = work_blk_ver;
	}
	if (!blk_was_free)
	{	/* 3: Parent of destination block (may be parent of working block too) */
		assert(0 == upg_mv_block);
		if (gv_target->hist.h[level+1].blk_num == dest_hist_ptr->h[dest_blk_level+1].blk_num)
		{	/* dest parent == work_blk parent */
			BLK_INIT(bs_ptr, bs1);
			/* Interchange pointer to dest_blk_id and work_blk_id */
			if (level != dest_blk_level ||
				gv_target->hist.h[level+1].curr_rec.offset == dest_hist_ptr->h[level+1].curr_rec.offset)
			{
				assert(CDB_STAGNATE > t_tries);
				NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
				return cdb_sc_blkmod;
			}
			if (gv_target->hist.h[level+1].curr_rec.offset < dest_hist_ptr->h[level+1].curr_rec.offset)
			{
				first_offset = gv_target->hist.h[level+1].curr_rec.offset;
				second_offset = dest_hist_ptr->h[level+1].curr_rec.offset;
			} else
			{
				first_offset = dest_hist_ptr->h[level+1].curr_rec.offset;
				second_offset = gv_target->hist.h[level+1].curr_rec.offset;
			}
			assert(dest_parent_size);
			assert(dest_parent_ptr);
			GET_RSIZ(rec_size1, dest_parent_ptr + first_offset);
			GET_RSIZ(rec_size2, dest_parent_ptr + second_offset);
			if ((dest_parent_size < first_offset + rec_size1) ||
				(dest_parent_size < second_offset + rec_size2) ||
				(bstar_rec_size(dest_long_parent_id) >= rec_size1) ||
				(bstar_rec_size(dest_long_parent_id) > rec_size2))
			{
				assert(CDB_STAGNATE > t_tries);
				NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
				return cdb_sc_blkmod;
			}
			piece_len1 =  first_offset + rec_size1 - dest_parent_id_sz;
			piece_len2 =  second_offset + rec_size2 - dest_parent_id_sz;
			READ_BLK_ID(dest_long_parent_id, &child1, dest_parent_ptr + piece_len1);
			READ_BLK_ID(dest_long_parent_id, &child2, dest_parent_ptr + piece_len2);
			BLK_SEG(bs_ptr, dest_parent_ptr + SIZEOF(blk_hdr), piece_len1 - SIZEOF(blk_hdr));
			BLK_ADDR(bn_ptr, dest_parent_id_sz, unsigned char);
			WRITE_BLK_ID(dest_long_parent_id, child2, bn_ptr);
			BLK_SEG(bs_ptr, bn_ptr, dest_parent_id_sz);
			BLK_SEG(bs_ptr, dest_parent_ptr + first_offset + rec_size1,
				second_offset + rec_size2 - dest_parent_id_sz - first_offset - rec_size1);
			BLK_ADDR(bn_ptr, dest_parent_id_sz, unsigned char);
			WRITE_BLK_ID(dest_long_parent_id, child1, bn_ptr);
			BLK_SEG(bs_ptr, bn_ptr, dest_parent_id_sz);
			BLK_SEG(bs_ptr, dest_parent_ptr + second_offset + rec_size2,
				dest_parent_size - second_offset - rec_size2);
			if (!BLK_FINI(bs_ptr,bs1))
			{
				assert(CDB_STAGNATE > t_tries);
				NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
				return cdb_sc_blkmod;
			}
			assert(level == dest_blk_level);
			assert(dest_parent_ptr == dest_hist_ptr->h[level+1].buffaddr);
			t_write(&dest_hist_ptr->h[level+1], bs1, 0, 0, level+1, FALSE, TRUE, GDS_WRITE_KILLTN);
		} else if (work_blk_id != dest_hist_ptr->h[dest_blk_level+1].blk_num)
		{	/* Destination block moved into the position of working block.
			 * So destination block's parent's pointer should be changed to work_blk_id
			 */
			BLK_INIT(bs_ptr, bs1);
			assert(dest_parent_size);
			assert(dest_parent_ptr);
			GET_RSIZ(rec_size1, dest_parent_ptr + dest_hist_ptr->h[dest_blk_level+1].curr_rec.offset);
			if ((dest_parent_size < (rec_size1 + dest_hist_ptr->h[dest_blk_level+1].curr_rec.offset)) ||
				(bstar_rec_size(dest_long_parent_id) > rec_size1))
			{
				assert(CDB_STAGNATE > t_tries);
				NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
				return cdb_sc_blkmod;
			}
			BLK_SEG (bs_ptr, dest_parent_ptr + SIZEOF(blk_hdr),
			    dest_hist_ptr->h[dest_blk_level+1].curr_rec.offset + rec_size1 - SIZEOF(blk_hdr) - dest_parent_id_sz);
			BLK_ADDR(bn_ptr, dest_parent_id_sz, unsigned char);
			WRITE_BLK_ID(dest_long_parent_id, work_blk_id, bn_ptr);
			BLK_SEG(bs_ptr, bn_ptr, dest_parent_id_sz);
			BLK_SEG(bs_ptr, dest_parent_ptr + dest_hist_ptr->h[dest_blk_level+1].curr_rec.offset + rec_size1,
				dest_parent_size - dest_hist_ptr->h[dest_blk_level+1].curr_rec.offset - rec_size1);
			if (!BLK_FINI(bs_ptr,bs1))
			{
				assert(CDB_STAGNATE > t_tries);
				NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
				return cdb_sc_blkmod;
			}
			assert(dest_parent_ptr == dest_hist_ptr->h[dest_blk_level+1].buffaddr);
			t_write(&dest_hist_ptr->h[dest_blk_level+1], bs1, 0, 0, dest_blk_level+1,
				FALSE, TRUE, GDS_WRITE_KILLTN);
		}
	}
	/* 4: Parent of working block, if different than destination's parent or, destination was a free block */
	if (blk_was_free || gv_target->hist.h[level+1].blk_num != dest_hist_ptr->h[dest_blk_level+1].blk_num)
	{	/* Parent block of working blk should correctly point the working block. Working block went to dest_blk_id  */
		GET_RSIZ(rec_size1, (work_parent_ptr + gv_target->hist.h[level+1].curr_rec.offset));
		if (work_parent_size < rec_size1 +  gv_target->hist.h[level+1].curr_rec.offset ||
				bstar_rec_size(work_long_parent_id) > rec_size1)
		{
			assert((CDB_STAGNATE > t_tries) && (0 == upg_mv_block));
			NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
			return cdb_sc_blkmod;
		}
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, work_parent_ptr + SIZEOF(blk_hdr),
			gv_target->hist.h[level+1].curr_rec.offset + rec_size1 - SIZEOF(blk_hdr) - work_parent_id_sz);
		BLK_ADDR(bn_ptr, work_parent_id_sz, unsigned char);
		WRITE_BLK_ID(work_long_parent_id, dest_blk_id, bn_ptr);
		BLK_SEG(bs_ptr, bn_ptr, work_parent_id_sz);
		BLK_SEG(bs_ptr, work_parent_ptr + gv_target->hist.h[level+1].curr_rec.offset + rec_size1,
			work_parent_size - gv_target->hist.h[level+1].curr_rec.offset - rec_size1);
		if (!BLK_FINI(bs_ptr, bs1))
		{
			assert(CDB_STAGNATE > t_tries);
			NONTP_TRACE_HIST_MOD(work_blk_stat, t_blkmod_mu_swap_blk);
			return cdb_sc_blkmod;
		}
		assert(gv_target->hist.h[level+1].buffaddr == work_parent_ptr);
		t_write(&gv_target->hist.h[level+1], bs1, 0, 0, level+1, FALSE, TRUE, GDS_WRITE_KILLTN);
	}
	/* else already taken care of, when dest_blk_id moved */
	if (blk_was_free)
	{	/* A free/recycled block will become busy block.
		 * So the local bitmap must be updated.
		 * Local bit map block will be added in the list of update arrray for concurrency check and
		 * 	also the cw_set element will be created to mark the free/recycled block as free.
		 * kill_set_ptr will save the block which will become free.
		 */
		child1 = ROUND_DOWN2(dest_blk_id, BLKS_PER_LMAP); /* bit map block */
		bmlhist.buffaddr = bmp_buff;
		bmlhist.blk_num = child1;
		child1 = dest_blk_id - child1;
		assert(child1);
		PUT_BLK_ID(update_array_ptr, child1);
		/* Need to put bit maps on the end of the cw set for concurrency checking.
		 * We want to simulate t_write_map, except we want to update "cw_map_depth" instead of "cw_set_depth".
		 * Hence the save and restore logic (for "cw_set_depth") below.
		 */
		save_cw_set_depth = cw_set_depth;
		assert(!cw_map_depth);
		t_write_map(&bmlhist, (block_id *)update_array_ptr, ctn, 1);	/* will increment cw_set_depth */
		cw_map_depth = cw_set_depth;		/* set cw_map_depth to the latest cw_set_depth */
		cw_set_depth = save_cw_set_depth;	/* restore cw_set_depth */
		/* t_write_map simulation end */
		update_array_ptr += SIZEOF(block_id);
		child1 = 0;
		PUT_BLK_ID(update_array_ptr, child1);
		update_array_ptr += SIZEOF(block_id);
		assert(1 == cw_set[cw_map_depth - 1].reference_cnt);	/* 1 free block is now becoming BLK_USED in the bitmap */
		/* working block will be removed */
		kill_set_ptr->blk[kill_set_ptr->used].flag = 0;
		kill_set_ptr->blk[kill_set_ptr->used].level = 0;
		kill_set_ptr->blk[kill_set_ptr->used++].block = work_blk_id;
	}
	if (0 != upg_mv_block)
		reorg_gv_target->hist.h[dest_blk_level] = destblkhist;
	*pdest_blk_id = dest_blk_id;
	return cdb_sc_normal;
}

/***************************************************************
Checks if an input global name is present in exclude global lists.
	curr_key_ptr = Key pointer
	key_len = curr_key_ptr length excludeing nulls
	exclude_glist_ptr = list of globals in -EXCLUDE option
Returns:
	TRUE if key is also present in list of exclude_glist_ptr
	FALSE Otherwise
***************************************************************/
boolean_t in_exclude_list(unsigned char *curr_key_ptr, int key_len, glist *exclude_glist_ptr)
{
	glist *gl_ptr;

	for (gl_ptr = exclude_glist_ptr->next; gl_ptr; gl_ptr = gl_ptr->next)
	{
		if ((GNAME(gl_ptr).len == key_len)
				&& (0 == memcmp(GNAME(gl_ptr).addr, curr_key_ptr, key_len)))
			return TRUE;
	}
	return FALSE;
}
