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

/******************************************************************************************
mu_clsce.c:
	Coalesce two adjacent blocks in GVT
Description:
	Join the working block (say, block1) and its right sibling (say, block2).
	In addition it needs to change the key value of the ancestor block(s).
	Say we have structure:
		block1: a(50),a(51),...a(58),a(59)
		block2: a(60),a(61),...a(68),a(69)
	After coalesce we have,
		block1: a(50),a(51),...a(58),a(59),a(60),...a(65)
		block2: a(66),a(67),a(68),a(69)
	We need to modify blocks
		1. block1
		2. block2
		3. ancestor of block1, key value "^a(59)" instead of "^a(65)"
		4. ancestor of block2, if it is different than ancestor of block1

	Some convention in naming:
		1. curr_key is the gv_currkey, which is first key of working block
		2. curr_key at level 'levelp' is the record's key value at blk1ptr->h[levelp].curr_offset
		3. a 'real key' value means a non-* key value (data blocks always have real value)
		4. working block means blk1ptr->h[level].blk_num (working block)
		5. rtsib block means blk2ptr->h[level].blk_num
		6. sz = size means uncompressed size of a key
		   len = length means compressed size of a key
*******************************************************************************************/

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
#include "t_write.h"
#include "mupip_reorg.h"

GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF char		*update_array, *update_array_ptr;
GBLREF uint4		update_array_size;	/* for the BLK_* macros */
GBLREF uint4            t_err;
GBLREF unsigned int     t_tries;
GBLREF gd_region        *gv_cur_region;
GBLREF gv_namehead	*gv_target;
GBLREF gv_key           *gv_currkey;
GBLREF gv_key           *gv_currkey_next_reorg;


/*************************************************************************************************
Input Parameters:
	gv_target: working block's history
	level : Level of working block and its right sibling
	d_blk_fill_size : Maximum fill allowed in a data block
	i_blk_fill_size : Maximum fill allowed in an index block
Output Parameters:
	kill_set_ptr : List of blocks to be freed from LBM (already killed in mu_clsce)
	remove_rtsib : if right sibling was completely merged with working
Returns:
	cdb_sc_normal on success
	Other wise error status
 *************************************************************************************************/
enum cdb_sc mu_clsce(int level, int i_max_fill, int d_max_fill, kill_set *kill_set_ptr, boolean_t *remove_rtsib)
{
	boolean_t	complete_merge = FALSE,
			old_ref_star_only = FALSE,
			new_rtsib_star_only = FALSE,
			star_only_merge = FALSE,
			blk2_ances_star_only = FALSE,
			delete_all_blk2_ances = TRUE,
			levelp_next_is_star, forward_process;
	unsigned char	oldblk1_prev_key[MAX_KEY_SZ+1],
			old_levelp_cur_prev_key[MAX_KEY_SZ+1],
			old_levelp_cur_key[MAX_KEY_SZ+1]; /* keys in private memory */
	unsigned short	temp_ushort;
	int		new_levelp_cur_cmpc, new_levelp_cur_next_cmpc, tkeycmpc,
			oldblk1_last_cmpc, newblk1_mid_cmpc, newblk1_last_cmpc;
	int		tmp_cmpc;
	int		levelp, level2;
	int		old_blk1_sz, old_blk2_sz;
	int		old_levelp_cur_prev_keysz,
			old_levelp_cur_keysz,
			old_levelp_cur_next_keysz,
			newblk1_last_keysz,
			newblk2_first_keysz,
			new_blk2_ances_first_keysz;
	int		old_levelp_cur_keylen,
			new_levelp_cur_keylen,
			old_levelp_cur_next_keylen,
			new_levelp_cur_next_keylen,
			oldblk1_last_keylen,
			newblk1_last_keylen,
			newblk2_first_keylen;
	int		rec_size, piece_len, tkeylen, old_levelp_rec_offset;
	int		blk_seg_cnt, blk_size;
	enum cdb_sc	status;
	sm_uc_ptr_t 	oldblk1_last_key, old_levelp_cur_next_key,
			newblk1_last_key, newblk2_first_key, new_blk2_ances_first_key; /* shared memory keys */
	sm_uc_ptr_t 	rec_base, old_levelp_blk_base,
			bn_ptr1, bn_ptr2, blk2_ances_remain, old_blk1_base, old_blk2_base,
			new_blk1_top, new_blk2_first_rec_base, new_blk2_remain; /* shared memory pointers */
	sm_uc_ptr_t 	rPtr1, rPtr2;
	rec_hdr_ptr_t	star_rec_hdr, old_last_rec_hdr1, new_rec_hdr1, new_rec_hdr2,
			blk2_ances_hdr, new_levelp_cur_hdr, new_levelp_cur_next_hdr;
	blk_segment	*bs_ptr1, *bs_ptr2;
	srch_hist	*blk1ptr, *blk2ptr; /* blk2ptr is for right sibling's hist from a minimum sub-tree containing both blocks */

