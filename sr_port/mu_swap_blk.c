/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*********************************************************************************
mu_swap_blk.c:
	This program will swap the working block with a destinition block (dest_blk_id).
	The destinition block is the block id, where a block should go for
	better performance of database.   This destinition block Id is picked
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
#include "hashtab.h"
#include "copy.h"
#include "muextr.h"
#include "mu_reorg.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "mupip_reorg.h"
#include "gvcst_search.h"

GBLREF gv_namehead	*gv_target;
GBLREF gv_namehead	*reset_gv_target;
GBLREF gv_namehead	*reorg_gv_target;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF char		*update_array, *update_array_ptr;
GBLREF int		update_array_size;
GBLREF uint4		t_err;
GBLREF cw_set_element 	cw_set[];
GBLREF unsigned char    cw_map_depth;
GBLREF unsigned char	cw_set_depth;
GBLREF unsigned char	rdfail_detail;
GBLREF unsigned int     t_tries;
GBLREF gv_key 		*gv_currkey;
GBLREF hashtab          *cw_stagnate;

/******************************************************************************************
Input Parameters:
	level: level of working block
	dest_blk_id: last destinition used for swap
Output Parameters:
	kill_set_ptr: Kill set to be freed
	*exclude_glist_ptr: List of globals not to be moved for a swap destination
Input/Output Parameters:
	gv_target : as working block's history
	reorg_gv_target->hist : as desitnitions block's history
 ******************************************************************************************/
enum cdb_sc mu_swap_blk(int level, block_id *pdest_blk_id, kill_set *kill_set_ptr, glist *exclude_glist_ptr)
{
	bool			blk_was_free;
	unsigned char		free_blk_cw_depth, x_blk_lmap;
	unsigned short		temp_ushort;
	int			rec_size1, rec_size2;
	int			wlevel, nslevel, dest_blk_level;
	int			piece_len1, piece_len2, first_offset, second_offset,
				work_blk_size, work_parent_size, dest_blk_size, dest_parent_size;
	int			map_cycle, dest_cycle;
	int			blk_seg_cnt, blk_size;
	uint4			dummy, save_t_err;
	trans_num		ctn;
	int			key_len, key_len_dir;
	block_id		dest_blk_id, work_blk_id, child1, child2;
	enum cdb_sc		status;
	static gv_key		*dest_gv_currkey;
	srch_hist 		*dest_hist_ptr;
	cache_rec_ptr_t		map_cr, dest_cr;
	blk_segment		*bs1, *bs_ptr;
	sm_uc_ptr_t		saved_blk, work_blk_ptr, work_parent_ptr, dest_parent_ptr, dest_blk_ptr,
				bn_ptr, bmp_buff, tblk_ptr, rec_base, rPtr1;
	boolean_t		gbl_target_was_set;
	gv_namehead		*save_targ;

	error_def(ERR_GVKILLFAIL);

