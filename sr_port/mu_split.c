/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
<<<<<<< HEAD
GBLREF	unsigned char		cw_set_depth;
GBLREF	unsigned int		t_tries;
=======
GBLREF	uint4			mu_upgrade_in_prog;			/* non-zero while UPGRADE/REORD -UPGRADE in progress */
GBLREF	unsigned char		cw_set_depth, t_tries;
>>>>>>> f9ca5ad6 (GT.M V7.1-000)

enum cdb_sc locate_block_split_point(srch_blk_status *blk_stat, int level, int cur_blk_size, int max_fill, int *last_rec_size,
					unsigned char *last_key, int *last_keysz, int *top_off);

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
enum cdb_sc mu_split(int cur_level, int i_max_fill, int d_max_fill, int *blks_created, int *lvls_increased)
{
	blk_hdr_ptr_t	blk_hdr_ptr;
	blk_segment	*bs_ptr1, *bs_ptr2;
	block_id	allocation_clue;
	block_index	left_index, right_index;
	block_offset	ins_off, ins_off2;
	boolean_t	create_root = FALSE, first_copy, insert_in_left, long_blk_id, new_rtblk_star_only, split_required;
	cw_set_element	*cse;
	enum cdb_sc	status;
	int		available_bytes, blk_id_sz, blk_seg_cnt, blk_size, bstar_rec_sz, delta, level, max_fill, max_fill_sav,
			new_ances_currkeycmpc, new_ances_currkeylen, new_ances_currkeysz, new_blk1_last_keysz, newblk2_first_keylen,
			newblk2_first_keysz, new_ins_keycmpc, new_ins_keylen, new_ins_keysz, new_leftblk_top_off,
			next_gv_currkeysz, old_ances_currkeycmpc, old_ances_currkeylen, old_blk1_last_rec_size, old_blk1_sz,
			old_right_piece_len, rec_size, reserve_bytes, save_blk_piece_len, tkeycmpc, tkeylen, tmp_cmpc;
	rec_hdr_ptr_t	new_rec_hdr1a, new_rec_hdr1b, new_rec_hdr2, root_hdr, star_rec_hdr, star_rec_hdr32, star_rec_hdr64;
	sm_uc_ptr_t	ances_currkey, bn_ptr1, bn_ptr2, key_base, rec_base, rPtr1, rPtr2, new_blk1_top, newblk2_first_key,
			new_blk2_frec_base, new_blk2_rem, new_blk2_top, new_ins_key, next_gv_currkey, old_blk_after_currec,
			old_blk1_base, save_blk_piece;
	srch_blk_status	*old_blk1_hist_ptr;
	unsigned char	curr_prev_key[MAX_KEY_SZ + 3], new_blk1_last_key[MAX_KEY_SZ + 3], *zeroes;
	unsigned short	temp_ushort;

