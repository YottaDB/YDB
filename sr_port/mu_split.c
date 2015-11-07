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

GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF gd_region        *gv_cur_region;
GBLREF char		*update_array, *update_array_ptr;
GBLREF uint4		update_array_size;	/* for the BLK_* macros */
GBLREF cw_set_element	cw_set[];
GBLREF unsigned char	cw_set_depth;
GBLREF unsigned int     t_tries;
GBLREF gv_key           *gv_currkey;
GBLREF gv_key           *gv_currkey_next_reorg;

static int4 const   	zeroes = 0;
enum cdb_sc locate_block_split_point(sm_uc_ptr_t blk_base, int level, int cur_blk_size, int max_fill,
int *last_rec_size, unsigned char last_key[], int *last_keysz, int *top_off);


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
	boolean_t	first_copy, new_rtblk_star_only, create_root = FALSE, split_required, insert_in_left;
	unsigned char	curr_prev_key[MAX_KEY_SZ+1], new_blk1_last_key[MAX_KEY_SZ+1];
	unsigned short  temp_ushort;
	int		rec_size, new_ins_keycmpc, tkeycmpc, new_ances_currkeycmpc, old_ances_currkeycmpc;
	int		tmp_cmpc;
	block_index	left_index, right_index;
	block_offset 	ins_off, ins_off2;
	int		level;
	int		new_ins_keysz, new_ances_currkeysz, new_blk1_last_keysz, newblk2_first_keysz, next_gv_currkeysz;
	int		old_ances_currkeylen, new_ins_keylen, new_ances_currkeylen, tkeylen, newblk2_first_keylen;
	int		old_blk1_last_rec_size, old_blk1_sz, save_blk_piece_len, old_right_piece_len;
	int		delta, max_fill;
	enum cdb_sc	status;
	int		blk_seg_cnt, blk_size, new_leftblk_top_off;
	block_id	allocation_clue;
	sm_uc_ptr_t 	rPtr1, rPtr2, rec_base, key_base, next_gv_currkey,
			bn_ptr1, bn_ptr2, save_blk_piece,
			old_blk_after_currec, ances_currkey,
			old_blk1_base,
			new_blk1_top, new_blk2_top,
			new_blk2_frec_base, new_blk2_rem,
			newblk2_first_key, new_ins_key;
	blk_segment     *bs_ptr1, *bs_ptr2;
	cw_set_element  *cse;
	rec_hdr_ptr_t	star_rec_hdr, new_rec_hdr1a, new_rec_hdr1b, new_rec_hdr2, root_hdr;
	blk_hdr_ptr_t	blk_hdr_ptr;

	blk_size = cs_data->blk_size;
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */

	BLK_ADDR(star_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
	star_rec_hdr->rsiz = BSTAR_REC_SIZE;
	SET_CMPC(star_rec_hdr, 0);
	level = cur_level;
	max_fill = (0 == level)? d_max_fill : i_max_fill;

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
	blk_hdr_ptr = (blk_hdr_ptr_t)(gv_target->hist.h[level].buffaddr);
	old_blk1_base = (sm_uc_ptr_t)blk_hdr_ptr;
	old_blk1_sz = blk_hdr_ptr->bsiz;
	new_blk2_top = old_blk1_base + old_blk1_sz;
	if (cdb_sc_normal != (status = locate_block_split_point (old_blk1_base, level, old_blk1_sz, max_fill,
		&old_blk1_last_rec_size, new_blk1_last_key, &new_blk1_last_keysz, &new_leftblk_top_off)))
	{
		assert(t_tries < CDB_STAGNATE);
		return cdb_sc_blkmod;
	}
	if (new_leftblk_top_off + BSTAR_REC_SIZE >= old_blk1_sz)
		/* Avoid split to create a small right sibling. Note this should not happen often when tolerance is high */
		return cdb_sc_oprnotneeded;
	old_right_piece_len = old_blk1_sz - new_leftblk_top_off;
	new_blk2_frec_base = old_blk1_base + new_leftblk_top_off;
	BLK_ADDR(newblk2_first_key, MAX_KEY_SZ + 1, unsigned char);
	READ_RECORD(status, &rec_size, &tkeycmpc, &newblk2_first_keylen, newblk2_first_key,
			level, old_blk1_base, new_blk2_frec_base);
	if (cdb_sc_normal != status) /* restart for cdb_sc_starrecord too, because we eliminated the possibility already */
	{
		assert(t_tries < CDB_STAGNATE);
		return cdb_sc_blkmod;
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

	/* Create new split piece, we already know that this will not be *-rec only.
	 * Note that this has to be done BEFORE modifying working block as building this buffer relies on the
	 * working block to be pinned which is possible only if this cw-set-element is created ahead of that
	 * of the working block (since order in which blocks are built is the order in which cses are created).
	 */
	BLK_INIT(bs_ptr2, bs_ptr1);
	BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1b, SIZEOF(rec_hdr));
	BLK_SEG(bs_ptr2, newblk2_first_key, newblk2_first_keysz);
	BLK_SEG(bs_ptr2, new_blk2_rem, new_blk2_top - new_blk2_rem);
	if (!BLK_FINI(bs_ptr2, bs_ptr1))
	{
		assert(t_tries < CDB_STAGNATE);
		return cdb_sc_blkmod;
	}
        allocation_clue = ALLOCATION_CLUE(cs_data->trans_hist.total_blks);
	right_index = t_create(allocation_clue++, (unsigned char *)bs_ptr1, 0, 0, level);
	(*blks_created)++;

	/* Modify working block removing split piece */
	BLK_INIT(bs_ptr2, bs_ptr1);
	if (0 == level)
	{
		BLK_SEG(bs_ptr2, old_blk1_base + SIZEOF(blk_hdr), new_leftblk_top_off - SIZEOF(blk_hdr));
	}
	else
	{
		BLK_SEG(bs_ptr2, old_blk1_base + SIZEOF(blk_hdr),
			new_leftblk_top_off - SIZEOF(blk_hdr) - old_blk1_last_rec_size);
		BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr) );
		BLK_ADDR(bn_ptr1, SIZEOF(block_id), unsigned char);
		memcpy(bn_ptr1, old_blk1_base + new_leftblk_top_off - SIZEOF(block_id), SIZEOF(block_id));
		BLK_SEG(bs_ptr2, bn_ptr1, SIZEOF(block_id));
	}
	if ( !BLK_FINI(bs_ptr2, bs_ptr1))
	{
		assert(t_tries < CDB_STAGNATE);
		return cdb_sc_blkmod;
	}
	t_write(&gv_target->hist.h[level], (unsigned char *)bs_ptr1, 0, 0, level, FALSE, TRUE, GDS_WRITE_KILLTN);

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
	for(;;) 	/* ========== loop through ancestors as necessary ======= */
	{
		level ++;
		max_fill = i_max_fill;
		/*
		old_blk_after_currec = remaining of current block after currec
		ances_currkey = old real value of currkey in ancestor block
		*/
		blk_hdr_ptr = (blk_hdr_ptr_t)(gv_target->hist.h[level].buffaddr);
		old_blk1_base = (sm_uc_ptr_t)blk_hdr_ptr;
		old_blk1_sz = blk_hdr_ptr->bsiz;
		new_blk2_top = old_blk1_base + old_blk1_sz;
		rec_base = old_blk1_base + gv_target->hist.h[level].curr_rec.offset;
		GET_RSIZ(rec_size, rec_base);
		old_blk_after_currec = rec_base + rec_size;
		old_ances_currkeycmpc = EVAL_CMPC((rec_hdr_ptr_t)rec_base);
		old_ances_currkeylen = rec_size - BSTAR_REC_SIZE;
		if (INVALID_RECORD(level, rec_size,  old_ances_currkeylen, old_ances_currkeycmpc))
		{
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_blkmod;
		}
		if (0 == old_ances_currkeylen)
		{
			if (0 != old_ances_currkeycmpc)
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			new_ances_currkeycmpc = new_ances_currkeylen = 0;
		}
		else
		{
			BLK_ADDR(ances_currkey, MAX_KEY_SZ + 1, unsigned char);
			key_base = rec_base +  SIZEOF(rec_hdr);
		}
		new_ances_currkeysz = old_ances_currkeycmpc + old_ances_currkeylen;
		if (SIZEOF(blk_hdr) != gv_target->hist.h[level].curr_rec.offset) /* cur_rec is not first key */
		{
			if (cdb_sc_normal != (status = gvcst_expand_any_key(old_blk1_base,
				old_blk1_base + gv_target->hist.h[level].curr_rec.offset,
				&curr_prev_key[0], &rec_size, &tkeylen, &tkeycmpc, NULL)))
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
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
		if (SIZEOF(blk_hdr) != gv_target->hist.h[level].curr_rec.offset)
		{
			/* new_ins_key will be inseted after curr_prev_key */
			GET_CMPC(new_ins_keycmpc, curr_prev_key, new_ins_key);
		}
		else
			new_ins_keycmpc = 0; /* new_ins_key will be the 1st key */
		new_ins_keylen = new_ins_keysz - new_ins_keycmpc ;

		delta = BSTAR_REC_SIZE + new_ins_keylen - old_ances_currkeylen + new_ances_currkeylen;
		if (old_blk1_sz + delta > blk_size - cs_data->reserved_bytes) /* split required */
		{
			split_required = TRUE;
			if (level == gv_target->hist.depth)
			{
				create_root = TRUE;
				if (MAX_BT_DEPTH - 1 <= level)  /* maximum level reached */
					return cdb_sc_maxlvl;
			}
			if (max_fill + BSTAR_REC_SIZE > old_blk1_sz)
			{
				if (SIZEOF(blk_hdr) + BSTAR_REC_SIZE == old_blk1_sz)
					return cdb_sc_oprnotneeded; /* Improve code to avoid this */
				max_fill = old_blk1_sz - BSTAR_REC_SIZE;
			}
			status = locate_block_split_point(old_blk1_base, level, old_blk1_sz, max_fill,
				&old_blk1_last_rec_size, new_blk1_last_key, &new_blk1_last_keysz, &new_leftblk_top_off);
			if (cdb_sc_normal != status || new_leftblk_top_off >= old_blk1_sz
				|| 0 == new_blk1_last_keysz)
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			assert(BSTAR_REC_SIZE != old_blk1_last_rec_size);
			old_right_piece_len = old_blk1_sz - new_leftblk_top_off;
			new_blk2_frec_base = new_blk1_top = old_blk1_base + new_leftblk_top_off;
			if (BSTAR_REC_SIZE == old_right_piece_len)
				new_rtblk_star_only = TRUE;
			else
				new_rtblk_star_only = FALSE;
			if (new_leftblk_top_off == gv_target->hist.h[level].curr_rec.offset)
			{
				/* inserted key will be the first record of new right block */
				new_ins_keylen = new_ins_keysz;
				new_ins_keycmpc = 0;
			}
			else
				/* process 1st record of new right block */
			{
				BLK_ADDR(newblk2_first_key, MAX_KEY_SZ + 1, unsigned char);
				READ_RECORD(status, &rec_size, &tkeycmpc, &newblk2_first_keylen, newblk2_first_key,
						level, old_blk1_base, new_blk2_frec_base);
				if (cdb_sc_normal == status)
				{
					memcpy(newblk2_first_key, &new_blk1_last_key[0], tkeycmpc); /* compressed piece */
					new_blk2_rem =  new_blk2_frec_base + SIZEOF(rec_hdr) + newblk2_first_keylen;
					newblk2_first_keysz = newblk2_first_keylen + tkeycmpc;
					BLK_ADDR(new_rec_hdr2, SIZEOF(rec_hdr), rec_hdr);
					new_rec_hdr2->rsiz = newblk2_first_keysz + BSTAR_REC_SIZE;
					SET_CMPC(new_rec_hdr2, 0);
				}
				else if (cdb_sc_starrecord != status || !new_rtblk_star_only)
				{
					assert(t_tries < CDB_STAGNATE);
					return cdb_sc_blkmod;
				}
			}
			/* else gv_target->hist.h[level].curr_rec will be newblk2_first_key */

			if (new_leftblk_top_off >  gv_target->hist.h[level].curr_rec.offset +
				old_ances_currkeylen + BSTAR_REC_SIZE)
			{
				/* in this case prev_rec (if exists), new key and curr_rec should go into left block */
				if (new_leftblk_top_off + delta - old_blk1_last_rec_size + BSTAR_REC_SIZE
					<= blk_size - cs_data->reserved_bytes)
					insert_in_left = TRUE;
				else
				{
					/* cannot handle it now */
					return cdb_sc_oprnotneeded;
				}
			}
			else if (new_leftblk_top_off <  gv_target->hist.h[level].curr_rec.offset +
				old_ances_currkeylen + BSTAR_REC_SIZE)
			{
				/* if gv_target->hist.h[level].curr_rec is the first key in old_blk1
				   then in new right block,
				   	new_ins_key will be the 1st record key and
					curr_rec will be 2nd record and
					there will be no prev_rec in right block.
				   Else (if curr_rec is not first key)
					there will be some records before new_ins_key, at least prev_rec */
				delta = (int)(BSTAR_REC_SIZE + new_ins_keylen
					- old_ances_currkeylen + new_ances_currkeylen
					+ ((0 == new_ins_keycmpc) ? 0 : (EVAL_CMPC((rec_hdr_ptr_t)new_blk2_frec_base))));
				if (SIZEOF(blk_hdr) + old_right_piece_len + delta <= blk_size - cs_data->reserved_bytes)
				{
					insert_in_left = FALSE;
					if (new_leftblk_top_off + BSTAR_REC_SIZE >= old_blk1_sz)
					{
						/* cannot handle it now */
						return cdb_sc_oprnotneeded;
					}
				}
				else
				{
					/* cannot handle it now */
					return cdb_sc_oprnotneeded;
				}
			}
			else
			{
				/* in this case prev_rec (if exists), new key and curr_rec should go into left block
					and curr_rec will be the last record (*-key) of left new block */
				delta = BSTAR_REC_SIZE + new_ins_keylen;
				if (new_leftblk_top_off + delta <= blk_size - cs_data->reserved_bytes)
					insert_in_left = TRUE;
				else
				{
					/* cannot handle it now */
					return cdb_sc_oprnotneeded;
				}
			}
		} /* end if split required */
		else
			split_required = FALSE;
		BLK_ADDR(new_rec_hdr1a, SIZEOF(rec_hdr), rec_hdr);
		new_rec_hdr1a->rsiz = BSTAR_REC_SIZE + new_ins_keylen;
		SET_CMPC(new_rec_hdr1a, new_ins_keycmpc);
		BLK_ADDR(new_rec_hdr1b, SIZEOF(rec_hdr), rec_hdr);
		new_rec_hdr1b->rsiz = BSTAR_REC_SIZE + new_ances_currkeylen;
		SET_CMPC(new_rec_hdr1b, new_ances_currkeycmpc);
		BLK_ADDR(bn_ptr1, SIZEOF(block_id), unsigned char);
		/* child pointer of ances_currkey */
		memcpy(bn_ptr1, old_blk1_base + gv_target->hist.h[level].curr_rec.offset +
			SIZEOF(rec_hdr) + old_ances_currkeylen, SIZEOF(block_id));
		if (!split_required)
		{
			/* LEFT part of old BLOCK */
			BLK_INIT(bs_ptr2, bs_ptr1);
			if (SIZEOF(blk_hdr) < gv_target->hist.h[level].curr_rec.offset)
			{
				BLK_SEG(bs_ptr2, old_blk1_base + SIZEOF(blk_hdr),
					gv_target->hist.h[level].curr_rec.offset - SIZEOF(blk_hdr));
				first_copy = FALSE;
			} else
				first_copy = TRUE;
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1a, SIZEOF(rec_hdr));
			BLK_SEG(bs_ptr2, new_ins_key + new_ins_keycmpc, new_ins_keylen);
			BLK_SEG(bs_ptr2, bn_ptr1, SIZEOF(block_id));
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1b, SIZEOF(rec_hdr));
			if (0 < new_ances_currkeylen)
				BLK_SEG(bs_ptr2, ances_currkey + new_ances_currkeycmpc, new_ances_currkeylen);
			ins_off = blk_seg_cnt;
			BLK_SEG(bs_ptr2, (unsigned char *)&zeroes, SIZEOF(block_id));
			if (0 < old_blk1_base + old_blk1_sz - old_blk_after_currec)
				BLK_SEG(bs_ptr2, old_blk_after_currec,  old_blk1_base + old_blk1_sz - old_blk_after_currec);
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			t_write(&gv_target->hist.h[level], (unsigned char *)bs_ptr1, ins_off, right_index,
				level, first_copy, FALSE, GDS_WRITE_KILLTN);
			break;
		}
		/* if SPLIT REQUIRED */
		if (insert_in_left) /* new_ins_key will go to left block */
		{
			/* LEFT BLOCK */
			BLK_INIT(bs_ptr2, bs_ptr1);
			if (SIZEOF(blk_hdr) < gv_target->hist.h[level].curr_rec.offset)
			{
				BLK_SEG(bs_ptr2, old_blk1_base + SIZEOF(blk_hdr),
					gv_target->hist.h[level].curr_rec.offset - SIZEOF(blk_hdr));
				first_copy = FALSE;
			} else
				first_copy = TRUE;
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1a, SIZEOF(rec_hdr));
			BLK_SEG(bs_ptr2, new_ins_key + new_ins_keycmpc, new_ins_keylen);
			BLK_SEG(bs_ptr2, bn_ptr1, SIZEOF(block_id));
			if (old_blk_after_currec < new_blk1_top) /* curr_rec is not the last record of new left block */
			{
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1b, SIZEOF(rec_hdr));
				BLK_SEG(bs_ptr2, ances_currkey + new_ances_currkeycmpc, new_ances_currkeylen);
				ins_off = blk_seg_cnt;
				BLK_SEG(bs_ptr2, (unsigned char *)&zeroes, SIZEOF(block_id));
				save_blk_piece_len = (int)(new_blk1_top - old_blk1_last_rec_size - old_blk_after_currec);
				if (0 < save_blk_piece_len )
				{
 					if (old_blk_after_currec + save_blk_piece_len >= new_blk2_top)
					{
						assert(t_tries < CDB_STAGNATE);
						return cdb_sc_blkmod;
					}
					BLK_ADDR(save_blk_piece, save_blk_piece_len, unsigned char);
					memcpy(save_blk_piece, old_blk_after_currec, save_blk_piece_len);
					BLK_SEG(bs_ptr2, save_blk_piece, save_blk_piece_len);
				}
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr) );
				BLK_ADDR(bn_ptr2, SIZEOF(block_id), unsigned char);
				memcpy(bn_ptr2, new_blk1_top - SIZEOF(block_id), SIZEOF(block_id));
				BLK_SEG(bs_ptr2, bn_ptr2, SIZEOF(block_id));
			} else
			{
				assert (old_blk_after_currec == new_blk1_top);
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr) );
                                ins_off = blk_seg_cnt;
                                BLK_SEG(bs_ptr2, (unsigned char *)&zeroes, SIZEOF(block_id));

			}
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
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
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr) );
				BLK_ADDR(bn_ptr2, SIZEOF(block_id), unsigned char);
				memcpy(bn_ptr2, new_blk2_top - SIZEOF(block_id), SIZEOF(block_id));
				BLK_SEG(bs_ptr2, bn_ptr2, SIZEOF(block_id));
			} else
			{
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr2, SIZEOF(rec_hdr));
				BLK_SEG(bs_ptr2, newblk2_first_key, newblk2_first_keysz);
				save_blk_piece_len = (int)(new_blk2_top - new_blk2_rem);
				if (0 > save_blk_piece_len)
				{
					assert(t_tries < CDB_STAGNATE);
					return cdb_sc_blkmod;
				}
				BLK_ADDR(save_blk_piece, save_blk_piece_len, unsigned char);
				memcpy(save_blk_piece, new_blk2_rem, save_blk_piece_len);
				BLK_SEG(bs_ptr2, save_blk_piece, new_blk2_top - new_blk2_rem );
			}
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			right_index = t_create(allocation_clue++, (unsigned char *)bs_ptr1, 0, 0, level);
			(*blks_created)++;
		} /* end if insert_in_left */
		else
		{	/* new_ins_key to be inserted in right block */
			/* LEFT BLOCK */
			BLK_INIT(bs_ptr2, bs_ptr1);
			save_blk_piece_len = (int)(new_leftblk_top_off - SIZEOF(blk_hdr) - old_blk1_last_rec_size);
			if ((old_blk1_base + SIZEOF(blk_hdr) + save_blk_piece_len >= new_blk2_top) || (0 > save_blk_piece_len))
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			BLK_ADDR(save_blk_piece, save_blk_piece_len, unsigned char);
			memcpy(save_blk_piece, old_blk1_base + SIZEOF(blk_hdr), save_blk_piece_len);
			BLK_SEG(bs_ptr2, save_blk_piece, save_blk_piece_len);
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr) );
			BLK_ADDR(bn_ptr2, SIZEOF(block_id), unsigned char);
			memcpy(bn_ptr2, old_blk1_base + new_leftblk_top_off - SIZEOF(block_id), SIZEOF(block_id));
			BLK_SEG(bs_ptr2, bn_ptr2, SIZEOF(block_id));
			if ( !BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			if (create_root)
				left_index = t_create(allocation_clue++, (unsigned char *)bs_ptr1, 0, 0, level);
			else
				t_write(&gv_target->hist.h[level], (unsigned char *)bs_ptr1, 0, 0,
					level, TRUE, TRUE, GDS_WRITE_KILLTN);
			/* RIGHT BLOCK */
			BLK_INIT(bs_ptr2, bs_ptr1);
			if (new_leftblk_top_off < gv_target->hist.h[level].curr_rec.offset)
			{	/* anything before curr_rec */
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr2, SIZEOF(rec_hdr));
				BLK_SEG(bs_ptr2, newblk2_first_key, newblk2_first_keysz);
				save_blk_piece_len = (int)(gv_target->hist.h[level].curr_rec.offset -
					new_leftblk_top_off  - (new_blk2_rem - new_blk2_frec_base));
				if ((new_blk2_rem + save_blk_piece_len >= new_blk2_top) || (0 > save_blk_piece_len))
				{
					assert(t_tries < CDB_STAGNATE);
					return cdb_sc_blkmod;
				}
				BLK_ADDR(save_blk_piece, save_blk_piece_len, unsigned char);
				memcpy(save_blk_piece, new_blk2_rem, save_blk_piece_len);
				BLK_SEG(bs_ptr2, save_blk_piece, save_blk_piece_len);
			}
			/* Following else if may not be necessary. But I wanted it to be safe:Layek:10/3/2000 */
			else if (new_leftblk_top_off > gv_target->hist.h[level].curr_rec.offset)
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1a, SIZEOF(rec_hdr));
			BLK_SEG(bs_ptr2, new_ins_key + new_ins_keycmpc, new_ins_keylen);
			BLK_SEG(bs_ptr2, bn_ptr1, SIZEOF(block_id));
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1b, SIZEOF(rec_hdr));
			BLK_SEG(bs_ptr2, ances_currkey + new_ances_currkeycmpc, new_ances_currkeylen);
			ins_off = blk_seg_cnt;
			BLK_SEG(bs_ptr2, (unsigned char *)&zeroes, SIZEOF(block_id));
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
				return cdb_sc_blkmod;
			}
			right_index = t_create(allocation_clue++, (unsigned char *)bs_ptr1, ins_off, right_index, level);
			(*blks_created)++;
		} /* endif new_ins_key insered in right block */
		BLK_ADDR(new_ins_key, new_blk1_last_keysz, unsigned char);
		memcpy(new_ins_key, &new_blk1_last_key[0], new_blk1_last_keysz);
		new_ins_keysz = new_blk1_last_keysz;
		if (create_root)
		{
			BLK_ADDR(root_hdr, SIZEOF(rec_hdr), rec_hdr);
			root_hdr->rsiz = BSTAR_REC_SIZE + new_ins_keysz;
			SET_CMPC(root_hdr, 0);
			BLK_INIT(bs_ptr2, bs_ptr1);
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)root_hdr, SIZEOF(rec_hdr));
			BLK_SEG(bs_ptr2, new_ins_key, new_ins_keysz);
			ins_off = blk_seg_cnt;
			BLK_SEG(bs_ptr2, (unsigned char *)&zeroes, SIZEOF(block_id));
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr) );
			ins_off2 = blk_seg_cnt;
			BLK_SEG(bs_ptr2, (unsigned char *)&zeroes, SIZEOF(block_id));
			if (!BLK_FINI(bs_ptr2, bs_ptr1))
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			cse = t_write(&gv_target->hist.h[level], (unsigned char *)bs_ptr1, ins_off, left_index,
				level + 1, TRUE, FALSE, GDS_WRITE_KILLTN);
			t_write_root(ins_off2, right_index);	/* create a sibling cw-set-element to store ins_off2/right_index */
			(*lvls_increased)++;
			break;
		}

	} /* ========== End loop through ancestors as necessary ======= */

	/* gv_currkey_next_reorg for next iteration in mu_reorg */
	memcpy(&gv_currkey_next_reorg->base[0], next_gv_currkey, next_gv_currkeysz);
	gv_currkey_next_reorg->end = next_gv_currkeysz - 1;

	return cdb_sc_normal;

}
/* end mu_split() */