	dest_blk_id = *pdest_blk_id;
	assert(update_array != NULL);
	update_array_ptr = update_array;
	if (NULL == dest_gv_currkey)
	{
		dest_gv_currkey = (gv_key *)malloc(sizeof(gv_key) + MAX_KEY_SZ + 1);
		dest_gv_currkey->top = MAX_KEY_SZ;
	}
	dest_hist_ptr = &(reorg_gv_target->hist);
	blk_size = cs_data->blk_size;
	work_parent_ptr = gv_target->hist.h[level+1].buffaddr;
	work_parent_size = ((blk_hdr_ptr_t)work_parent_ptr)->bsiz;
	work_blk_ptr = gv_target->hist.h[level].buffaddr;
	work_blk_size = ((blk_hdr_ptr_t)work_blk_ptr)->bsiz;
	work_blk_id = gv_target->hist.h[level].blk_num;
	if (sizeof(blk_hdr) >= work_blk_size || blk_size < work_blk_size)
	{
		assert(t_tries < CDB_STAGNATE);
		return cdb_sc_blkmod;
	}
	/*===== Infinite loop to find the destinition block =====*/
	for ( ; ; )
	{
		blk_was_free = FALSE;
		INCR_BLK_NUM(dest_blk_id);
		/* A Pre-order traversal should not cause a child block to go to its parent.
		   However, in case it happens because already the organization was like that or
		   for any other reason , skip swap. If we decide to swap, code below should be
		   changed to take care of the special case.
		   Still a grand-children can got to grand-parent. This is rare and
		   following code can handle it.  */
		if (dest_blk_id == gv_target->hist.h[level+1].blk_num)
			continue;
		if (cs_data->trans_hist.total_blks - 1 < dest_blk_id || dest_blk_id == work_blk_id)
		{
			*pdest_blk_id = dest_blk_id;
			return cdb_sc_oprnotneeded;
		}
		ctn = cs_addrs->ti->curr_tn;
		/* read destinition block */
		if (!(dest_blk_ptr = t_qread(dest_blk_id, (sm_int_ptr_t)&dest_cycle, &dest_cr)))
		{
			assert(t_tries < CDB_STAGNATE);
			return rdfail_detail;
		}
		bmp_buff = get_lmap(dest_blk_id, &x_blk_lmap, (sm_int_ptr_t)&map_cycle, &map_cr);
		if (!bmp_buff || BLK_MAPINVALID == x_blk_lmap ||
			((blk_hdr_ptr_t)bmp_buff)->bsiz != BM_SIZE(BLKS_PER_LMAP) ||
			((blk_hdr_ptr_t)bmp_buff)->levl != LCL_MAP_LEVL)
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_badbitmap;
		}
		if (BLK_BUSY != x_blk_lmap)
		{
			blk_was_free = TRUE;
			dest_blk_size = ((blk_hdr_ptr_t)dest_blk_ptr)->bsiz;
			break;
		}

