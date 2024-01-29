/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/***********************************************************************************
mu_split.c:
	Split a block on the boundary of fill_factor.
	Split ancestors's if necessary. Ancestor's split will also honor fill_factor
 ***********************************************************************************/

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
#include "jnl.h"
#include "copy.h"
#include "muextr.h"
#include "mu_reorg.h"

/* Include prototypes */
#include "t_qread.h"
#include "t_write.h"
#include "t_write_root.h"
#include "t_create.h"
#include "mupip_reorg.h"
#include "mu_upgrade_bmm.h"

GBLREF	char			*update_array, *update_array_ptr;
GBLREF	cw_set_element		cw_set[];
GBLREF	enum db_ver		upgrade_block_split_format;	/* UPGRADE/REORG -UPGRADE switch between old and new block format */
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey, *gv_currkey_next_reorg;
GBLREF	gv_namehead		*gv_target;
GBLREF	sgmnt_data_ptr_t	cs_data;
static	gtm_int8 const		zeroes_64 = 0;
static	int4 const		zeroes_32 = 0;
GBLREF	uint4			update_array_size;			/* for the BLK_* macros */
GBLREF	uint4			mu_upgrade_in_prog;			/* non-zero while UPGRADE/REORD -UPGRADE in progress */
GBLREF	unsigned char		cw_set_depth, t_tries;

static inline enum cdb_sc locate_block_split_point(srch_blk_status *blk_stat, int level, int cur_blk_size, int max_fill,
		int *last_rec_size, unsigned char *last_key, int *last_keysz, int *top_off);

/* Clear the block format setting before returning with the desired code */
#define CLEAR_BLKFMT_AND_RETURN(CDB)		\
MBSTART {					\
	upgrade_block_split_format = 0;		\
	return CDB;				\
} MBEND

/***********************************************************************************************
	Input Parameters:
		cur_level: Working block's level
		d_max_fill: Database fill factor
		i_max_fill: Index fill factor
	Output Parameters:
		blks_created: how many new blocks are created
		lvls_increased : How much level is increased
	Input/Output Parameters:
		gv_target: History of working block
	Here it is assumed that i_max_fill or, d_max_fill is strictly less than block size.
	Returns:
		cdb_sc_normal: if successful
		cdb_sc status otherwise
 ************************************************************************************************/
enum cdb_sc mu_split(int cur_level, int i_max_fill, int d_max_fill, int *blks_created, int *lvls_increased, int *max_rightblk_lvl)
{
	blk_hdr_ptr_t	blk_hdr_ptr;
	blk_segment	*bs_ptr1, *bs_ptr2;
	block_id	allocation_clue;
	block_index	left_index, right_index;
	block_offset	ins_off, ins_off2;
	boolean_t	create_root = FALSE, first_copy, insert_in_left = TRUE, long_blk_id, new_rtblk_star_only, split_required;
	cw_set_element	*cse;
	enum cdb_sc	status;
	int		blk_id_sz, blk_seg_cnt, blk_size, bstar_rec_sz, delta, new_bsiz1, new_bsiz2, level, max_fill, max_fill_sav,
			new_ances_currkeycmpc = 0, new_ances_currkeylen = 0, new_ances_currkeysz, new_blk1_last_keysz,
			newblk2_first_keylen, newblk2_first_keysz, new_ins_keycmpc, new_ins_keylen, new_ins_keysz,
			new_leftblk_top_off, next_gv_currkeysz, old_ances_currkeycmpc, old_ances_currkeylen, old_blk1_last_rec_size,
			old_blk1_sz, old_right_piece_len, rec_size, reserve_bytes, save_blk_piece_len, tkeycmpc, tkeylen, tmp_cmpc,
			new_blk1_sz, new_blk2_sz;
	rec_hdr_ptr_t	new_rec_hdr1a, new_rec_hdr1b, new_rec_hdr2 = NULL, root_hdr, star_rec_hdr, star_rec_hdr32, star_rec_hdr64;
	sm_uc_ptr_t	ances_currkey = NULL, bn_ptr1, bn_ptr2, key_base, rec_base, rPtr1, rPtr2, new_blk1_top = NULL,
			newblk2_first_key, new_blk2_frec_base, new_blk2_rem, new_blk2_top, new_ins_key, next_gv_currkey,
			old_blk_after_currec, old_blk1_base, save_blk_piece;
	srch_blk_status	*old_blk1_hist_ptr;
	unsigned char	curr_prev_key[MAX_KEY_SZ + 3], new_blk1_last_key[MAX_KEY_SZ + 3], *zeroes;
	unsigned short	temp_ushort;
	bool		split_ins_and_curr;
	sm_uc_ptr_t 	prev_blk2_frec_base, curr_blk2_frec_base;
	int 		prev_blk1_top_off, prev_blk1_last_keysz, prev_blk1_last_rec_size, prev_blk1_last_cmpc, prev_new_blk1_size,
			prev_new_blk2_size, curr_blk1_top_off, curr_blk1_last_cmpc, curr_blk1_last_keysz, curr_blk1_last_rec_size,
			exp_level, curr_new_blk1_size, curr_new_blk2_size;
	unsigned char 	prev_blk1_last_key[MAX_KEY_SZ + 3], curr_blk1_last_key[MAX_KEY_SZ + 3];
	bool		prev_splits_ins_and_curr, curr_splits_ins_and_curr, prev_ins_starts_rt, curr_ins_starts_rt;