	blk_size = cs_data->blk_size;
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */

	blk1ptr = &(gv_target->hist);
	blk2ptr = gv_target->alt_hist;
	old_blk1_base = blk1ptr->h[level].buffaddr;
	old_blk2_base = blk2ptr->h[level].buffaddr;
	old_blk1_sz = ((blk_hdr_ptr_t)old_blk1_base)->bsiz;
	old_blk2_sz = ((blk_hdr_ptr_t)old_blk2_base)->bsiz;
	if (0 != level && SIZEOF(blk_hdr) + BSTAR_REC_SIZE == old_blk1_sz)
		old_ref_star_only = TRUE;
	/* Search an ancestor block at levelp >= level+1,
	which has a real key value corresponding to the working block.
	This key value will be changed after coalesce.  */
	levelp = level;
	do
	{
		if (++levelp > blk1ptr->depth ||  levelp > blk2ptr->depth)
		{
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_blkmod;
		}
		old_levelp_blk_base = blk1ptr->h[levelp].buffaddr;
		old_levelp_rec_offset = blk1ptr->h[levelp].curr_rec.offset;
		rec_base = old_levelp_blk_base + old_levelp_rec_offset;
		GET_RSIZ(rec_size, rec_base);
	} while (BSTAR_REC_SIZE == rec_size); /* search ancestors to get a real value */

	/*
	old_levelp_cur_prev_key = real value of the key before the curr_key at levelp
	old_levelp_cur_prev_keysz = uncompressed size of the key
	Note: we may not have a previous key (old_levelp_cur_prev_keysz = 0)
	*/
	if (SIZEOF(blk_hdr) == old_levelp_rec_offset)
		old_levelp_cur_prev_keysz = 0;
	else
	{
		if (cdb_sc_normal != (status = gvcst_expand_any_key (old_levelp_blk_base, rec_base,
			&old_levelp_cur_prev_key[0], &rec_size, &tkeylen, &tkeycmpc, NULL)))
		{
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_blkmod;
		}
		old_levelp_cur_prev_keysz = tkeylen + tkeycmpc;
	}

	/*
	old_levelp_cur_key = real value of the curr_key at levelp
	old_levelp_cur_keysz = uncompressed size of the key
	old_levelp_cur_keylen = compressed size of the key
	*/
	READ_RECORD(status, &rec_size, &tkeycmpc, &old_levelp_cur_keylen, old_levelp_cur_key,
			levelp, old_levelp_blk_base, rec_base);
	if (cdb_sc_normal != status)
	{
		assert(t_tries < CDB_STAGNATE);
		return cdb_sc_blkmod;
	}
	if (old_levelp_cur_prev_keysz)
		memcpy(&old_levelp_cur_key[0], &old_levelp_cur_prev_key[0], tkeycmpc);
	rec_base += rec_size;
	old_levelp_cur_keysz = old_levelp_cur_keylen + tkeycmpc;

	/*
	old_levelp_cur_next_key = uncompressed value of the next right key of old_levelp_cur_key
	old_levelp_cur_next_keysz = uncomressed size of the key
	old_levelp_cur_next_keylen = comressed size of the key
		Note: we may not have a next key (old_levelp_cur_next_keysz = 0)
	*/
	BLK_ADDR(old_levelp_cur_next_key, MAX_KEY_SZ + 1, unsigned char);
	READ_RECORD(status, &rec_size, &tkeycmpc, &old_levelp_cur_next_keylen, old_levelp_cur_next_key,
			levelp, old_levelp_blk_base, rec_base);
	if (cdb_sc_starrecord == status)
		levelp_next_is_star = TRUE;
	else if (cdb_sc_normal != status)
	{
		assert(t_tries < CDB_STAGNATE);
		return cdb_sc_blkmod;
	} else
	{
		memcpy(old_levelp_cur_next_key, &old_levelp_cur_key[0], tkeycmpc);
		old_levelp_cur_next_keysz = old_levelp_cur_next_keylen + tkeycmpc;
		levelp_next_is_star = FALSE;
	}