	blk_hdr_ptr = (blk_hdr_ptr_t)(gv_target->hist.h[cur_level].buffaddr);
	reserve_bytes = cs_data->reserved_bytes;			/* for now, simple if not upgrade/downgrade */
	if (mu_upgrade_in_prog)	/* Future enhancement should make full use of i_max_fill and d_max_fill */
	{
		assert(i_max_fill == d_max_fill);
		reserve_bytes = cs_data->i_reserved_bytes + i_max_fill;
		available_bytes = cs_data->blk_size - blk_hdr_ptr->bsiz;
		if (available_bytes >= reserve_bytes)
			CLEAR_BLKFMT_AND_RETURN(cdb_sc_oprnotneeded);	/* upgrade does not require a split */
		if ((cs_data->blk_size << 1) < reserve_bytes)
		{
			DEBUG_ONLY(available_bytes += cs_data->i_reserved_bytes);
			reserve_bytes = i_max_fill;
		}
		assert((cs_data->blk_size << 1) > available_bytes);	/* Half block size should always be enough */
	}
	blk_size = cs_data->blk_size;
	CHECK_AND_RESET_UPDATE_ARRAY;					/* reset update_array_ptr to update_array */
	long_blk_id = IS_64_BLK_ID(blk_hdr_ptr);
	bstar_rec_sz = bstar_rec_size(long_blk_id);
	blk_id_sz = SIZEOF_BLK_ID(long_blk_id);
	zeroes = (long_blk_id ? (unsigned char*)(&zeroes_64) : (unsigned char*)(&zeroes_32));
	if (mu_upgrade_in_prog)
		upgrade_block_split_format = blk_hdr_ptr->bver;
	BLK_ADDR(star_rec_hdr32, SIZEOF(rec_hdr), rec_hdr);		/* Space for 32bit block number star-key */
	star_rec_hdr32->rsiz = bstar_rec_size(FALSE);
	SET_CMPC(star_rec_hdr32, 0);
	BLK_ADDR(star_rec_hdr64, SIZEOF(rec_hdr), rec_hdr);		/* Space for 64bit block number star-key */
	star_rec_hdr64->rsiz = bstar_rec_size(TRUE);
	SET_CMPC(star_rec_hdr64, 0);
	star_rec_hdr = long_blk_id ? star_rec_hdr64 : star_rec_hdr32;
	level = cur_level;
	max_fill_sav = max_fill = (0 == level) ? d_max_fill : i_max_fill;
<<<<<<< HEAD
	assert(0 <= max_fill);

=======
	if (create_root = ((level == gv_target->hist.depth) && (0 < level))) /* WARNING: assigment */
	{	/* MUPIP REORG -UPGRADE starts from the top down as opposed to regular REORG which works from the
		 * bottom up. As a result, the cur_level from the caller in a REORG -UPGRADE might be a root.
		 */
		assert(mu_upgrade_in_prog);
	}
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
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
	if (cdb_sc_normal != locate_block_split_point(old_blk1_hist_ptr, level, old_blk1_sz,
		max_fill, &old_blk1_last_rec_size, new_blk1_last_key, &new_blk1_last_keysz, &new_leftblk_top_off))
	{
		assert(t_tries < CDB_STAGNATE);
		NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
		CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
	}
	if ((new_leftblk_top_off + bstar_rec_sz >= old_blk1_sz) && !mu_upgrade_in_prog)
	{	/* Avoid split to create a small right sibling. Note this should not happen often when tolerance is high */
		CLEAR_BLKFMT_AND_RETURN(cdb_sc_oprnotneeded);
	}
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
	right_index = t_create(allocation_clue++, (unsigned char *)bs_ptr1, 0, 0, level);
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
		left_index = t_create(allocation_clue++, (unsigned char *)bs_ptr1, 0, 0, level);
	else
		t_write(old_blk1_hist_ptr, (unsigned char *)bs_ptr1, 0, 0, level, FALSE, TRUE, GDS_WRITE_KILLTN);
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
<<<<<<< HEAD
	assert(!mu_reorg_upgrd_dwngrd_in_prog || max_fill);
	for (;;) 	/* ========== loop through ancestors as necessary ======= */
=======
	assert(!mu_upgrade_in_prog || max_fill);
	if (mu_upgrade_in_prog)
		reserve_bytes = 0;				/* REORG -UPGRADE is top down, so zero the reserve bytes */
	while (!create_root) 	/* ========== loop through ancestors as necessary ======= */
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
	{
		if (create_root = (level == gv_target->hist.depth)) /* WARNING: assigment */
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
		if (mu_upgrade_in_prog)
			upgrade_block_split_format = blk_hdr_ptr->bver;
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
			if (cdb_sc_normal != gvcst_expand_any_key(old_blk1_hist_ptr,
				old_blk1_base + old_blk1_hist_ptr->curr_rec.offset,
				&curr_prev_key[0], &rec_size, &tkeylen, &tkeycmpc, NULL))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			if (old_ances_currkeycmpc)
				memcpy(ances_currkey, &curr_prev_key[0], old_ances_currkeycmpc);
		}
		if (old_ances_currkeylen)
		{
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
		if ((old_blk1_sz + delta) > (blk_size - reserve_bytes))
		{
			split_required = TRUE;
			if (level == gv_target->hist.depth)
			{
				create_root = TRUE;
				if ((MAX_BT_DEPTH - 1) <= level)
					CLEAR_BLKFMT_AND_RETURN(cdb_sc_maxlvl);		/* maximum level reached */
			}
			if (max_fill + bstar_rec_sz > old_blk1_sz)
			{	/* need more space than what was in the old block, so new block will be "too big" */
				if (((SIZEOF(blk_hdr) + bstar_rec_sz) == old_blk1_sz) && !mu_upgrade_in_prog)
					CLEAR_BLKFMT_AND_RETURN(cdb_sc_oprnotneeded);	/* Improve code to avoid this */
				max_fill = old_blk1_sz - bstar_rec_sz;
			}
			status = locate_block_split_point(old_blk1_hist_ptr, level, old_blk1_sz, max_fill,
				&old_blk1_last_rec_size, new_blk1_last_key, &new_blk1_last_keysz, &new_leftblk_top_off);
			if ((cdb_sc_normal != status) || (new_leftblk_top_off >= old_blk1_sz) || (0 == new_blk1_last_keysz))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			assert((bstar_rec_sz != old_blk1_last_rec_size) || mu_upgrade_in_prog);
			old_right_piece_len = old_blk1_sz - new_leftblk_top_off;
			new_blk2_frec_base = new_blk1_top = old_blk1_base + new_leftblk_top_off;
			if (bstar_rec_sz == old_right_piece_len)
				new_rtblk_star_only = TRUE;
			else
				new_rtblk_star_only = FALSE;
			if (new_leftblk_top_off == old_blk1_hist_ptr->curr_rec.offset)
			{	/* inserted key will be the first record of new right block */
				new_ins_keylen = new_ins_keysz;
				new_ins_keycmpc = 0;
				delta = (int)(bstar_rec_sz + new_ins_keylen - old_ances_currkeylen + new_ances_currkeylen);
			} else
			{	/* process 1st record of new right block */
				BLK_ADDR(newblk2_first_key, MAX_KEY_SZ + 1, unsigned char);
				READ_RECORD(status, &rec_size, &tkeycmpc, &newblk2_first_keylen, newblk2_first_key,
						level, old_blk1_hist_ptr, new_blk2_frec_base);
				if (cdb_sc_normal == status)
				{
					memcpy(newblk2_first_key, &new_blk1_last_key[0], tkeycmpc); /* compressed piece */
					new_blk2_rem =  new_blk2_frec_base + SIZEOF(rec_hdr) + newblk2_first_keylen;
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
			/* else old_blk1_hist_ptr->curr_rec will be newblk2_first_key */
			if ((old_blk1_hist_ptr->curr_rec.offset + old_ances_currkeylen + bstar_rec_sz) < new_leftblk_top_off)
			{	/* in this case prev_rec (if it exists), new key and curr_rec should go into left block */
				if ((new_leftblk_top_off + delta - old_blk1_last_rec_size + bstar_rec_sz)
					<= (blk_size - (mu_upgrade_in_prog ? 0 : reserve_bytes)))
					insert_in_left = TRUE;
				else
				{	/* cannot handle this now */
					assert(!mu_upgrade_in_prog);
					CLEAR_BLKFMT_AND_RETURN(cdb_sc_oprnotneeded);
				}
			} else if ((old_blk1_hist_ptr->curr_rec.offset + old_ances_currkeylen + bstar_rec_sz) > new_leftblk_top_off)
			{	/* if old_blk1_hist_ptr->curr_rec is the first key in old_blk1
				   then in new right block,
				   	new_ins_key will be the 1st record key and
					curr_rec will be 2nd record and
					there will be no prev_rec in right block.
				   Else (if curr_rec is not first key)
					there will be some records before new_ins_key, at least prev_rec */
				delta =(int)(bstar_rec_sz + new_ins_keylen - old_ances_currkeylen + new_ances_currkeylen
					+ ((0 == new_ins_keycmpc) ? 0 : (EVAL_CMPC((rec_hdr_ptr_t)new_blk2_frec_base))));
				if ((SIZEOF(blk_hdr) + old_right_piece_len + delta)
					<= (blk_size - (mu_upgrade_in_prog ? 0 : reserve_bytes)))
				{
					insert_in_left = FALSE;
					if ((new_leftblk_top_off + bstar_rec_sz) >= old_blk1_sz)
					{	/* cannot handle this now */
						assert(!mu_upgrade_in_prog);
						CLEAR_BLKFMT_AND_RETURN(cdb_sc_oprnotneeded);
					}
				} else
				{	/* cannot handle this now */
					assert(!mu_upgrade_in_prog);
					CLEAR_BLKFMT_AND_RETURN(cdb_sc_oprnotneeded);
				}
			} else
			{	/* in this case prev_rec (if it exists), new key and curr_rec should go into left block
				 * and curr_rec becomes the last record (*-key) of left new block
				 */
				delta = bstar_rec_sz + new_ins_keylen;
				if ((new_leftblk_top_off + delta)
					<= (blk_size - (mu_upgrade_in_prog ? 0 : reserve_bytes)))
					insert_in_left = TRUE;
				else
				{	/* cannot handle this now */
					assert(!mu_upgrade_in_prog);
					CLEAR_BLKFMT_AND_RETURN(cdb_sc_oprnotneeded);
				}
			}
		}	/* end if split required */
		else
			split_required = FALSE;
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
				BLK_SEG(bs_ptr2, ances_currkey + new_ances_currkeycmpc, new_ances_currkeylen);
			ins_off = blk_seg_cnt;
			BLK_SEG(bs_ptr2, zeroes, blk_id_sz);
			if (0 < (old_blk1_base + old_blk1_sz - old_blk_after_currec))
				BLK_SEG(bs_ptr2, old_blk_after_currec, old_blk1_base + old_blk1_sz - old_blk_after_currec);
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			t_write(&gv_target->hist.h[level], (unsigned char *)bs_ptr1, ins_off, right_index,
				level, first_copy, FALSE, GDS_WRITE_KILLTN);
			break;
		}
		/* if SPLIT REQUIRED */
		if (insert_in_left) /* new_ins_key will go to left block */
		{	/* LEFT BLOCK */
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
			if (old_blk_after_currec < new_blk1_top) /* curr_rec is not the last record of new left block */
			{
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
				assert (old_blk_after_currec == new_blk1_top);
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr));
				ins_off = blk_seg_cnt;
				BLK_SEG(bs_ptr2, zeroes, blk_id_sz);
			}
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			if (create_root)
				left_index = t_create(allocation_clue++, (unsigned char *)bs_ptr1, ins_off, right_index, level);
			else
				t_write(&gv_target->hist.h[level], (unsigned char *)bs_ptr1, ins_off, right_index,
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
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			right_index = t_create(allocation_clue++, (unsigned char *)bs_ptr1, 0, 0, level);
			(*blks_created)++;
		} else	/* end if insert_in_left */
		{	/* new_ins_key to be inserted in right block */
			/* RIGHT BLOCK */
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
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			if (create_root)
				left_index = t_create(allocation_clue++, (unsigned char *)bs_ptr1, 0, 0, level);
			else
				t_write(&gv_target->hist.h[level], (unsigned char *)bs_ptr1, 0, 0,
					level, TRUE, TRUE, GDS_WRITE_KILLTN);
			/* RIGHT BLOCK */
			BLK_INIT(bs_ptr2, bs_ptr1);
			if (new_leftblk_top_off < old_blk1_hist_ptr->curr_rec.offset)
			{	/* anything before curr_rec */
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
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				NONTP_TRACE_HIST_MOD(old_blk1_hist_ptr, t_blkmod_mu_split);
				CLEAR_BLKFMT_AND_RETURN(cdb_sc_blkmod);
			}
			right_index = t_create(allocation_clue++, (unsigned char *)bs_ptr1, ins_off, right_index, level);
			(*blks_created)++;
		}	/* endif new_ins_key inserted in right block */
		BLK_ADDR(new_ins_key, new_blk1_last_keysz, unsigned char);
		memcpy(new_ins_key, &new_blk1_last_key[0], new_blk1_last_keysz);
		new_ins_keysz = new_blk1_last_keysz;
		if (create_root)
<<<<<<< HEAD
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
				return cdb_sc_blkmod;
			}
			cse = t_write(&gv_target->hist.h[level], (unsigned char *)bs_ptr1, ins_off, left_index,
				level + 1, TRUE, FALSE, GDS_WRITE_KILLTN);
			UNUSED(cse);
			t_write_root(ins_off2, right_index);	/* create a sibling cw-set-element to store ins_off2/right_index */
			(*lvls_increased)++;
=======
>>>>>>> f9ca5ad6 (GT.M V7.1-000)
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
		cse = t_write(&gv_target->hist.h[level], (unsigned char *)bs_ptr1, ins_off, left_index,
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
enum cdb_sc locate_block_split_point(srch_blk_status *blk_stat, int level, int cur_blk_size, int max_fill, int *last_rec_size,
					unsigned char *last_key, int *last_keysz, int *top_off)
{
	int		rec_size, tkeycmpc;
	enum cdb_sc	status;
	sm_uc_ptr_t 	blk_base, rec_base;

	*last_keysz = 0;
	*top_off = SIZEOF(blk_hdr);
	*last_rec_size = 0;
	blk_base = blk_stat->buffaddr;
	rec_base = blk_base + SIZEOF(blk_hdr);
	/* max_fill is computed based on the fill factor after taking reserved_bytes into account. But since MAX_RESERVED_B
	 * macro (which is used by MUPIP SET to limit the reserved_bytes value to not go very close to the block_size value)
	 * ensures we leave space for at least SIZEOF(blk_hdr) so we are guaranteed *top_off (which is == SIZEOF(blk_hdr))
	 * is less than max_fill. This guarantees that we will always return with *top_off at least 1 record past the
	 * beginning of the block. This is necessary to ensure "mu_split" creates a split point that has at least one record
	 * in both the split blocks (or else issue #349 occurs). Assert that.
	 */
	assert(*top_off < max_fill);
	do
	{
		READ_RECORD(status, &rec_size, &tkeycmpc, last_keysz, last_key,
				level, blk_stat, rec_base);
		*top_off += rec_size;
		*last_keysz += tkeycmpc;
		rec_base += rec_size;
		*last_rec_size = rec_size;
		if ((cdb_sc_starrecord == status) && (*top_off == cur_blk_size))
			break;
		if (cdb_sc_normal != status)
		{
			assert(t_tries < CDB_STAGNATE);
			NONTP_TRACE_HIST_MOD(blk_stat, t_blkmod_mu_split);
			return cdb_sc_blkmod; /* block became invalid */
		}
	} while (*top_off < max_fill);
	if (*top_off > cur_blk_size
		|| (((blk_hdr_ptr_t)blk_base)->levl != level)
		|| (((blk_hdr_ptr_t)blk_base)->bsiz != cur_blk_size))
	{
		assert(t_tries < CDB_STAGNATE);
		NONTP_TRACE_HIST_MOD(blk_stat, t_blkmod_mu_split);
		return cdb_sc_blkmod; /* block became invalid */
	}
	return cdb_sc_normal;
}