	blk_hdr_ptr = (blk_hdr_ptr_t)(gv_target->hist.h[cur_level].buffaddr);
	reserve_bytes = cs_data->i_reserved_bytes; /* for now, simple if not upgrade/downgrade */
	if (mu_upgrade_in_prog)	/* Future enhancement should make full use of i_max_fill and d_max_fill */
		reserve_bytes = 0;					/* REORG -UPGRADE is top down, so zero the reserve bytes */
	blk_size = cs_data->blk_size;
	CHECK_AND_RESET_UPDATE_ARRAY;					/* reset update_array_ptr to update_array */
	long_blk_id = IS_64_BLK_ID(blk_hdr_ptr);
	bstar_rec_sz = bstar_rec_size(long_blk_id);
	blk_id_sz = SIZEOF_BLK_ID(long_blk_id);
	zeroes = (long_blk_id ? (unsigned char*)(&zeroes_64) : (unsigned char*)(&zeroes_32));
	upgrade_block_split_format = blk_hdr_ptr->bver;			/* Retain block format */
	assert((FALSE == cs_data->fully_upgraded) || (upgrade_block_split_format == cs_data->desired_db_format));
	BLK_ADDR(star_rec_hdr32, SIZEOF(rec_hdr), rec_hdr);		/* Space for 32bit block number star-key */
	star_rec_hdr32->rsiz = bstar_rec_size(FALSE);
	SET_CMPC(star_rec_hdr32, 0);
	BLK_ADDR(star_rec_hdr64, SIZEOF(rec_hdr), rec_hdr);		/* Space for 64bit block number star-key */
	star_rec_hdr64->rsiz = bstar_rec_size(TRUE);
	SET_CMPC(star_rec_hdr64, 0);
	star_rec_hdr = long_blk_id ? star_rec_hdr64 : star_rec_hdr32;
	level = cur_level;
	max_fill_sav = max_fill = (0 == level) ? d_max_fill : i_max_fill;
	create_root = ((level == gv_target->hist.depth) && (0 < level));
	/*  -------------------
	 *  Split working block.
	 *  -------------------
	 *  new_blk1_last_key = last key of the new working block after split
	 *  new_blk1_last_keysz = size of new_blk1_last_key
	 *  old_blk1_last_rec_size = last record size of the new working block after split (for old block)
	 *  new_blk2_frec_base = base of first record of right block created after split
	 *  newblk2_first_key = first key of new block created after split
	 *  newblk2_first_keysz = size of newblk2_first_key
	 *  new_blk2_rem = pointer to new block to be created after split exclude 1st record header + key
	 */
	old_blk1_hist_ptr = &gv_target->hist.h[level];
	old_blk1_base = (sm_uc_ptr_t)blk_hdr_ptr;
	old_blk1_sz = blk_hdr_ptr->bsiz;
	new_blk2_top = old_blk1_base + old_blk1_sz;
	if (cdb_sc_normal != (status = locate_block_split_point(old_blk1_hist_ptr, level, old_blk1_sz,		/* WARNING assign */
		max_fill, &old_blk1_last_rec_size, new_blk1_last_key, &new_blk1_last_keysz, &new_leftblk_top_off)))
	{
		assert(t_tries < CDB_STAGNATE);
		NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
		CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
	}
	if (new_leftblk_top_off + bstar_rec_sz >= old_blk1_sz)
	{	/* Avoid split to create a small right sibling. Note this should not happen often when tolerance is high */
		if (!mu_upgrade_in_prog)
			CLEAR_BLKFMT_AND_RETURN(cdb_sc_oprnotneeded);
		else
		{	/* MUPIP UPGRADE requires a split; Assume concurrency issue */
			assert(t_tries < CDB_STAGNATE);
			assert(MUPIP_REORG_UPGRADE_IN_PROGRESS == mu_upgrade_in_prog);
			NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
			CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
		}
	}
	old_right_piece_len = old_blk1_sz - new_leftblk_top_off;
	new_blk2_frec_base = old_blk1_base + new_leftblk_top_off;
	BLK_ADDR(newblk2_first_key, MAX_KEY_SZ + 1, unsigned char);	/* Space for the first key */
	READ_RECORD(status, &rec_size, &tkeycmpc, &newblk2_first_keylen, newblk2_first_key,
			level, old_blk1_hist_ptr, new_blk2_frec_base);
	if (cdb_sc_normal != status) /* restart for cdb_sc_starrecord too, because we eliminated the possibility already */
	{
		assert(t_tries < CDB_STAGNATE);
		NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
		CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
	}
	memcpy(newblk2_first_key, &new_blk1_last_key[0], tkeycmpc); /* copy the compressed key piece */
	new_blk2_rem = new_blk2_frec_base + SIZEOF(rec_hdr) + newblk2_first_keylen;
	newblk2_first_keysz = newblk2_first_keylen + tkeycmpc;
	if (new_blk2_top < new_blk2_rem)
	{	/* The remainder cannot be greater than the actual block end */
		assert(t_tries < CDB_STAGNATE);
		NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
		CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
	}
	/* gv_currkey_next_reorg will be saved for next iteration in mu_reorg */
	next_gv_currkey = newblk2_first_key;
	next_gv_currkeysz = newblk2_first_keysz;