	/*
	Now process the actual working block at current level
		oldblk1_last_key = real value of last key of the working block
			For index block decompress *-key
		oldblk1_last_keylen = compressed size of the last key
		oldblk1_last_cmpc = compression count of last key of working block
		old_last_rec_hdr1 = New working index block's last record header
	*/
	BLK_ADDR(oldblk1_last_key, MAX_KEY_SZ + 1, unsigned char);
	if (0 == level) /* data block */
	{
		if (cdb_sc_normal != (status = gvcst_expand_any_key (old_blk1_base, old_blk1_base + old_blk1_sz,
			oldblk1_last_key, &rec_size, &oldblk1_last_keylen, &oldblk1_last_cmpc, NULL)))
		{
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_blkmod;
		}
		rec_base = old_blk1_base + old_blk1_sz;
	} else  /* Index blocks */
	{
		/* Since we will join this working block with the right sibling,
		we need to remove the *-key at the end of working block
		and replace with actual key value (with required compression).
		We will get the real value of *-rec from its ancestor at levelp */
                memcpy (oldblk1_last_key, &old_levelp_cur_key[0], old_levelp_cur_keysz);
		if (!old_ref_star_only) /* if the index block is not a *-key only block) */
		{
			if (cdb_sc_normal != (status = gvcst_expand_any_key (old_blk1_base,
				old_blk1_base + old_blk1_sz - BSTAR_REC_SIZE, &oldblk1_prev_key[0],
				&rec_size, &tkeylen, &tkeycmpc, NULL)))
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			GET_CMPC(oldblk1_last_cmpc, oldblk1_prev_key, old_levelp_cur_key);
			oldblk1_last_keylen = old_levelp_cur_keysz - oldblk1_last_cmpc;
		}
		else /* working block has a *-key record only */
		{
			/* get key value from ancestor blocks key */
			oldblk1_last_keylen = old_levelp_cur_keysz;
			oldblk1_last_cmpc = 0;
        	}
                BLK_ADDR(old_last_rec_hdr1, SIZEOF(rec_hdr), rec_hdr);
                old_last_rec_hdr1->rsiz = BSTAR_REC_SIZE + oldblk1_last_keylen;
                SET_CMPC(old_last_rec_hdr1, oldblk1_last_cmpc);
	}

	/*
	newblk1_last_key = new working blocks final appended key
	newblk1_mid_cmpc = new working blocks firstly appended key's cmpc
	newblk1_last_keysz = new working blocks lastly appended key's size
	star_only_merge = TRUE, we can append only a *-key record into the working block
				(decompressing current *-key)
	complete_merge = TRUE, rtsib can be completely merged with working block
	piece_len = Size of data from old rtsibling to be merged into working block (includes rec_hdr size)
	*/
	BLK_ADDR(newblk1_last_key, MAX_KEY_SZ + 1, unsigned char);
	rec_base = old_blk2_base + SIZEOF(blk_hdr);
	READ_RECORD(status, &rec_size, &newblk1_last_cmpc, &newblk1_last_keylen, newblk1_last_key,
			level, old_blk2_base, rec_base);
	if (cdb_sc_starrecord == status) /* rtsib index block has *-record only */
	{
		if (old_blk1_sz + oldblk1_last_keylen + BSTAR_REC_SIZE > i_max_fill ) /* cannot fit even one record */
			return cdb_sc_oprnotneeded;
		star_only_merge = TRUE;
		complete_merge = TRUE;
		rec_base = old_blk2_base + SIZEOF(blk_hdr) + BSTAR_REC_SIZE;
	} else if (cdb_sc_normal != status)
	{
		assert(t_tries < CDB_STAGNATE);;
		return cdb_sc_blkmod;
	} else /* for both data and non-* index block */
	{
		newblk1_last_keysz = newblk1_last_keylen; /* first key has uncompressed real value */
		GET_CMPC(newblk1_mid_cmpc, oldblk1_last_key, newblk1_last_key);
		piece_len = rec_size - newblk1_mid_cmpc;
		if (level == 0) /* data block */
		{
			if (old_blk1_sz + piece_len > d_max_fill ) /* cannot fit even one record */
				return cdb_sc_oprnotneeded;
		} else /* else an index block */
		{
			if (old_blk1_sz + oldblk1_last_keylen + BSTAR_REC_SIZE > i_max_fill ) /* cannot fit even one record */
				return cdb_sc_oprnotneeded;
			if (old_blk1_sz + oldblk1_last_keylen + piece_len + BSTAR_REC_SIZE > i_max_fill )
					star_only_merge = TRUE; /* can fit only a *-record */
		}
		rec_base += rec_size;
	}

	/* new_blk2_first_rec_base and new_blk1_top is set with final value for star_only_merge  for index block */
	new_blk2_first_rec_base = new_blk1_top = rec_base;
	if (!star_only_merge)
	{
		BLK_ADDR(new_rec_hdr1, SIZEOF(rec_hdr), rec_hdr);
		new_rec_hdr1->rsiz = piece_len;
		SET_CMPC(new_rec_hdr1, newblk1_mid_cmpc);
	}
	/* else only new_blk1_last_key will be appeneded in working block */