		/*
		dest_blk_id might contain a *-record only.
		So follow the pointer to go to the data/index block,
		which has a non-* key to search.
		*/
		nslevel = dest_blk_level = ((blk_hdr_ptr_t)dest_blk_ptr)->levl;
		rec_base = dest_blk_ptr + sizeof(blk_hdr);
		GET_RSIZ(rec_size1, rec_base);
		tblk_ptr = dest_blk_ptr;
		while (BSTAR_REC_SIZE == rec_size1 && 0 != nslevel)
                {
                        GET_LONG(child1, (rec_base + sizeof(rec_hdr)));
			if (0 == child1 || child1 > cs_data->trans_hist.total_blks - 1)
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_rdfail;
			}
                        if (!(tblk_ptr = t_qread(child1, (sm_int_ptr_t)&dest_cycle, &dest_cr)))
                        {
				assert(t_tries < CDB_STAGNATE);
				return rdfail_detail;
                        }
			/* leaf of a killed GVT can have block header only.   Skip those blocks */
			if (sizeof(blk_hdr) >= ((blk_hdr_ptr_t)tblk_ptr)->bsiz)
			{
				/* if (CDB_STAGNATE <= t_tries) */
				if (cw_stagnate)
					del_hashtab_ent(&cw_stagnate, (void *)child1, &dummy);
				break;
			}
                        nslevel--;
                        rec_base = tblk_ptr + sizeof(blk_hdr);
			GET_RSIZ(rec_size1, rec_base);
                }
		/* leaf of a killed GVT can have block header only.   Skip those blocks */
		if (sizeof(blk_hdr) >= ((blk_hdr_ptr_t)tblk_ptr)->bsiz)
		{
			/* if (CDB_STAGNATE <= t_tries) */
			if (cw_stagnate)
				del_hashtab_ent(&cw_stagnate, (void *)(dest_blk_id), &dummy);
			continue;
		}

		/* get length of global variable name (do not read subscript) for dest_blk_id */
		GET_GBLNAME_LEN(key_len_dir, rec_base + sizeof(rec_hdr));
		/* key_len = length of 1st key value (including subscript) for dest_blk_id */
		GET_KEY_LEN(key_len, rec_base + sizeof(rec_hdr));
                if (1 >= key_len_dir || 2 >= key_len || MAX_KEY_SZ < key_len)
                {
			assert(t_tries < CDB_STAGNATE);
                        return cdb_sc_blkmod;
                }
                memcpy(&(dest_gv_currkey->base[0]), rec_base + sizeof(rec_hdr), key_len_dir);
                dest_gv_currkey->base[key_len_dir] = 0;
                dest_gv_currkey->end = key_len_dir;
		if (exclude_glist_ptr->next)
		{
			/* exclude blocks for globals in the list of EXCLUDE option */
			if  (in_exclude_list(&(dest_gv_currkey->base[0]), key_len_dir - 1, exclude_glist_ptr))
			{
				/* if (CDB_STAGNATE <= t_tries) */
				if (cw_stagnate)
					del_hashtab_ent(&cw_stagnate, (void *)(dest_blk_id), &dummy);
				continue;
			}
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
		gv_target->root = cs_addrs->dir_tree->root;
		reorg_gv_target->clue.end = gv_target->clue.end = 0;
		/* assign Directort tree path to find dest_blk_id in dest_hist_ptr */
                status = gvcst_search(dest_gv_currkey, dest_hist_ptr);
		if (cdb_sc_normal != status)
                {
			assert(t_tries < CDB_STAGNATE);
			RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ);
			return status;
                }
		if (dest_hist_ptr->h[0].curr_rec.match != dest_gv_currkey->end + 1)
			/* may be in a kill_set of another process */
		{
			RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ);
			continue;
		}
		for (wlevel = 0; wlevel <= dest_hist_ptr->depth &&
			dest_hist_ptr->h[wlevel].blk_num != dest_blk_id; wlevel++);
		if (dest_hist_ptr->h[wlevel].blk_num == dest_blk_id) /* do not swap a dir_tree block */
		{
			RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ);
			continue;
		}
		/* dest_gv_currkey will now have the first key from dest_blk_id,
		   or, from a descendant of dest_blk_id (in case it had a *-key only).  */
                memcpy(&(dest_gv_currkey->base[0]), rec_base + sizeof(rec_hdr), key_len);
                dest_gv_currkey->end  =  key_len - 1;
		GET_KEY_LEN(key_len_dir, dest_hist_ptr->h[0].buffaddr + dest_hist_ptr->h[0].curr_rec.offset + sizeof(rec_hdr));
		/* Get root of GVT for dest_blk_id */
		GET_LONG(gv_target->root,
			dest_hist_ptr->h[0].buffaddr + dest_hist_ptr->h[0].curr_rec.offset + sizeof(rec_hdr) + key_len_dir);
		if ((0 == gv_target->root) || (gv_target->root > cs_data->trans_hist.total_blks - 1))
                {
			assert(t_tries < CDB_STAGNATE);
			RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ);
			return cdb_sc_blkmod;
                }
		/* Assign Global Variable Tree path to find dest_blk_id in dest_hist_ptr */
		reorg_gv_target->clue.end = gv_target->clue.end = 0;
		status = gvcst_search(dest_gv_currkey, dest_hist_ptr);
		RESET_GV_TARGET_LCL_AND_CLR_GBL(save_targ);
		if (dest_blk_level >= dest_hist_ptr->depth || /* do not swap in root level */
			dest_hist_ptr->h[dest_blk_level].blk_num != dest_blk_id) /* must be in a kill set of another process. */
			continue;
		if (cdb_sc_normal != status ||
			dest_hist_ptr->h[nslevel].curr_rec.match != dest_gv_currkey->end + 1)
                {
			assert(t_tries < CDB_STAGNATE);
			return (cdb_sc_normal != status ? status : cdb_sc_blkmod);
                }
		for (wlevel = nslevel; wlevel <= dest_blk_level; wlevel++)
			dest_hist_ptr->h[wlevel].tn = ctn;
		dest_blk_ptr = dest_hist_ptr->h[dest_blk_level].buffaddr;
		dest_blk_size = ((blk_hdr_ptr_t)dest_blk_ptr)->bsiz;
		dest_parent_ptr = dest_hist_ptr->h[dest_blk_level+1].buffaddr;
		dest_parent_size = ((blk_hdr_ptr_t)dest_parent_ptr)->bsiz;
		break;
	}
	/*===== End of infinite loop to find the destinition block =====*/


	/*-----------------------------------------------------
	   Now modify blocks for swapping. Maximum of 4 blocks.
	   -----------------------------------------------------*/
	if (!blk_was_free)
	{
		/*
		1: dest_blk_id into work_blk_id
		*/
		BLK_INIT(bs_ptr, bs1);
		BLK_ADDR(saved_blk, dest_blk_size, unsigned char);
		memcpy(saved_blk, dest_blk_ptr, dest_blk_size);
		BLK_SEG (bs_ptr, saved_blk + sizeof(blk_hdr), dest_blk_size - sizeof(blk_hdr));
                if (!BLK_FINI (bs_ptr,bs1))
                {
                        assert(t_tries < CDB_STAGNATE);
                        return cdb_sc_blkmod;
                }
		if (dest_blk_level || level)
		{
			save_t_err = t_err;
			t_err = ERR_GVKILLFAIL;
		}
                t_write(work_blk_id, (unsigned char *)bs1, 0, 0, work_blk_ptr, dest_blk_level, TRUE, TRUE);
		if (dest_blk_level || level)
			t_err = save_t_err;
	}
	/*
	2: work_blk_id into dest_blk_id
	*/
	if (!blk_was_free &&
		work_blk_id == dest_hist_ptr->h[dest_blk_level+1].blk_num) /* work_blk_id will be swapped with its child */
	{
		/* Note: This is the only vertical swap.  Here working block goes to its child.
			 Working block cannot goto its parent because of traversal */
		if (dest_blk_level + 1 != level || dest_parent_size != work_blk_size)
		{
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_blkmod;
		}
		BLK_INIT(bs_ptr, bs1);
		BLK_ADDR(saved_blk, dest_parent_size, unsigned char);
		memcpy(saved_blk, dest_parent_ptr, dest_parent_size);
		first_offset = dest_hist_ptr->h[dest_blk_level+1].curr_rec.offset;
		GET_RSIZ(rec_size1, saved_blk + first_offset);
		if (work_blk_size < first_offset + rec_size1)
		{
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_blkmod;
		}
		piece_len1 =  first_offset + rec_size1;
		BLK_SEG(bs_ptr, saved_blk + sizeof(blk_hdr), piece_len1 - sizeof(block_id) - sizeof(blk_hdr));
		BLK_ADDR(bn_ptr, sizeof(block_id), unsigned char);
		PUT_LONG(bn_ptr, work_blk_id); /* since dest_blk_id replaces work_blk_id */
		BLK_SEG(bs_ptr, bn_ptr, sizeof(block_id));
		BLK_SEG(bs_ptr, saved_blk + piece_len1, dest_parent_size - piece_len1);
		if (!BLK_FINI(bs_ptr, bs1))
		{
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_blkmod;
		}
		if (dest_blk_level || level)
		{
			save_t_err = t_err;
			t_err = ERR_GVKILLFAIL;
		}
		t_write(dest_blk_id, (unsigned char *)bs1, 0, 0, dest_blk_ptr, level, TRUE, TRUE);
		if (dest_blk_level || level)
			t_err = save_t_err;
	}
	else /* free block or, when working block does not move vertically (swap with parent/child) */
	{
		BLK_INIT(bs_ptr, bs1);
		BLK_ADDR(saved_blk, work_blk_size, unsigned char);
		memcpy(saved_blk, work_blk_ptr, work_blk_size);
		BLK_SEG(bs_ptr, saved_blk + sizeof(blk_hdr), work_blk_size - sizeof(blk_hdr));
		if (!BLK_FINI(bs_ptr, bs1))
		{
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_blkmod;
		}
		if (blk_was_free || dest_blk_level || level)
		{
			save_t_err = t_err;
			t_err = ERR_GVKILLFAIL;
		}
		free_blk_cw_depth = cw_set_depth;
		if (BLK_FREE == x_blk_lmap) /* Destinition block was free and never used */
			t_write(dest_blk_id, (unsigned char *)bs1, 0, 0, NULL, level, TRUE, TRUE);
		else	/* Destinition is a recycled block, so we need before image */
			t_write(dest_blk_id, (unsigned char *)bs1, 0, 0, dest_blk_ptr, level, TRUE, TRUE);
		if (blk_was_free || dest_blk_level || level)
			t_err = save_t_err;
	}

	if (!blk_was_free)
	{
		/*
		3: Parent of destinition block (may be parent of working block too)
		*/
		if (gv_target->hist.h[level+1].blk_num == dest_hist_ptr->h[dest_blk_level+1].blk_num)
		/* dest parent == work_blk parent */
		{
			BLK_INIT(bs_ptr, bs1);
			/* Interchange pointer to dest_blk_id and work_blk_id */
			if (level != dest_blk_level ||
				gv_target->hist.h[level+1].curr_rec.offset == dest_hist_ptr->h[level+1].curr_rec.offset)
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			if (gv_target->hist.h[level+1].curr_rec.offset < dest_hist_ptr->h[level+1].curr_rec.offset)
			{
				first_offset = gv_target->hist.h[level+1].curr_rec.offset;
				second_offset = dest_hist_ptr->h[level+1].curr_rec.offset;
			}
			else
			{
				first_offset = dest_hist_ptr->h[level+1].curr_rec.offset;
				second_offset = gv_target->hist.h[level+1].curr_rec.offset;
			}
			GET_RSIZ(rec_size1, dest_parent_ptr + first_offset);
			GET_RSIZ(rec_size2, dest_parent_ptr + second_offset);
			if (dest_parent_size < first_offset + rec_size1 ||
				dest_parent_size < second_offset + rec_size2 ||
				BSTAR_REC_SIZE >= rec_size1 || BSTAR_REC_SIZE > rec_size2)
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			piece_len1 =  first_offset + rec_size1 - sizeof(block_id);
			piece_len2 =  second_offset + rec_size2 - sizeof(block_id);
			GET_LONG(child1, dest_parent_ptr + piece_len1);
			GET_LONG(child2, dest_parent_ptr + piece_len2);
			BLK_SEG(bs_ptr, dest_parent_ptr + sizeof(blk_hdr), piece_len1 - sizeof(blk_hdr));
			BLK_ADDR(bn_ptr, sizeof(block_id), unsigned char);
               		PUT_LONG(bn_ptr, child2);
			BLK_SEG(bs_ptr, bn_ptr, sizeof(block_id));
			BLK_SEG(bs_ptr, dest_parent_ptr + first_offset + rec_size1,
				second_offset + rec_size2 - sizeof(block_id) - first_offset - rec_size1);
			BLK_ADDR(bn_ptr, sizeof(block_id), unsigned char);
               		PUT_LONG(bn_ptr, child1);
			BLK_SEG(bs_ptr, bn_ptr, sizeof(block_id));
			BLK_SEG(bs_ptr, dest_parent_ptr + second_offset + rec_size2,
				dest_parent_size - second_offset - rec_size2);
			if (!BLK_FINI(bs_ptr,bs1))
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			if (dest_blk_level || level)
			{
				save_t_err = t_err;
				t_err = ERR_GVKILLFAIL;
			}
			t_write(dest_hist_ptr->h[level+1].blk_num, (unsigned char *)bs1, 0, 0,
				dest_parent_ptr, level+1, FALSE, TRUE);
			if (dest_blk_level || level)
				t_err = save_t_err;
		}
		else if (work_blk_id != dest_hist_ptr->h[dest_blk_level+1].blk_num)
		{
			BLK_INIT(bs_ptr, bs1);
			/* Destinition block moved in the position of working block.
			   So destinition block's parent's pointer should be changed to work_blk_id */
			GET_RSIZ(rec_size1, dest_parent_ptr + dest_hist_ptr->h[dest_blk_level+1].curr_rec.offset);
			if (dest_parent_size < rec_size1 +  dest_hist_ptr->h[dest_blk_level+1].curr_rec.offset ||
				BSTAR_REC_SIZE > rec_size1)
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			BLK_SEG (bs_ptr, dest_parent_ptr + sizeof(blk_hdr),
			 dest_hist_ptr->h[dest_blk_level+1].curr_rec.offset + rec_size1 - sizeof(blk_hdr) - sizeof(block_id));
			BLK_ADDR(bn_ptr, sizeof(block_id), unsigned char);
			PUT_LONG(bn_ptr, work_blk_id);
			BLK_SEG(bs_ptr, bn_ptr, sizeof(block_id));
			BLK_SEG(bs_ptr, dest_parent_ptr + dest_hist_ptr->h[dest_blk_level+1].curr_rec.offset + rec_size1,
				dest_parent_size - dest_hist_ptr->h[dest_blk_level+1].curr_rec.offset  - rec_size1);
			if (!BLK_FINI(bs_ptr,bs1))
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			if (dest_blk_level || level)
			{
				save_t_err = t_err;
				t_err = ERR_GVKILLFAIL;
			}
			t_write(dest_hist_ptr->h[dest_blk_level+1].blk_num, (unsigned char *)bs1, 0, 0,
				dest_parent_ptr, dest_blk_level+1, FALSE, TRUE);
			if (dest_blk_level || level)
				t_err = save_t_err;
		}
	}

	/*
	4: Parent of working block,
		 if different than destinion's parent or, destinition was a free block
	*/
	if (blk_was_free || gv_target->hist.h[level+1].blk_num != dest_hist_ptr->h[dest_blk_level+1].blk_num)
	{
		/* Parent block of working blk should correctly point
		the working block. Working block went to dest_blk_id  */
		GET_RSIZ(rec_size1, (work_parent_ptr + gv_target->hist.h[level+1].curr_rec.offset));
		if (work_parent_size < rec_size1 +  gv_target->hist.h[level+1].curr_rec.offset ||
			BSTAR_REC_SIZE > rec_size1)
		{
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_blkmod;
		}
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, work_parent_ptr + sizeof(blk_hdr),
			gv_target->hist.h[level+1].curr_rec.offset + rec_size1 - sizeof(blk_hdr) - sizeof(block_id));
		BLK_ADDR(bn_ptr, sizeof(block_id), unsigned char);
		PUT_LONG(bn_ptr, dest_blk_id);
		BLK_SEG(bs_ptr, bn_ptr, sizeof(block_id));
		BLK_SEG(bs_ptr, work_parent_ptr + gv_target->hist.h[level+1].curr_rec.offset + rec_size1,
			work_parent_size - gv_target->hist.h[level+1].curr_rec.offset - rec_size1);
		if (!BLK_FINI(bs_ptr, bs1))
		{
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_blkmod;
		}
		if (blk_was_free || dest_blk_level || level)
		{
			save_t_err = t_err;
			t_err = ERR_GVKILLFAIL;
		}
		t_write(gv_target->hist.h[level+1].blk_num, (unsigned char *)bs1, 0, 0,
			work_parent_ptr, level+1, FALSE, TRUE);
		if (blk_was_free || dest_blk_level || level)
			t_err = save_t_err;
	}
	/* else already taken care of, when dest_blk_id moved */

	if (blk_was_free)
	{
		/* A free/recycled block will become busy block.
		So the local bitmap must be updated.
		Local bit map block will be added in the list of update arrray for concurrency check and
		also the cw_set element will be created to mark the free/recycled block as free.
		kill_set_ptr will save the block which will become free */
                child1 = ROUND_DOWN(dest_blk_id, BLKS_PER_LMAP); /* bit map block */
		PUT_LONG(update_array_ptr, dest_blk_id);
                mu_write_map(child1, bmp_buff, (uchar_ptr_t)update_array_ptr, ctn);
                update_array_ptr += sizeof(block_id);
                child1 = 0;
                PUT_LONG(update_array_ptr, child1);
                update_array_ptr += sizeof(block_id);
		cw_set[cw_map_depth - 1].reference_cnt = 1; /* ??? */
		cw_set[cw_map_depth - 1].cycle = map_cycle;
		cw_set[cw_map_depth - 1].cr = map_cr;
		/* same is done in t_end() for t_create mode */
		cw_set[free_blk_cw_depth].mode = gds_t_acquired;
		cw_set[free_blk_cw_depth].cycle = dest_cycle;
		cw_set[free_blk_cw_depth].cr = dest_cr;
		/* working block will be removed */
                kill_set_ptr->blk[kill_set_ptr->used].flag = 0;
                kill_set_ptr->blk[kill_set_ptr->used].level = 0;
                kill_set_ptr->blk[kill_set_ptr->used++].block = work_blk_id;
        }
	*pdest_blk_id = dest_blk_id;
	return cdb_sc_normal;
}

/***************************************************************
Checks if a key is present in exclude global lists.
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
		if (gl_ptr->name.str.len == key_len && 0 == memcmp(gl_ptr->name.str.addr, curr_key_ptr, gl_ptr->name.str.len))
			return TRUE;
	}
	return FALSE;
}