	BLK_ADDR(new_rec_hdr1b, SIZEOF(rec_hdr), rec_hdr);
	new_rec_hdr1b->rsiz = rec_size + tkeycmpc;
	SET_CMPC(new_rec_hdr1b, 0);
	/* Note this must be done BEFORE modifying working block as building this buffer relies on the working block to be pinned,
	 * which is possible only if this cw-set-element is created ahead of that for the working block (since order in which blocks
	 * are built is the order in which cses are created); we already know that this will not be *-rec only.
	 */
	BLK_INIT(bs_ptr2, bs_ptr1);
	BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1b, SIZEOF(rec_hdr));
	BLK_SEG(bs_ptr2, newblk2_first_key, newblk2_first_keysz);
	BLK_SEG(bs_ptr2, new_blk2_rem, new_blk2_top - new_blk2_rem);
	if (!BLK_FINI(bs_ptr2, bs_ptr1))
	{
		assert(t_tries < CDB_STAGNATE);
		NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
		CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
	}
	allocation_clue = ALLOCATION_CLUE(cs_data->trans_hist.total_blks);
	right_index = t_create(allocation_clue++, bs_ptr1, 0, 0, level);
	(*blks_created)++;
	/* Modify working block removing split piece */
	BLK_INIT(bs_ptr2, bs_ptr1);
	if (0 == level)
	{
		BLK_SEG(bs_ptr2, old_blk1_base + SIZEOF(blk_hdr), new_leftblk_top_off - SIZEOF(blk_hdr));
	} else
	{	/* Allocate the copy of the remaining pointer minus the last record */
		BLK_SEG(bs_ptr2, old_blk1_base + SIZEOF(blk_hdr),
			new_leftblk_top_off - SIZEOF(blk_hdr) - old_blk1_last_rec_size);
		/* Convert the last key pointer record into a *-key */
		BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr));
		BLK_ADDR(bn_ptr1, blk_id_sz, unsigned char);
		memcpy(bn_ptr1, old_blk1_base + new_leftblk_top_off - blk_id_sz, blk_id_sz);
		BLK_SEG(bs_ptr2, bn_ptr1, blk_id_sz);
	}
	if (!BLK_FINI(bs_ptr2, bs_ptr1))
	{
		assert(t_tries < CDB_STAGNATE);
		NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
		CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
	}
	if (create_root)
		left_index = t_create(allocation_clue++, bs_ptr1, 0, 0, level);
	else
	{
		t_write(old_blk1_hist_ptr, bs_ptr1, 0, 0, level, FALSE, TRUE, GDS_WRITE_KILLTN);
		left_index = -1;
	}
	/*
	----------------------------------------------------------------------------
	Modify ancestor block for the split in current level.
	new_ins_key = new key to be inserted in parent because of split in child
	new_ins_key will be inserted after gv_target->hist.h[level].prev_rec and
	                            before gv_target->hist.h[level].curr_rec
	    new_ins_keysz = size of new_ins_key
	    Note: A restriction of the algorithm is to have current key and new_ins_key
		in the same block, either left or, new right block
	----------------------------------------------------------------------------
	*/
	BLK_ADDR(new_ins_key, new_blk1_last_keysz, unsigned char);
	memcpy(new_ins_key, &new_blk1_last_key[0], new_blk1_last_keysz);
	new_ins_keysz = new_blk1_last_keysz;
	max_fill_sav = i_max_fill;
	assert(!mu_upgrade_in_prog || max_fill_sav);
	while (!create_root) 	/* ========== loop through ancestors as necessary ======= */
	{
		if ((create_root = (level == gv_target->hist.depth))) /* WARNING: assigment */
			break;
		level++;
		max_fill = max_fill_sav;
		/* old_blk_after_currec = remainder of current block after; currec ances_currkey = old real value of currkey
		 * in ancestor block
		 */
		blk_hdr_ptr = (blk_hdr_ptr_t)(gv_target->hist.h[level].buffaddr);
		long_blk_id = IS_64_BLK_ID(blk_hdr_ptr);
		bstar_rec_sz = bstar_rec_size(long_blk_id);
		blk_id_sz = SIZEOF_BLK_ID(long_blk_id);
		zeroes = (long_blk_id ? (unsigned char*)(&zeroes_64) : (unsigned char*)(&zeroes_32));
		star_rec_hdr = long_blk_id ? star_rec_hdr64 : star_rec_hdr32;
		upgrade_block_split_format = blk_hdr_ptr->bver;	/* Retain block format */
		assert((FALSE == cs_data->fully_upgraded) || (upgrade_block_split_format == cs_data->desired_db_format));
		old_blk1_hist_ptr = &gv_target->hist.h[level];
		old_blk1_base = (sm_uc_ptr_t)blk_hdr_ptr;
		old_blk1_sz = blk_hdr_ptr->bsiz;
		new_blk2_top = old_blk1_base + old_blk1_sz;
		rec_base = old_blk1_base + gv_target->hist.h[level].curr_rec.offset;
		GET_RSIZ(rec_size, rec_base);
		old_blk_after_currec = rec_base + rec_size;
		old_ances_currkeycmpc = EVAL_CMPC((rec_hdr_ptr_t)rec_base);
		old_ances_currkeylen = rec_size - bstar_rec_sz;
		if (INVALID_RECORD(level, rec_size, old_ances_currkeylen, old_ances_currkeycmpc, long_blk_id))
		{
			assert(t_tries < CDB_STAGNATE);
			NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
			CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
		}
		if (0 == old_ances_currkeylen)
		{
			if (0 != old_ances_currkeycmpc)
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			new_ances_currkeycmpc = new_ances_currkeylen = 0;
		} else
		{
			BLK_ADDR(ances_currkey, MAX_KEY_SZ + 1, unsigned char);
			key_base = rec_base +  SIZEOF(rec_hdr);
		}
		new_ances_currkeysz = old_ances_currkeycmpc + old_ances_currkeylen;
		if (SIZEOF(blk_hdr) != old_blk1_hist_ptr->curr_rec.offset)
		{	/* cur_rec is not first key */
			if (cdb_sc_normal != (status = gvcst_expand_any_key(old_blk1_hist_ptr,	/* WARNING assignment */
				old_blk1_base + old_blk1_hist_ptr->curr_rec.offset,
				&curr_prev_key[0], &rec_size, &tkeylen, &tkeycmpc, NULL, &exp_level)))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			assert((level == exp_level) || (t_tries < CDB_STAGNATE));
			if (old_ances_currkeycmpc)
			{
				assert(ances_currkey);
				memcpy(ances_currkey, &curr_prev_key[0], old_ances_currkeycmpc);
			}
		}
		if (old_ances_currkeylen)
		{
			assert(ances_currkey);
			memcpy(ances_currkey + old_ances_currkeycmpc, key_base, old_ances_currkeylen);
			GET_CMPC(new_ances_currkeycmpc, new_ins_key, ances_currkey);
			new_ances_currkeylen = new_ances_currkeysz - new_ances_currkeycmpc;
		}
		if (SIZEOF(blk_hdr) != old_blk1_hist_ptr->curr_rec.offset)
		{	/* new_ins_key will be inserted after curr_prev_key */
			GET_CMPC(new_ins_keycmpc, curr_prev_key, new_ins_key);
		}
		else
			new_ins_keycmpc = 0;						 /* new_ins_key will be the 1st key */
		new_ins_keylen = new_ins_keysz - new_ins_keycmpc;
		delta = bstar_rec_sz + new_ins_keylen - old_ances_currkeylen + new_ances_currkeylen;
		if ((old_blk1_sz == (SIZEOF(blk_hdr) + bstar_rec_sz)) /* If we've got a bstar-only block */
				&& ((((old_blk1_sz + delta) > max_fill) && !mu_upgrade_in_prog) /* And max_fill says split */
					|| (old_blk1_sz + delta) > (blk_size - reserve_bytes))) /* Or reserved bytes does */
		{
			/* Then we must have a concurrency issue, since max_fill is protected by limits in reorg and reserved bytes
			 * by MAX_RESERVE_B such that even at an extreme there will be room for a full record and star record in
			 * an index block.
			 */
			assert(CDB_STAGNATE > t_tries);
			CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
		}
		if ((old_blk1_sz + delta) > (blk_size - reserve_bytes))
		{
			split_required = TRUE;
			/* Note that now the delta can no longer be taken at face value. There are three options for this split:
			 * 	1) The split point will occur before the ancestor cur_rec such and it and the newly inserted rec
			 * 	will be in the righthand block.
			 * 	2) The split point will occur just before the ancestor curr_rec, and it will be the first record in
			 * 	the righthand block with the inserted record becoming the lasts record in the lefthand block.
			 * 	3) The split point will occur at or after the ancestor cur_rec such that it will be inside the left
			 * 	block along with the newly inserted key.
			 */
			if (level == gv_target->hist.depth)
			{
				create_root = TRUE;
				if ((MAX_BT_DEPTH - 1) <= level)
					CLEAR_BLKFMT_AND_RETURN(cdb_sc_maxlvl);		/* maximum level reached */
			}
			if (old_blk1_sz - bstar_rec_sz < max_fill)
			{
				if (((SIZEOF(blk_hdr) + bstar_rec_sz) == old_blk1_sz))
				{
					if (mu_upgrade_in_prog)
					{
						assert(t_tries < CDB_STAGNATE);
						CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
					}
					/* This should be impossible given reorg input limits */
					assert(FALSE);
					CLEAR_BLKFMT_AND_RETURN(cdb_sc_oprnotneeded);
				}
			}
			/* 2) Find the most appropriate split point.
			 */
			prev_blk2_frec_base = curr_blk2_frec_base = old_blk1_base + SIZEOF(blk_hdr);
			prev_blk1_top_off = curr_blk1_top_off = SIZEOF(blk_hdr);
			prev_blk1_last_cmpc = prev_blk1_last_keysz = prev_blk1_last_rec_size = 0;
			curr_blk1_last_cmpc = curr_blk1_last_keysz = curr_blk1_last_rec_size = 0;
			prev_splits_ins_and_curr = curr_splits_ins_and_curr = false;
			prev_ins_starts_rt = curr_ins_starts_rt = false;
			/* Invariant: at the start of every loop, all curr_* variables reflect the state of things if the
			 * split takes place right after the last record read in by READ_RECORD, or zero on the first loop.
			 * All prev_* variables reflect the state of things if the split takes place right after the
			 * next-to-last record read in by READ_RECORD, or zero on the second loop/uninitialized on the
			 * first.
			 */
			prev_new_blk1_size = curr_new_blk1_size = SIZEOF(blk_hdr);
			prev_new_blk2_size = curr_new_blk2_size = old_blk1_sz + delta;
			if ((curr_blk1_top_off >= old_blk1_sz)
					|| (curr_new_blk1_size > max_fill)
					|| (curr_new_blk1_size > (blk_size - reserve_bytes)))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			/* We're now guaranteed at least one loop, though not that this loop will turn up anything valid */
			for ( ; (curr_blk1_top_off < old_blk1_sz)
					&& (curr_new_blk1_size <= max_fill)
					&& (curr_new_blk1_size <= (blk_size - reserve_bytes)); )
			{
				/* Store the results of the last iteration so we have something to compare against */
				prev_blk2_frec_base = curr_blk2_frec_base;
				prev_blk1_top_off = curr_blk1_top_off;
				prev_blk1_last_keysz = curr_blk1_last_keysz;
				prev_blk1_last_rec_size = curr_blk1_last_rec_size;
				prev_blk1_last_cmpc = curr_blk1_last_cmpc;
				prev_new_blk1_size = curr_new_blk1_size;
				prev_new_blk2_size = curr_new_blk2_size;
				prev_splits_ins_and_curr = curr_splits_ins_and_curr;
				prev_ins_starts_rt = curr_ins_starts_rt;
				if (curr_blk1_last_keysz)
					memcpy(prev_blk1_last_key, curr_blk1_last_key, curr_blk1_last_keysz);
				/* Finish storing the last iteration */
				if (!prev_ins_starts_rt)
				{
					READ_RECORD(status, &curr_blk1_last_rec_size, &curr_blk1_last_cmpc,
							&curr_blk1_last_keysz, curr_blk1_last_key, level,
							old_blk1_hist_ptr, curr_blk2_frec_base);
					/* Store READ_RECORD meta-results */
					/* (buffaddr + blk1_top_off) and blk2_frec_base are the same */
					curr_blk1_top_off += curr_blk1_last_rec_size;
					curr_blk2_frec_base += curr_blk1_last_rec_size;
					curr_blk1_last_keysz += curr_blk1_last_cmpc;
					/* Following assert guaranteed by the check in read_record */
					assert((curr_blk1_last_keysz <= MAX_KEY_SZ)
							|| ((cdb_sc_normal != status) && (cdb_sc_starrecord != status)));
				}
				/* Finish maintaining READ_RECORD meta-results */
				if ((cdb_sc_starrecord == status) && (curr_blk1_top_off == old_blk1_sz))
				{
					status = cdb_sc_normal;
					break;
				}
				if ((cdb_sc_normal != status))
				{
					assert(t_tries < CDB_STAGNATE);
					status = cdb_sc_blkmod;
					break;
				}
				/* Start calculation of new blk1 and blk2 sizes */
				if ((curr_blk1_top_off == old_blk1_hist_ptr->curr_rec.offset) && prev_ins_starts_rt)
				{
					curr_ins_starts_rt = false;
					curr_splits_ins_and_curr = true;
					/* Calculate baseline size start */
					curr_new_blk1_size = curr_blk1_top_off;
					curr_new_blk2_size = SIZEOF(blk_hdr) + (old_blk1_sz - curr_blk1_top_off);
					/* Calculate baseline size end */
					/* Calculate adjustments due to new record insertion start */
					curr_new_blk1_size += bstar_rec_sz;
					/* Calculate adjustments due to new record insertion end */
					/* Calculate contraction of last record of block 1 start */
					/* Special case: when the last record is the ins_key.
					 * Already handled by not adding new_ins_keylen above.
					 */
					/* Calculate contraction of last record of block 1 end */
					/* Calculate expansion of first record of block 2 start */
					/* Special case: when the curr rec is the first record in the right block.
					 * In this situation curr_key will be the first key and will fully expand, so
					 * add in its compression count.
					 */
					curr_new_blk2_size += old_ances_currkeycmpc;
					/* Not using the newcmpc which was calculated on the assumption
					 * of a preceding new_ins_key.
					 */
					/* Calculate expansion of first record of block 2 end */
					/* Will redo this loop without splitting ins and curr thanks to condition around
					 * READ_RECORD.
					 */
				} else if (curr_blk1_top_off <= old_blk1_hist_ptr->curr_rec.offset)
					/* curr_ and new_ins records will go to the right */
				{
					curr_ins_starts_rt = (curr_blk1_top_off == old_blk1_hist_ptr->curr_rec.offset);
					curr_splits_ins_and_curr = false;
					/* Calculate baseline size start */
					curr_new_blk1_size = curr_blk1_top_off;
					curr_new_blk2_size = SIZEOF(blk_hdr) + (old_blk1_sz - curr_blk1_top_off);
					/* Calculate baseline size end */
					/* Calculate adjustments due to new record insertion start */
					curr_new_blk2_size += (new_ins_keylen + bstar_rec_sz
							+ new_ances_currkeylen - old_ances_currkeylen);
					/* Calculate adjustments due to new record insertion end */
					/* Calculate contraction of last record of block 1 start */
					/* Special case: when the last record is the ances_curr_rec.
					 * This cannot happen in this branch since this record is
					 * going to the right.
					 */
					curr_new_blk1_size -= curr_blk1_last_rec_size;
					curr_new_blk1_size += bstar_rec_sz;
					/* Calculate contraction of last record of block 1 end */
					/* Calculate expansion of first record of block 2 start */
					/* Special case: when the curr rec is the first record in the right block.
					 * In this situation new_ins_key will be the first key and will fully expand, so
					 * add in its compression count. Otherwise, add in the compression count of the
					 * curr_blk2_frec_base record.
					 */
					if (curr_blk1_top_off == old_blk1_hist_ptr->curr_rec.offset)
						curr_new_blk2_size += new_ins_keycmpc;
					else
						curr_new_blk2_size += EVAL_CMPC((rec_hdr_ptr_t)curr_blk2_frec_base);
					/* Calculate expansion of first record of block 2 end */
				} else if (curr_blk1_top_off <= old_blk1_sz)
				{	/* curr_ and new_ins records will go to the left. */
					curr_ins_starts_rt = false;
					curr_splits_ins_and_curr = false;
					/* Calculate baseline size start */
					curr_new_blk1_size = curr_blk1_top_off;
					curr_new_blk2_size = SIZEOF(blk_hdr) + (old_blk1_sz - curr_blk1_top_off);
					/* Calculate baseline size end */
					/* Calculate adjustments due to new record insertion start */
					curr_new_blk1_size += (new_ins_keylen + bstar_rec_sz
							+ new_ances_currkeylen - old_ances_currkeylen);
					/* Special case: when the curr rec is the first record in the left block.
					 * This can and must be dealt with at the time of calculating the initial
					 * ances_currkeycmpc, etc. Assert that here without depending on volatile memory */
					assert((SIZEOF(blk_hdr) != old_blk1_hist_ptr->curr_rec.offset) ||
							(!new_ins_keycmpc && (new_ins_keysz == new_ins_keylen)));
					/* Calculate adjustments due to new record insertion end */
					/* Calculate contraction of last record of block 1 start */
					/* Special case: when the last record is the ances_curr_rec.
					 * in this case, we should subtract the new_ances_currkeylen.
					 */
					if (prev_blk1_top_off == old_blk1_hist_ptr->curr_rec.offset)
					{
						curr_new_blk1_size -= new_ances_currkeylen;
						/* No way to assert that reclen - keylen == bstar_rec_size,
						 * but the code depends on that invariant.
						 */
					} else
					{
						curr_new_blk1_size -= curr_blk1_last_rec_size;
						curr_new_blk1_size += bstar_rec_sz;
					}
					/* Calculate contraction of last record of block 1 end */
					/* Calculate expansion of first record of block 2 start */
					/* Special case: when the curr rec is the first record in the right block.
					 * Cannot occur here since we are in a split-to-left situation.
					 */
					curr_new_blk2_size += EVAL_CMPC((rec_hdr_ptr_t)curr_blk2_frec_base);
					/* Calculate expansion of first record of block 2 end */
				} else
				{	/* We've overrun the buffer. Instead of segfaulting on the EVAL_CMPC, stop here */
					assert(t_tries < CDB_STAGNATE);
					status = cdb_sc_blkmod;
					break;
				}
				/* Finish calculation of new blk1 and blk2 sizes */
			} /* Finish READ_RECORD for-loop */
			if (prev_new_blk1_size > max_fill)
			{
				/* If the loop overran its goal somehow, something has gone wrong in the READ_RECORD loop.
				 * Still, this is not known to be possible.
				 */
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			} else if ((prev_new_blk1_size > (blk_size - reserve_bytes))
					|| (prev_new_blk2_size > blk_size)
					|| (prev_blk1_top_off >= old_blk1_sz))
			{
				/* If the loop overran the size of the previous block, or the hard limit of reserved bytes,
				 * then there must have been a concurrency issue in reading from the block.
				 */
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			} else if ((curr_blk1_top_off > old_blk1_sz) || (((blk_hdr_ptr_t)old_blk1_base)->levl != level)
					|| (((blk_hdr_ptr_t)old_blk1_base)->bsiz != old_blk1_sz))
			{
				/* If we think that the lefthand block is larger than the current block, or the level
				 * doesn't match, or the size doesn't match what we read before, then concurrency issue
				 */
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			} else if ((SIZEOF(blk_hdr) >= prev_new_blk1_size)  || (SIZEOF(blk_hdr) >= prev_new_blk2_size))
			{
				/* If either of the calculated sizes contains only a block header or less, then concurrency issue.
				 */
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			} else if ((cdb_sc_normal != status)  || (0 == prev_blk1_last_keysz))
			{
				/* if we get a bad status from READ_RECORD or are trying to split after the star record,
				 * concurrency issue.
				 */
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			split_ins_and_curr = prev_splits_ins_and_curr;
			if ((SIZEOF(blk_hdr) + bstar_rec_sz) == prev_new_blk2_size)
				new_rtblk_star_only = TRUE;
			else
				new_rtblk_star_only = FALSE;
			if (prev_blk1_last_keysz)
				memcpy(new_blk1_last_key, prev_blk1_last_key, prev_blk1_last_keysz);
			if (prev_blk1_top_off == old_blk1_hist_ptr->curr_rec.offset)
			{
				if (split_ins_and_curr)
				{
					/* Need to preserve this information to re-initialize new_ins_key if needed
					 * at the next split level
					 */
					memcpy(new_blk1_last_key, new_ins_key, new_ins_keysz);
					prev_blk1_last_keysz = new_ins_keysz;
					/* Newly inserted key will be the final star key, so has no length */
					new_ins_keylen = new_ins_keysz = new_ins_keycmpc = 0;
					/* If it had sz, will already have been expanded. If not, it's the star key
					 * and will be the star key also in the new block.
					 */
					new_ances_currkeylen = new_ances_currkeysz;
					new_ances_currkeycmpc = 0;
				} else
				{
					/* Insertion key will start us off, and it is already expanded so just set the
					 * length and cmpc to indicate that it won't be compressed.
					 */
					new_ins_keylen = new_ins_keysz;
					new_ins_keycmpc = 0;

				}
			} else
			{	/* process 1st record of new right block */
				assert(!split_ins_and_curr);
				BLK_ADDR(newblk2_first_key, MAX_KEY_SZ + 1, unsigned char);
				READ_RECORD(status, &rec_size, &tkeycmpc, &newblk2_first_keylen, newblk2_first_key,
						level, old_blk1_hist_ptr, prev_blk2_frec_base);
				if (cdb_sc_normal == status)
				{
					memcpy(newblk2_first_key, new_blk1_last_key, tkeycmpc); /* compressed piece */
					new_blk2_rem = prev_blk2_frec_base + SIZEOF(rec_hdr) + newblk2_first_keylen;
					newblk2_first_keysz = newblk2_first_keylen + tkeycmpc;
					BLK_ADDR(new_rec_hdr2, SIZEOF(rec_hdr), rec_hdr);
					new_rec_hdr2->rsiz = newblk2_first_keysz + bstar_rec_sz;
					SET_CMPC(new_rec_hdr2, 0);
				} else if ((cdb_sc_starrecord != status) || !new_rtblk_star_only)
				{
					assert(t_tries < CDB_STAGNATE);
					NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
					CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
				}
			}
			if ((prev_blk1_top_off <= old_blk1_hist_ptr->curr_rec.offset) && !split_ins_and_curr)
			{
				insert_in_left = FALSE;
			} else
			{
				insert_in_left = TRUE;
			}
			new_blk2_frec_base = new_blk1_top = old_blk1_base + prev_blk1_top_off;
			old_blk1_last_rec_size = prev_blk1_last_rec_size;
			new_blk1_last_keysz = prev_blk1_last_keysz; /* Only used if we go another level up */
			new_leftblk_top_off = prev_blk1_top_off;
			new_blk1_sz = prev_new_blk1_size;
			new_blk2_sz = prev_new_blk2_size;
			assert(new_blk2_frec_base == prev_blk2_frec_base);
			assert(bstar_rec_sz != prev_blk1_last_rec_size);
		}	/* end if split required */
		else
		{
			new_blk1_sz = delta + old_blk1_sz;
			/* Used only in sanity checks at end; this asserts that we should only ever build one block in this case */
			new_blk2_sz = -1;
			split_required = FALSE;
			new_rtblk_star_only = FALSE;
		}
		/* Construct the new index block(s) */
		BLK_ADDR(new_rec_hdr1a, SIZEOF(rec_hdr), rec_hdr);
		new_rec_hdr1a->rsiz = bstar_rec_sz + new_ins_keylen;
		SET_CMPC(new_rec_hdr1a, new_ins_keycmpc);
		BLK_ADDR(new_rec_hdr1b, SIZEOF(rec_hdr), rec_hdr);
		new_rec_hdr1b->rsiz = bstar_rec_sz + new_ances_currkeylen;
		SET_CMPC(new_rec_hdr1b, new_ances_currkeycmpc);
		BLK_ADDR(bn_ptr1, blk_id_sz, unsigned char);
		/* child block pointer of ances_currkey */
		memcpy(bn_ptr1, old_blk1_base + old_blk1_hist_ptr->curr_rec.offset +
				SIZEOF(rec_hdr) + old_ances_currkeylen, blk_id_sz);
		if (!split_required)
		{	/* LEFT part of old BLOCK */
			BLK_INIT(bs_ptr2, bs_ptr1);
			if (SIZEOF(blk_hdr) < old_blk1_hist_ptr->curr_rec.offset)
			{
				BLK_SEG(bs_ptr2, old_blk1_base + SIZEOF(blk_hdr),
						old_blk1_hist_ptr->curr_rec.offset - SIZEOF(blk_hdr));
				first_copy = FALSE;
			} else
				first_copy = TRUE;
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1a, SIZEOF(rec_hdr));
			BLK_SEG(bs_ptr2, new_ins_key + new_ins_keycmpc, new_ins_keylen);
			BLK_SEG(bs_ptr2, bn_ptr1, blk_id_sz);
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1b, SIZEOF(rec_hdr));
			if (0 < new_ances_currkeylen)
			{
				assert(ances_currkey);
				BLK_SEG(bs_ptr2, ances_currkey + new_ances_currkeycmpc, new_ances_currkeylen);
			}
			ins_off = blk_seg_cnt;
			BLK_SEG(bs_ptr2, zeroes, blk_id_sz);
			if (0 < (old_blk1_base + old_blk1_sz - old_blk_after_currec))
				BLK_SEG(bs_ptr2, old_blk_after_currec, old_blk1_base + old_blk1_sz - old_blk_after_currec);
			if (new_blk1_sz != blk_seg_cnt)
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			t_write(&gv_target->hist.h[level], bs_ptr1, ins_off, right_index,
				level, first_copy, FALSE, GDS_WRITE_KILLTN);
			break;
		}
		/* if SPLIT REQUIRED */
		if (split_ins_and_curr)
		{
			/* If we're splitting the insertion key from the current key, there are two options for max_rightblk_lvl.
			 * Take two extremes: In the first, the secondary split sends both to the left and the tertiary+ splits
			 * split ins and curr. In the second, the secondary split sends both to the right and the tertiary+ splits
			 * split ins and curr.
			 * In the first case, we do not need to process any blocks; the right-sibling at the lowest level
			 * (gv_currkey_next_reorg at the end of this fundtion) has the same lineage as the gv_currkey/target,
			 * and that lineage is an entirely lefthand one (and therefore not in need of further traversal).
			 * In the second case, we need to process all the split-to-right blocks: these are new blocks, so
			 * they've been appended to the end and are not ideally swapped, the split logic does not guarantee
			 * split-to-right blocks are well-sized (just that split-to-left ones are), and in the more common case
			 * where these blocks are undersized they could benefit from coalescing. But we will never ordinarily
			 * traverse them, because they are all ancestors we share with our right-sibling
			 * (see the mu_reorg no-split no-coalesce branch for this mechanism).
			 * Therefore this algorithm increases max_rightblk_lvl whenever:
			 * 	1) There is a new righthand block created which contains both the insertion and current ancestor
			 * 	keys, at any level, since thes means that there is a novel block which is an ancestor of both
			 * 	the gv_currkey and the gv_currkey_next_reorg (and potentially many more keys besides), and it
			 * 	needs processing.
			 * 	2) There is a new righthand block that contains the current key, and 1) or 2) was true for
			 * 	the previous split in the current sequence, or this is a secondary split.
			 */
			if (*max_rightblk_lvl == (level - 1))
				*max_rightblk_lvl = level;
			assert(insert_in_left);
			/* LEFT BLOCK */
			BLK_INIT(bs_ptr2, bs_ptr1);
			if (SIZEOF(blk_hdr) < old_blk1_hist_ptr->curr_rec.offset)
			{
				BLK_SEG(bs_ptr2, old_blk1_base + SIZEOF(blk_hdr),
					old_blk1_hist_ptr->curr_rec.offset - SIZEOF(blk_hdr));
				first_copy = FALSE;
			} else
				first_copy = TRUE;
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr));
			BLK_SEG(bs_ptr2, bn_ptr1, blk_id_sz);
			if (new_blk1_sz != blk_seg_cnt)
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}

			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			if (create_root)
				left_index = t_create(allocation_clue++, bs_ptr1, 0, 0, level);
			else
				t_write(&gv_target->hist.h[level], bs_ptr1, 0, 0,
						level, first_copy, TRUE, GDS_WRITE_KILLTN);
			/* RIGHT BLOCK */
			BLK_INIT(bs_ptr2, bs_ptr1);
			if (new_rtblk_star_only)
			{
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr));
				ins_off = blk_seg_cnt;
				BLK_SEG(bs_ptr2, zeroes, blk_id_sz);
			} else
			{
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1b, SIZEOF(rec_hdr));
				assert(new_ances_currkeysz && ances_currkey);
				assert(new_ances_currkeysz == new_ances_currkeylen);
				BLK_SEG(bs_ptr2, ances_currkey, new_ances_currkeysz);
				ins_off = blk_seg_cnt;
				BLK_SEG(bs_ptr2, zeroes, blk_id_sz);
				save_blk_piece_len = (int)(new_blk2_top - old_blk_after_currec);
				assert(save_blk_piece_len);
				BLK_ADDR(save_blk_piece, save_blk_piece_len, unsigned char);
				memcpy(save_blk_piece, old_blk_after_currec, save_blk_piece_len);
				BLK_SEG(bs_ptr2, save_blk_piece, save_blk_piece_len);
			}
			if (new_blk2_sz != blk_seg_cnt)
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			right_index = t_create(allocation_clue++, bs_ptr1, ins_off, right_index, level);
		} else if (insert_in_left) /* new_ins_key will go to left block */
		{	/* LEFT BLOCK */
			assert(new_blk1_top);
			BLK_INIT(bs_ptr2, bs_ptr1);
			if (SIZEOF(blk_hdr) < old_blk1_hist_ptr->curr_rec.offset)
			{
				BLK_SEG(bs_ptr2, old_blk1_base + SIZEOF(blk_hdr),
					old_blk1_hist_ptr->curr_rec.offset - SIZEOF(blk_hdr));
				first_copy = FALSE;
			} else
				first_copy = TRUE;
			assert(new_ins_keylen);
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1a, SIZEOF(rec_hdr));
			BLK_SEG(bs_ptr2, new_ins_key + new_ins_keycmpc, new_ins_keylen);
			BLK_SEG(bs_ptr2, bn_ptr1, blk_id_sz);
			if (old_blk_after_currec < new_blk1_top) /* curr_rec is not the last record of new left block */
			{
				assert(ances_currkey);
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1b, SIZEOF(rec_hdr));
				BLK_SEG(bs_ptr2, ances_currkey + new_ances_currkeycmpc, new_ances_currkeylen);
				ins_off = blk_seg_cnt;
				BLK_SEG(bs_ptr2, zeroes, blk_id_sz);
				save_blk_piece_len = (int)(new_blk1_top - old_blk1_last_rec_size - old_blk_after_currec);
				if (0 < save_blk_piece_len)
				{
					if ((old_blk_after_currec + save_blk_piece_len) >= new_blk2_top)
					{
						assert(t_tries < CDB_STAGNATE);
						NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
						CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
					}
					BLK_ADDR(save_blk_piece, save_blk_piece_len, unsigned char);
					memcpy(save_blk_piece, old_blk_after_currec, save_blk_piece_len);
					BLK_SEG(bs_ptr2, save_blk_piece, save_blk_piece_len);
				}
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr));
				BLK_ADDR(bn_ptr2, blk_id_sz, unsigned char);		/* Block pointer for 1st new block rec */
				memcpy(bn_ptr2, new_blk1_top - blk_id_sz, blk_id_sz);
				BLK_SEG(bs_ptr2, bn_ptr2, blk_id_sz);
			} else
			{
				assert(old_blk_after_currec == new_blk1_top);
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr));
				ins_off = blk_seg_cnt;
				BLK_SEG(bs_ptr2, zeroes, blk_id_sz);
			}
			if (new_blk1_sz != blk_seg_cnt)
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			if (create_root)
				left_index = t_create(allocation_clue++, bs_ptr1, ins_off, right_index, level);
			else
				t_write(&gv_target->hist.h[level], bs_ptr1, ins_off, right_index,
						level, first_copy, FALSE, GDS_WRITE_KILLTN);
			/* RIGHT BLOCK */
			BLK_INIT(bs_ptr2, bs_ptr1);
			if (new_rtblk_star_only)
			{
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr));
				BLK_ADDR(bn_ptr2, blk_id_sz, unsigned char);
				memcpy(bn_ptr2, new_blk2_top - blk_id_sz, blk_id_sz);
				BLK_SEG(bs_ptr2, bn_ptr2, blk_id_sz);
			} else
			{
				assert(new_rec_hdr2);
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr2, SIZEOF(rec_hdr));
				BLK_SEG(bs_ptr2, newblk2_first_key, newblk2_first_keysz);
				save_blk_piece_len = (int)(new_blk2_top - new_blk2_rem);
				if (0 > save_blk_piece_len)
				{
					assert(t_tries < CDB_STAGNATE);
					NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
					CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
				}
				BLK_ADDR(save_blk_piece, save_blk_piece_len, unsigned char);
				memcpy(save_blk_piece, new_blk2_rem, save_blk_piece_len);
				BLK_SEG(bs_ptr2, save_blk_piece, save_blk_piece_len);
			}
			if (new_blk2_sz != blk_seg_cnt)
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			right_index = t_create(allocation_clue++, bs_ptr1, 0, 0, level);
			(*blks_created)++;
		} else	/* end if insert_in_left */
		{	/* new_ins_key to be inserted in right block */
			/* This means that it is certain that a parent block of our gv_currkey_next_reorg-to-be
			 * will be a newly-created righthand block, yet to be coalesced or swapped.
			 * In this case we can unconditionally set max_rightblk_lvl
			 */
			*max_rightblk_lvl = level;
			/* LEFT BLOCK */
			BLK_INIT(bs_ptr2, bs_ptr1);
			save_blk_piece_len = (int)(new_leftblk_top_off - SIZEOF(blk_hdr) - old_blk1_last_rec_size);
			if ((old_blk1_base + SIZEOF(blk_hdr) + save_blk_piece_len >= new_blk2_top) || (0 > save_blk_piece_len))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			BLK_ADDR(save_blk_piece, save_blk_piece_len, unsigned char);
			memcpy(save_blk_piece, old_blk1_base + SIZEOF(blk_hdr), save_blk_piece_len);
			BLK_SEG(bs_ptr2, save_blk_piece, save_blk_piece_len);
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr));
			BLK_ADDR(bn_ptr2, blk_id_sz, unsigned char);
			memcpy(bn_ptr2, old_blk1_base + new_leftblk_top_off - blk_id_sz, blk_id_sz);
			BLK_SEG(bs_ptr2, bn_ptr2, blk_id_sz);
			if (new_blk1_sz != blk_seg_cnt)
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			if (create_root)
				left_index = t_create(allocation_clue++, bs_ptr1, 0, 0, level);
			else
				t_write(&gv_target->hist.h[level], bs_ptr1, 0, 0,
					level, TRUE, TRUE, GDS_WRITE_KILLTN);
			/* RIGHT BLOCK */
			BLK_INIT(bs_ptr2, bs_ptr1);
			if (new_leftblk_top_off < old_blk1_hist_ptr->curr_rec.offset)
			{	/* anything before curr_rec */
				assert(new_rec_hdr2);
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr2, SIZEOF(rec_hdr));
				BLK_SEG(bs_ptr2, newblk2_first_key, newblk2_first_keysz);
				save_blk_piece_len = (int)(old_blk1_hist_ptr->curr_rec.offset -
					new_leftblk_top_off  - (new_blk2_rem - new_blk2_frec_base));
				if ((((new_blk2_rem + save_blk_piece_len)) >= new_blk2_top) || (0 > save_blk_piece_len))
				{
					assert(t_tries < CDB_STAGNATE);
					NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
					CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
				}
				BLK_ADDR(save_blk_piece, save_blk_piece_len, unsigned char);
				memcpy(save_blk_piece, new_blk2_rem, save_blk_piece_len);
				BLK_SEG(bs_ptr2, save_blk_piece, save_blk_piece_len);
			}
			/* Following else if may not be necessary. But I wanted it to be safe:Layek:10/3/2000 */
			else if (new_leftblk_top_off > old_blk1_hist_ptr->curr_rec.offset)
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			assert(ances_currkey || (0 == new_ances_currkeylen));
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1a, SIZEOF(rec_hdr));
			BLK_SEG(bs_ptr2, new_ins_key + new_ins_keycmpc, new_ins_keylen);
			BLK_SEG(bs_ptr2, bn_ptr1, blk_id_sz);
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1b, SIZEOF(rec_hdr));
			BLK_SEG(bs_ptr2, ances_currkey + new_ances_currkeycmpc, new_ances_currkeylen);
			ins_off = blk_seg_cnt;
			BLK_SEG(bs_ptr2, zeroes, blk_id_sz);
			save_blk_piece_len = (int)(new_blk2_top - old_blk_after_currec);
			if (0 < save_blk_piece_len)
			{
				BLK_ADDR(save_blk_piece, save_blk_piece_len, unsigned char);
				memcpy(save_blk_piece, old_blk_after_currec, save_blk_piece_len);
				BLK_SEG(bs_ptr2, save_blk_piece, save_blk_piece_len);
			}
			if (new_blk2_sz != blk_seg_cnt)
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			right_index = t_create(allocation_clue++, bs_ptr1, ins_off, right_index, level);
			(*blks_created)++;
		}	/* endif new_ins_key inserted in right block */
		assert(new_blk1_last_keysz);
		BLK_ADDR(new_ins_key, new_blk1_last_keysz, unsigned char);
		memcpy(new_ins_key, &new_blk1_last_key[0], new_blk1_last_keysz);
		new_ins_keysz = new_blk1_last_keysz;
		if (create_root)
			break;
	} /* ========== End loop through ancestors as necessary ======= */
	if (create_root && (-1 < left_index))
	{
		BLK_ADDR(root_hdr, SIZEOF(rec_hdr), rec_hdr);
		root_hdr->rsiz = bstar_rec_sz + new_ins_keysz;
		SET_CMPC(root_hdr, 0);
		BLK_INIT(bs_ptr2, bs_ptr1);
		BLK_SEG(bs_ptr2, (sm_uc_ptr_t)root_hdr, SIZEOF(rec_hdr));
		BLK_SEG(bs_ptr2, new_ins_key, new_ins_keysz);
		ins_off = blk_seg_cnt;
		BLK_SEG(bs_ptr2, zeroes, blk_id_sz);
		BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr));
		ins_off2 = blk_seg_cnt;
		BLK_SEG(bs_ptr2, zeroes, blk_id_sz);
		if (!BLK_FINI(bs_ptr2, bs_ptr1))
		{
			assert(t_tries < CDB_STAGNATE);
			NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
			CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
		}
		cse = t_write(&gv_target->hist.h[level], bs_ptr1, ins_off, left_index,
			level + 1, TRUE, FALSE, GDS_WRITE_KILLTN);
		t_write_root(ins_off2, right_index);	/* create a sibling cw-set-element to store ins_off2/right_index */
		(*lvls_increased)++;
	}
	/* gv_currkey_next_reorg for next iteration in mu_reorg */
	memcpy(&gv_currkey_next_reorg->base[0], next_gv_currkey, next_gv_currkeysz);
	gv_currkey_next_reorg->end = next_gv_currkeysz - 1;
	CLEAR_BLKFMT_AND_RETURN(cdb_sc_normal);
}	/* end mu_split() */