	/* find a piece of the right sibling to be copied into the working block.
	Note: rec_base points to 2nd record of old rtsib */
	if (0 == level) /* if data block */
	{
		complete_merge = TRUE;
		while (rec_base < old_blk2_base + old_blk2_sz)
		{
			GET_RSIZ(rec_size, rec_base);
			if (old_blk1_sz + piece_len + rec_size > d_max_fill )
			{
				complete_merge = FALSE;
				break;
			}
			READ_RECORD(status, &rec_size, &newblk1_last_cmpc, &newblk1_last_keylen, newblk1_last_key,
					level, old_blk2_base, rec_base);
			if (cdb_sc_normal != status)
			{
				assert(t_tries < CDB_STAGNATE);
				return cdb_sc_blkmod;
			}
			newblk1_last_keysz = newblk1_last_keylen + newblk1_last_cmpc;
			rec_base += rec_size;
			piece_len += rec_size;
		}/* end of "while" loop */
		new_blk1_top = new_blk2_first_rec_base = rec_base;
	} else /* index block */
	{
		if (!star_only_merge)
		{
			/* we know we can fit more record in working block and rtsibling has more records */
			complete_merge = TRUE;
			while (rec_base < old_blk2_base + old_blk2_sz)
			{
				GET_RSIZ(rec_size, rec_base);
				if (BSTAR_REC_SIZE == rec_size)
				{
					rec_base += rec_size;
					piece_len += rec_size;
					break; /* already we know we can fit this *-record in working block */
				}
				READ_RECORD(status, &rec_size, &newblk1_last_cmpc, &newblk1_last_keylen, newblk1_last_key,
						level, old_blk2_base, rec_base);
				if (cdb_sc_normal != status)
				{
					assert(t_tries < CDB_STAGNATE);
					return cdb_sc_blkmod;
				}
				newblk1_last_keysz = newblk1_last_keylen + newblk1_last_cmpc;
				rec_base += rec_size;
				piece_len += rec_size;
				if (old_blk1_sz + oldblk1_last_keylen + piece_len + BSTAR_REC_SIZE > i_max_fill )
				{
					complete_merge = FALSE;
					break;
				}
			}/* end of "while" loop */
			new_blk1_top = new_blk2_first_rec_base = rec_base;
		} /* end else  *-only merge */
	} /* end else index block */

	if (!complete_merge)
	{
		/*
		Adjust new right sibling's buffer
		if new_rtsib_star_only == TRUE then
			new right sibling will have a *-key record only
		else
			new_blk2_remain = base pointer of buffer including 1st record but exclude rec_header and key
			new_blk2_first_keysz = size of new rtsib block's first key
		*/
		BLK_ADDR(newblk2_first_key, MAX_KEY_SZ + 1, unsigned char);
		READ_RECORD(status, &rec_size, &tkeycmpc, &newblk2_first_keylen, newblk2_first_key,
				level, old_blk2_base, new_blk2_first_rec_base);
		if (cdb_sc_starrecord == status) /* new rtsib will have a *-record only */
			new_rtsib_star_only = TRUE;
		else if (cdb_sc_normal != status)
		{
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_blkmod;
		} else
		{
			memcpy(newblk2_first_key, newblk1_last_key, tkeycmpc); /* copy the compressed piece */
			newblk2_first_keysz = newblk2_first_keylen + tkeycmpc;
			new_blk2_remain = new_blk2_first_rec_base + SIZEOF(rec_hdr) + newblk2_first_keylen;
			BLK_ADDR(new_rec_hdr2, SIZEOF(rec_hdr), rec_hdr);
			new_rec_hdr2->rsiz = rec_size + tkeycmpc;
			SET_CMPC(new_rec_hdr2, 0);
		}
	}