/*
-------------------------------------------------------------------------
locate_block_split_point ():
	This will split a block at a point given by fill factor
Input Parameter:
	blk_base = base of the block
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
enum cdb_sc locate_block_split_point(sm_uc_ptr_t blk_base, int level, int cur_blk_size, int max_fill,
int *last_rec_size, unsigned char last_key[], int *last_keysz, int *top_off)
{
	unsigned short	temp_ushort;
	int		tkeycmpc;
	int		rec_size;
	enum cdb_sc	status;
	sm_uc_ptr_t 	rPtr1, rPtr2, rec_base;

	*last_keysz = 0;
	*top_off = SIZEOF(blk_hdr);
	*last_rec_size = 0;
	rec_base = blk_base + SIZEOF(blk_hdr);
	while (*top_off < max_fill)
	{
		READ_RECORD(status, &rec_size, &tkeycmpc, last_keysz, last_key,
				level, blk_base, rec_base);
		*top_off += rec_size;
		*last_keysz += tkeycmpc;
		rec_base += rec_size;
		*last_rec_size = rec_size;
		if (cdb_sc_starrecord == status &&  *top_off == cur_blk_size)
			return cdb_sc_normal;
		else if (cdb_sc_normal != status)
		{
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_blkmod; /* block became invalid */
		}
	}/* end of "while" loop */
	if (*top_off > cur_blk_size || ((blk_hdr_ptr_t)blk_base)->levl != level  ||
		((blk_hdr_ptr_t)blk_base)->bsiz != cur_blk_size)
	{
		assert(t_tries < CDB_STAGNATE);
		return cdb_sc_blkmod; /* block became invalid */
	}
	return cdb_sc_normal;
}