/*
-------------------------------------------------------------------------
locate_block_split_point ():
	This will split a block at a point given by fill factor
Input Parameter:
	blk_stat = target block history
	level = level of the block
	cur_blk_size = size of the block
	max_fill = maximum fill allowed for the block (max_fill < cur_blk_size)
Output Parameter:
	last_rec_size = last record size of first piece
	last_key = actual value of last key of the first block
	last_keysz = size of  actual value of last key of the first block
	top_off = offset of left piece's top
Return :
	cdb_sc_blkmod : If block is already modified
	cdb_sc_normal : Otherwise (not necessary block is fine)
	Note:	After split
			*top_off >=  max_fill,
			max_fill <= cur_blk_size
			max_fill > SIZEOF(blk_hdr)
		At least one record will be in left block after split
-------------------------------------------------------------------------
*/
static inline enum cdb_sc locate_block_split_point(srch_blk_status *blk_stat, int level, int cur_blk_size, int max_fill,
		int *last_rec_size, unsigned char *last_key, int *last_keysz, int *top_off)
{
	block_id	blkid;
	int		tkeycmpc;
	enum cdb_sc	status;
	sm_uc_ptr_t 	blk_base, iter, rec_base, rPtr1, rPtr2, rPtr_arry[cs_data->max_rec];
	unsigned short	temp_ushort;

	*last_keysz = 0;
	*top_off = SIZEOF(blk_hdr);
	*last_rec_size = 0;
	blk_base = blk_stat->buffaddr;
	rec_base = blk_base + SIZEOF(blk_hdr);
	for (; *top_off < max_fill; )
	{
		READ_RECORD(status, last_rec_size, &tkeycmpc, last_keysz, last_key,
				level, blk_stat, rec_base);
		*top_off += *last_rec_size;
		*last_keysz += tkeycmpc;
		rec_base += *last_rec_size;
		if ((cdb_sc_starrecord == status) && (*top_off == cur_blk_size))
			break;
		if (cdb_sc_normal != status)
		{
			assert(t_tries < CDB_STAGNATE);
			NONTP_TRACE_HIST_MOD(blk_stat, t_blkmod_mu_split);
			return cdb_sc_blkmod; /* block became invalid */
		}
	}	/* end of "while" loop */
	if ((*top_off > cur_blk_size) || (((blk_hdr_ptr_t)blk_base)->levl != level)
		|| (((blk_hdr_ptr_t)blk_base)->bsiz != cur_blk_size))
	{
		assert(t_tries < CDB_STAGNATE);
		NONTP_TRACE_HIST_MOD(blk_stat, t_blkmod_mu_split);
		return cdb_sc_blkmod; /* block became invalid */
	}
	return cdb_sc_normal;
}