	/*
	if complete_merge and level+1 <= level2 < levelp,
		if blk2ptr->h[level2].blk_num is *-record block then
			delete it
		else
			prepare to update  blk2ptr->h[level2].blk_num (for first level2>level+1)
			that is, we will delete 1st record pointing to rtsib which is merged with working.
	*/
	else /* complete merge */
	{
		for (level2 = level + 1; level2 < levelp; level2++)
		{
			if (SIZEOF(blk_hdr) + BSTAR_REC_SIZE == ((blk_hdr_ptr_t)blk2ptr->h[level2].buffaddr)->bsiz)
			{
				kill_set_ptr->blk[kill_set_ptr->used].flag = 0;
				kill_set_ptr->blk[kill_set_ptr->used].level = 0;
				kill_set_ptr->blk[kill_set_ptr->used++].block = blk2ptr->h[level2].blk_num;
			} else
			{
				/*
				new_blk2_ances_first_key =  new rtsib's ancestor's 1st key
				new_blk2_ances_first_keysz = new rtsib's ancestor's 1st key size
				blk2_ances_hdr = new rtsib's ancestor's 1st record's header
				*/
				delete_all_blk2_ances = FALSE;
				BLK_ADDR(new_blk2_ances_first_key, MAX_KEY_SZ + 1, unsigned char);
				rec_base =  blk2ptr->h[level2].buffaddr + SIZEOF(blk_hdr);
				READ_RECORD(status, &rec_size, &tkeycmpc, &tkeylen, new_blk2_ances_first_key,
						level2, blk2ptr->h[level2].buffaddr, rec_base);
				if (cdb_sc_normal != status)
				{
					assert(t_tries < CDB_STAGNATE);
					return cdb_sc_blkmod;
				}
				/* newblk1_last_key was the last key before *-key.
				   So new_blk2_ances_first_key will become real newblk1_last_key. */
				GET_CMPC(newblk1_last_cmpc, newblk1_last_key, new_blk2_ances_first_key);
				newblk1_last_keysz = tkeylen + tkeycmpc;
				newblk1_last_keylen = newblk1_last_keysz - newblk1_last_cmpc;
				memcpy(newblk1_last_key, new_blk2_ances_first_key, newblk1_last_keysz);
				/* 2nd record will become 1st record of current block at level2 */
				rec_base += rec_size;
				READ_RECORD(status, &rec_size, &tkeycmpc, &tkeylen, new_blk2_ances_first_key,
						level2, blk2ptr->h[level2].buffaddr, rec_base);
				blk2_ances_remain = rec_base + rec_size - SIZEOF(block_id);
				if (cdb_sc_starrecord == status)
					blk2_ances_star_only = TRUE;
				else if (cdb_sc_normal != status)
				{
					assert(t_tries < CDB_STAGNATE);
					return cdb_sc_blkmod;
				} else
				{
					new_blk2_ances_first_keysz = tkeylen + tkeycmpc;
					BLK_ADDR(blk2_ances_hdr, SIZEOF(rec_hdr), rec_hdr); /* new 1st record's header */
					blk2_ances_hdr->rsiz = new_blk2_ances_first_keysz + BSTAR_REC_SIZE;
					SET_CMPC(blk2_ances_hdr, 0);
				}
				break;
			}
		} /* end for level2 */
	} /* end if/else complete_merge */

	/*
	new_levelp_cur_hdr = new ancestor level curr_key header
	new_levelp_cur_keylen =  new ancestor level curr_key length
	new_levelp_cur_cmpc = new ancestor level curr_key compression count
	*/
	if (!complete_merge || !delete_all_blk2_ances) /* old_levelp_cur_key will be
							replaced by newblk1_last_key */
	{
		if (old_levelp_cur_prev_keysz == 0)	/* If a previous record doesn't exist */
		{
			new_levelp_cur_cmpc = 0;
			new_levelp_cur_keylen = newblk1_last_keysz;
		} else	/* If the previous record exists */
		{
			GET_CMPC(new_levelp_cur_cmpc, old_levelp_cur_prev_key, newblk1_last_key);
			new_levelp_cur_keylen = newblk1_last_keysz - new_levelp_cur_cmpc;
		}
		/*
		forming a new record header for the current record
		in the upper index block
		*/
		BLK_ADDR(new_levelp_cur_hdr, SIZEOF(rec_hdr), rec_hdr);
		new_levelp_cur_hdr->rsiz = BSTAR_REC_SIZE + new_levelp_cur_keylen;
		SET_CMPC(new_levelp_cur_hdr, new_levelp_cur_cmpc);
	}
	/* else  old_levelp_cur_key will be deleted */

	/*
	new_levelp_cur_next_hdr = new ancestor level curr_key's next key's header
	new_levelp_cur_next_keylen =  new ancestor level curr_key's next key's length
	new_levelp_cur_next_cmpc = new ancestor level curr_key's next key's compression count
	*/
	if (!levelp_next_is_star) /* if next record is not a *-record after levelp currkey */
	{
		if (!complete_merge || !delete_all_blk2_ances) /* old_levelp_cur_key will be
			replaced by newblk1_last_key and followed by old_levelp_cur_next_key */
		{
			GET_CMPC(new_levelp_cur_next_cmpc, newblk1_last_key, old_levelp_cur_next_key);
			new_levelp_cur_next_keylen = old_levelp_cur_next_keysz - new_levelp_cur_next_cmpc;
			if (((blk_hdr_ptr_t)old_levelp_blk_base)->bsiz
				- old_levelp_cur_keylen + new_levelp_cur_keylen
				- old_levelp_cur_next_keylen + new_levelp_cur_next_keylen > i_max_fill)
				return cdb_sc_oprnotneeded;
		} else /*	old_levelp_cur_key will be deleted */
		{
			if (old_levelp_cur_prev_keysz == 0) /* If the previous record deos not exist */
			{
				new_levelp_cur_next_cmpc = 0;
				new_levelp_cur_next_keylen = old_levelp_cur_next_keysz;
			} else    /* If the previous record exists */
			{
				GET_CMPC(new_levelp_cur_next_cmpc, old_levelp_cur_prev_key, old_levelp_cur_next_key);
				new_levelp_cur_next_keylen = old_levelp_cur_next_keysz - new_levelp_cur_next_cmpc;
			}
		}
		/*
		forming a new record header for the next record of current record
		in the upper index block
		*/
		BLK_ADDR(new_levelp_cur_next_hdr, SIZEOF(rec_hdr), rec_hdr);
		new_levelp_cur_next_hdr->rsiz = BSTAR_REC_SIZE + new_levelp_cur_next_keylen;
		SET_CMPC(new_levelp_cur_next_hdr, new_levelp_cur_next_cmpc);
	} else
	{
		if (!complete_merge || !delete_all_blk2_ances)
		{
			if (((blk_hdr_ptr_t)old_levelp_blk_base)->bsiz
				- old_levelp_cur_keylen + new_levelp_cur_keylen > i_max_fill)
				return cdb_sc_oprnotneeded;
		}
	}

	BLK_ADDR(star_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
	star_rec_hdr->rsiz = BSTAR_REC_SIZE;
	SET_CMPC(star_rec_hdr, 0);
	/* ------------------------
	 * Working block's t_write
	 * ------------------------
	 */
	BLK_INIT(bs_ptr2, bs_ptr1);
	if (0 == level)
	{	/* if a data block */
		/* adjust old block */
		BLK_SEG(bs_ptr2, old_blk1_base + SIZEOF(blk_hdr), old_blk1_sz - SIZEOF(blk_hdr) );
		/* Join data from right sibling */
		BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1, SIZEOF(rec_hdr));
		REORG_BLK_SEG(bs_ptr2, old_blk2_base + SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + newblk1_mid_cmpc,
				piece_len - SIZEOF(rec_hdr));
	} else
	{	/* if an index block */
		BLK_ADDR(bn_ptr1, SIZEOF(block_id), unsigned char);
		memcpy(bn_ptr1, old_blk1_base + old_blk1_sz - SIZEOF(block_id), SIZEOF(block_id));
		/* Keep whatever was there in working block */
		BLK_SEG(bs_ptr2,  old_blk1_base + SIZEOF(blk_hdr), old_blk1_sz - SIZEOF(blk_hdr) - BSTAR_REC_SIZE);
		/* last record was a *-key for index block, so replace with its real value */
		BLK_SEG(bs_ptr2, (sm_uc_ptr_t)old_last_rec_hdr1, SIZEOF(rec_hdr) );
		BLK_SEG(bs_ptr2, oldblk1_last_key + oldblk1_last_cmpc, oldblk1_last_keylen);
		BLK_SEG(bs_ptr2, bn_ptr1, SIZEOF(block_id) );
		/* Now join data from right sibling */
		if (star_only_merge)
		{	/* May be a complete_merge too */
			/* write a new *-rec only */
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr) );
			REORG_BLK_SEG(bs_ptr2, new_blk1_top - SIZEOF(block_id), SIZEOF(block_id));
		} else
		{
			if (complete_merge)
			{	/* First key from rtsib had cmpc=0. After coalesce it will be nonzero.
				 * Remainings from rtsib will be appened without change.
				 */
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1, SIZEOF(rec_hdr));
				REORG_BLK_SEG(bs_ptr2, old_blk2_base + SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + newblk1_mid_cmpc,
					piece_len - SIZEOF(rec_hdr) );
			} else
			{	/* First key from rtsib had cmpc=0. After coalesce it will be nonzero.
				 * Remainings from rtsib will be appened without change.
				 * However last record will be written as a *-key record
				 * (newblk1_last_keylen + BSTAR_REC_SIZE) = old length of the last record appended
				 */
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr1, SIZEOF(rec_hdr));
				REORG_BLK_SEG(bs_ptr2, old_blk2_base + SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + newblk1_mid_cmpc,
					piece_len - (newblk1_last_keylen + BSTAR_REC_SIZE)- SIZEOF(rec_hdr) );
				/* write a new *-rec only */
				BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr) );
				REORG_BLK_SEG(bs_ptr2, new_blk1_top - SIZEOF(block_id), SIZEOF(block_id));
			}
		}
	}
	if ( !BLK_FINI(bs_ptr2, bs_ptr1))
	{
		assert(t_tries < CDB_STAGNATE);
		return cdb_sc_blkmod;
	}
	t_write(&blk1ptr->h[level], (unsigned char *)bs_ptr1, 0, 0, level, FALSE, TRUE, GDS_WRITE_KILLTN);
	/* -----------------
	 * The right sibling
	 * -----------------
	 */
	if (!complete_merge)
	{
		BLK_INIT(bs_ptr2, bs_ptr1);
		if (!new_rtsib_star_only) /* more than one record in rtsib */
		{
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_rec_hdr2, SIZEOF(rec_hdr));
			BLK_SEG(bs_ptr2, newblk2_first_key, newblk2_first_keysz);
			BLK_SEG(bs_ptr2, new_blk2_remain, old_blk2_base + old_blk2_sz - new_blk2_remain );
		} else  /* only a *-key will remain in rtsib after coalesce is done */
		{
			/* write a new *-rec only */
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr) );
			BLK_SEG(bs_ptr2, old_blk2_base + old_blk2_sz - SIZEOF(block_id), SIZEOF(block_id));
		}
		if (!BLK_FINI(bs_ptr2,bs_ptr1))
		{
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_blkmod;
		}
		t_write(&blk2ptr->h[level], (unsigned char *)bs_ptr1, 0, 0, level, TRUE, TRUE, GDS_WRITE_KILLTN);
	} else
	{
		kill_set_ptr->blk[kill_set_ptr->used].flag = 0;
		kill_set_ptr->blk[kill_set_ptr->used].level = 0;
		kill_set_ptr->blk[kill_set_ptr->used++].block = blk2ptr->h[level].blk_num;
	}

	/* --------------------------
	 * ancestor of working block
	 * --------------------------
	 * bn_ptr2 = child of levelp ancestor block of currkey
	 */
	BLK_ADDR(bn_ptr2, SIZEOF(block_id), unsigned char);
	memcpy(bn_ptr2, old_levelp_blk_base + old_levelp_rec_offset + SIZEOF(rec_hdr) + old_levelp_cur_keylen, SIZEOF(block_id));

	BLK_INIT(bs_ptr2, bs_ptr1);
	/* data up to cur_rec */
	BLK_SEG(bs_ptr2, old_levelp_blk_base + SIZEOF(blk_hdr), old_levelp_rec_offset - SIZEOF(blk_hdr) );
	if (!levelp_next_is_star) /* if next record is not a *-record at levelp currkey */
	{
		if (complete_merge && delete_all_blk2_ances)
		{ /* old_levelp_curr_key will be removed and old_levelp_cur_next_key will be inserted there */
			assert (t_tries < CDB_STAGNATE || 0 != new_levelp_cur_next_keylen);
			assert (t_tries < CDB_STAGNATE || (old_levelp_rec_offset - SIZEOF(blk_hdr)) || !new_levelp_cur_next_cmpc);
			/* new header of cur_next instead of cur_rec */
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_levelp_cur_next_hdr, SIZEOF(rec_hdr));
			/* new key value of curr_next_key */
			BLK_SEG(bs_ptr2, old_levelp_cur_next_key + new_levelp_cur_next_cmpc, new_levelp_cur_next_keylen);
			/* new child is the working block (= descendent of levelp ancestor of currkey) */
			BLK_SEG(bs_ptr2, bn_ptr2, SIZEOF(block_id) );
			/* remaining records after levelp cur_next */
			BLK_SEG(bs_ptr2, old_levelp_blk_base + old_levelp_rec_offset +
				old_levelp_cur_keylen + BSTAR_REC_SIZE + old_levelp_cur_next_keylen + BSTAR_REC_SIZE,
				((blk_hdr_ptr_t)old_levelp_blk_base)->bsiz - old_levelp_rec_offset -
				(old_levelp_cur_keylen + BSTAR_REC_SIZE + old_levelp_cur_next_keylen + BSTAR_REC_SIZE));
			forward_process = TRUE;
		} else
		{ /* old_levelp_curr_key will be replaced by newblk1_last_key and old_levelp_cur_next_key will be inserted there */
			assert (t_tries < CDB_STAGNATE || 0 != new_levelp_cur_keylen);
			assert (t_tries < CDB_STAGNATE || 0 != old_levelp_rec_offset - SIZEOF(blk_hdr) || 0 == new_levelp_cur_cmpc);
			/* new header for new cur_rec of levelp */
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_levelp_cur_hdr, SIZEOF(rec_hdr) );
			/* new key value for cur_rec of levelp  */
			BLK_SEG(bs_ptr2, newblk1_last_key + new_levelp_cur_cmpc, new_levelp_cur_keylen);
			/* new child is old child */
			BLK_SEG(bs_ptr2, bn_ptr2, SIZEOF(block_id) );
			/* new header for next record after cur_rec of levelp */
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_levelp_cur_next_hdr, SIZEOF(rec_hdr) );
			/* new key value for cur_next of levelp  */
			BLK_SEG(bs_ptr2, old_levelp_cur_next_key + new_levelp_cur_next_cmpc, new_levelp_cur_next_keylen);
			/* copy old contents after old_levelp_cur_key */
			BLK_SEG(bs_ptr2, old_levelp_blk_base + old_levelp_rec_offset +
				BSTAR_REC_SIZE + old_levelp_cur_keylen + SIZEOF(rec_hdr) + old_levelp_cur_next_keylen,
				((blk_hdr_ptr_t)old_levelp_blk_base)->bsiz - old_levelp_rec_offset -
				(BSTAR_REC_SIZE + old_levelp_cur_keylen + SIZEOF(rec_hdr) + old_levelp_cur_next_keylen));
			forward_process = FALSE;
		}
	} else /* there is *-rec after old_levelp_cur_key */
	{
		if (complete_merge && delete_all_blk2_ances)
		{	/* delete old old_levelp_cur_key and *-key and write new *-key */
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr));
			BLK_SEG(bs_ptr2, bn_ptr2, SIZEOF(block_id) );
			forward_process = TRUE;
		} else
		{	/* new header for new cur_rec of levelp */
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)new_levelp_cur_hdr, SIZEOF(rec_hdr) );
			/* new key value for cur_rec of levelp  */
			BLK_SEG(bs_ptr2, newblk1_last_key + new_levelp_cur_cmpc, new_levelp_cur_keylen);
			/* new child is old child */
			BLK_SEG(bs_ptr2, bn_ptr2, SIZEOF(block_id) );
			/* old *-rec */
			BLK_SEG(bs_ptr2, old_levelp_blk_base + ((blk_hdr_ptr_t)old_levelp_blk_base)->bsiz - BSTAR_REC_SIZE,
				BSTAR_REC_SIZE);
			forward_process = FALSE;
		}
	}
	if (!BLK_FINI(bs_ptr2, bs_ptr1))
	{
		assert(t_tries < CDB_STAGNATE);
		return cdb_sc_blkmod;
	}
	t_write(&blk1ptr->h[levelp], (unsigned char *)bs_ptr1, 0, 0, levelp, FALSE, forward_process, GDS_WRITE_KILLTN);
	/* ---------------------------------------------------------------------------
	 * if delete_all_blk2_ances and level+1 <= level2 < levelp,
	 * 	if blk2ptr->h[level2].blk_num are *-record blocks
	 * 		we already deleted them
	 * 	else
	 * 		update  blk2ptr->h[level2].blk_num for first level2>level+1
	 * Note: delete_all_blk2_ances == FALSE => complete_merge = TRUE;
	 */
	if (!delete_all_blk2_ances)
	{
		BLK_INIT(bs_ptr2, bs_ptr1);
		if (blk2_ances_star_only)
		{
			BLK_SEG(bs_ptr2, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr));
		} else
		{
			BLK_SEG(bs_ptr2,  (sm_uc_ptr_t)blk2_ances_hdr, SIZEOF(rec_hdr));
			BLK_SEG(bs_ptr2, new_blk2_ances_first_key,  new_blk2_ances_first_keysz);
			assert (t_tries < CDB_STAGNATE || 0 != new_blk2_ances_first_keysz);
		}
		BLK_SEG(bs_ptr2, blk2_ances_remain,  blk2ptr->h[level2].buffaddr +
			((blk_hdr_ptr_t)blk2ptr->h[level2].buffaddr)->bsiz - blk2_ances_remain);
		if (!BLK_FINI(bs_ptr2,bs_ptr1))
		{
			assert(t_tries < CDB_STAGNATE);
			return cdb_sc_blkmod;
		}
		t_write(&blk2ptr->h[level2], (unsigned char *)bs_ptr1, 0, 0, level2, TRUE, TRUE, GDS_WRITE_KILLTN);
	}
	/* else do not need to change blk2ptr->h[level2].blk_num.
	 * Because, for level+1 == levelp this is the same as levelp ancestor block (blk1ptr->h[levelp].blk_num).
	 * For levelp > level + 1, and a complete_merge, levelp ancestor block will take care of the case.
	 * For levelp > level + 1, and not complete_merge,  old blk2ptr->h[level2].blk_num records are still valid.
	 * 	(only leftmost records from the collation sequence are moved to the working block
	 * 	and still blk2ptr->h[level+1] correctly points to the
	 * 	right most value of collation sequence at correct block.)
	 */
	*remove_rtsib = complete_merge;
	/* prepare next gv_currkey for reorg */
	if (0 == level && !complete_merge)
	{
		memcpy(&gv_currkey_next_reorg->base[0], newblk2_first_key, newblk2_first_keysz);
		gv_currkey_next_reorg->end = newblk2_first_keysz - 1;
	}
	return cdb_sc_normal;
} /* end of the program */
