/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cdb_sc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "copy.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "spec_type.h"

/* Include prototypes */
#include "t_write.h"
#include "t_create.h"
#include "gvcst_delete_blk.h"
#include "gvcst_kill_blk.h"

GBLREF boolean_t	horiz_growth;
GBLREF char		*update_array, *update_array_ptr;
GBLREF gv_namehead	*gv_target;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF uint4		update_array_size;	/* for the BLK_* macros */
GBLREF uint4		dollar_tlevel;
GBLREF unsigned int	t_tries;

/* delete all records greater than low and less than high in blkhist->blk_num */

enum cdb_sc	gvcst_kill_blk(srch_blk_status	*blkhist,
				char		level,
				gv_key		*search_key,
				srch_rec_status	low,
				srch_rec_status	high,
				boolean_t	right_extra,
				cw_set_element	**cseptr)
{
	blk_hdr_ptr_t			old_blk_hdr;
	blk_segment			*bs1, *bs_ptr;
	block_id			blk, temp_blk;
	block_index			new_block_index;
	bool				kill_root, first_copy;
	cw_set_element			*cse, *old_cse;
	int4				blk_size, blk_seg_cnt, lmatch, rmatch, targ_len, prev_len, targ_base, next_rec_shrink,
					temp_int, blkseglen, tmp_cmpc;
	off_chain			chain1, curr_chain, prev_chain;
	v6_off_chain			v6_chain;
	rec_hdr_ptr_t			left_ptr;	/*pointer to record before first record to delete*/
	rec_hdr_ptr_t			del_ptr;	/*pointer to first record to delete*/
	rec_hdr_ptr_t			right_ptr;	/*pointer to record after last record to delete*/
	rec_hdr_ptr_t			right_prev_ptr;
	rec_hdr_ptr_t			rp, rp1;	/*scratch record pointer*/
	rec_hdr_ptr_t			first_in_blk, top_of_block, new_rec_hdr, star_rec_hdr;
	sm_uc_ptr_t			buffer, curr, prev, right_bytptr;
	srch_blk_status			*t1;
	unsigned char			*skb;
	unsigned short			temp_ushort;
	boolean_t			long_blk_id;
	int4				blk_id_sz, off_chain_sz;

	*cseptr = NULL;
	if (low.offset == high.offset)
		return cdb_sc_normal;
	blk = blkhist->blk_num;
	if (dollar_tlevel)
	{
		PUT_BLK_ID(&chain1, blk);
		assert((SIZEOF(int) * 8) >= CW_INDEX_MAX_BITS);
		if ((1 == chain1.flag) && ((int)chain1.cw_index >= sgm_info_ptr->cw_set_depth))
		{
			assert(sgm_info_ptr->tp_csa == cs_addrs);
			assert(FALSE == cs_addrs->now_crit);
			return cdb_sc_blknumerr;
		}
	}
	buffer = blkhist->buffaddr;
	old_blk_hdr = (blk_hdr_ptr_t)buffer;
	kill_root = FALSE;
	blk_size = cs_data->blk_size;
	first_in_blk = (rec_hdr_ptr_t)((sm_uc_ptr_t)old_blk_hdr + SIZEOF(blk_hdr));
	top_of_block = (rec_hdr_ptr_t)((sm_uc_ptr_t)old_blk_hdr + old_blk_hdr->bsiz);
	left_ptr = (rec_hdr_ptr_t)((sm_uc_ptr_t)old_blk_hdr + low.offset);
	right_ptr = (rec_hdr_ptr_t)((sm_uc_ptr_t)old_blk_hdr + high.offset);
	long_blk_id = IS_64_BLK_ID(buffer);
	blk_id_sz = SIZEOF_BLK_ID(long_blk_id);
	off_chain_sz = long_blk_id ? SIZEOF(off_chain) : SIZEOF(v6_off_chain);
	assert(blk_id_sz == off_chain_sz); /* block_id and off_chain should be the same size */
	if (right_extra && right_ptr < top_of_block)
	{
		right_prev_ptr = right_ptr;
		GET_USHORT(temp_ushort, &right_ptr->rsiz);
		right_ptr = (rec_hdr_ptr_t)((sm_uc_ptr_t)right_ptr + temp_ushort);
	}
	if ((sm_uc_ptr_t)left_ptr < (sm_uc_ptr_t)old_blk_hdr ||
		(sm_uc_ptr_t)right_ptr > (sm_uc_ptr_t)top_of_block ||
		(sm_uc_ptr_t)left_ptr >= (sm_uc_ptr_t)right_ptr)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_rmisalign;
	}
	if ((sm_uc_ptr_t)left_ptr == (sm_uc_ptr_t)old_blk_hdr)
	{
		if ((sm_uc_ptr_t)right_ptr == (sm_uc_ptr_t)top_of_block)
		{
			if ((sm_uc_ptr_t)first_in_blk == (sm_uc_ptr_t)top_of_block)
			{
				if (0 != level)
				{
					assert(CDB_STAGNATE > t_tries);
					return cdb_sc_rmisalign;
				}
				return cdb_sc_normal;
			}
			if (!gv_target->hist.h[level + 1].blk_num)
				kill_root = TRUE;
			else
			{	/* We are about to free up the contents of this entire block. If this block corresponded to
				 * a global that has NOISOLATION turned on and has a non-zero recompute list (i.e. some SETs
				 * already happened in this same TP transaction), make sure we disable the NOISOLATION
				 * optimization in this case as that is applicable only if one or more SETs happened in this
				 * data block and NOT if a KILL happens. Usually this is done by a t_write(GDS_WRITE_KILLTN)
				 * call but since in this case the entire block is being freed, "t_write" wont be invoked
				 * so we need to explicitly set GDS_WRITE_KILLTN like t_write would have (GTM-8269).
				 * Note: blkhist->first_tp_srch_status is not reliable outside of TP. Thankfully the recompute
				 * list is also maintained only in case of TP so a check of dollar_tlevel is enough to
				 * dereference both "first_tp_srch_status" and "recompute_list_head".
				 */
				if (dollar_tlevel)
				{
					t1 = blkhist->first_tp_srch_status ? blkhist->first_tp_srch_status : blkhist;
					cse = t1->cse;
					if ((NULL != cse) && cse->recompute_list_head)
						cse->write_type |= GDS_WRITE_KILLTN;
				}
				return cdb_sc_delete_parent;
			}
		}
		del_ptr = first_in_blk;
	} else
	{
		GET_USHORT(temp_ushort, &left_ptr->rsiz);
		del_ptr = (rec_hdr_ptr_t)((sm_uc_ptr_t)left_ptr + temp_ushort);
		if ((sm_uc_ptr_t)del_ptr <= (sm_uc_ptr_t)(left_ptr + 1)  ||  (sm_uc_ptr_t)del_ptr > (sm_uc_ptr_t)right_ptr)
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_rmisalign;
		}
	}
	if ((sm_uc_ptr_t)del_ptr == (sm_uc_ptr_t)right_ptr)
		return cdb_sc_normal;
	lmatch = low.match;
	rmatch = high.match;
	if (level)
	{
		for (rp = del_ptr ;  rp < right_ptr ;  rp = rp1)
		{
			GET_USHORT(temp_ushort, &rp->rsiz);
			rp1 = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + temp_ushort);
			if (((sm_uc_ptr_t)rp1 < (sm_uc_ptr_t)(rp + 1) + blk_id_sz) ||
				((sm_uc_ptr_t)rp1 < buffer) || ((sm_uc_ptr_t)rp1 > (buffer + blk_size)))
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_rmisalign;
			}
			READ_BLK_ID(long_blk_id, &temp_blk, (sm_uc_ptr_t)rp1 - blk_id_sz);
			if (dollar_tlevel)
			{
				chain1 = *(off_chain *)&temp_blk;
				assert((SIZEOF(int) * 8) >= CW_INDEX_MAX_BITS);
				if ((1 == chain1.flag) && ((int)chain1.cw_index >= sgm_info_ptr->cw_set_depth))
				{
					assert(sgm_info_ptr->tp_csa == cs_addrs);
					assert(FALSE == cs_addrs->now_crit);
					return cdb_sc_blknumerr;
				}
			}
			gvcst_delete_blk(temp_blk, level - 1, FALSE);
		}
	}
	if (kill_root)
	{	/* create an empty data block */
		BLK_INIT(bs_ptr, bs1);
		if (!BLK_FINI(bs_ptr, bs1))
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_mkblk;
		}
		new_block_index = t_create(blk, (uchar_ptr_t)bs1, 0, 0, 0);
		/* create index block */
		BLK_ADDR(new_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
		new_rec_hdr->rsiz = bstar_rec_size(long_blk_id);
		SET_CMPC(new_rec_hdr, 0);
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, (sm_uc_ptr_t)new_rec_hdr, SIZEOF(rec_hdr));
		BLK_SEG_ZERO(bs_ptr, blk_id_sz);
		if (!BLK_FINI(bs_ptr, bs1))
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_mkblk;
		}
		cse = t_write(blkhist, (unsigned char *)bs1, SIZEOF(blk_hdr) + SIZEOF(rec_hdr), new_block_index, 1,
			TRUE, FALSE, GDS_WRITE_KILLTN);
		assert(!dollar_tlevel || !cse->high_tlevel);
		if (dollar_tlevel)
		{
			assert(cse);
			*cseptr = cse;
			cse->first_off = 0;
		}
		return cdb_sc_normal;
	}
	next_rec_shrink = (int)(old_blk_hdr->bsiz + ((sm_uc_ptr_t)del_ptr - (sm_uc_ptr_t)right_ptr));
	if (SIZEOF(blk_hdr) >= next_rec_shrink)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_rmisalign;
	}
	if ((sm_uc_ptr_t)right_ptr == (sm_uc_ptr_t)top_of_block)
	{
		if (level)
		{
			GET_USHORT(temp_ushort, &left_ptr->rsiz);
			next_rec_shrink += SIZEOF(rec_hdr) + blk_id_sz - temp_ushort;
		}
	} else
	{
		targ_base = (rmatch < lmatch) ? rmatch : lmatch;
		prev_len = 0;
		if (right_extra)
		{
			EVAL_CMPC2(right_prev_ptr, tmp_cmpc);
			targ_len = tmp_cmpc - targ_base;
			if (targ_len < 0)
				targ_len = 0;
			temp_int = tmp_cmpc - EVAL_CMPC(right_ptr);
			if (0 >= temp_int)
				prev_len = - temp_int;
			else
			{
				if (temp_int < targ_len)
					targ_len -= temp_int;
				else
					targ_len = 0;
			}
		} else
		{
			targ_len = EVAL_CMPC(right_ptr) - targ_base;
			if (targ_len < 0)
				targ_len = 0;
		}
		next_rec_shrink += targ_len + prev_len;
	}
	BLK_INIT(bs_ptr, bs1);
	first_copy = TRUE;
	blkseglen = (int)((sm_uc_ptr_t)del_ptr - (sm_uc_ptr_t)first_in_blk);
	if (0 < blkseglen)
	{
		if (((sm_uc_ptr_t)right_ptr != (sm_uc_ptr_t)top_of_block)  ||  (0 == level))
		{
			BLK_SEG(bs_ptr, (sm_uc_ptr_t)first_in_blk, blkseglen);
			first_copy = FALSE;
		} else
		{
			blkseglen = (int)((sm_uc_ptr_t)left_ptr - (sm_uc_ptr_t)first_in_blk);
			if (0 < blkseglen)
			{
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)first_in_blk, blkseglen);
				first_copy = FALSE;
			}
			BLK_ADDR(star_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
			SET_CMPC(star_rec_hdr, 0);
			star_rec_hdr->rsiz = (unsigned short)(bstar_rec_size(long_blk_id));
			BLK_SEG(bs_ptr, (sm_uc_ptr_t)star_rec_hdr, SIZEOF(rec_hdr));
			GET_USHORT(temp_ushort, &left_ptr->rsiz);
			BLK_SEG(bs_ptr, ((sm_uc_ptr_t)left_ptr + temp_ushort - blk_id_sz), blk_id_sz);
		}
	}
	blkseglen = (int)((sm_uc_ptr_t)top_of_block - (sm_uc_ptr_t)right_ptr);
	assert(0 <= blkseglen);
	if (0 != blkseglen)
	{
		next_rec_shrink = targ_len + prev_len;
		if (0 >= next_rec_shrink)
		{
			BLK_SEG(bs_ptr, (sm_uc_ptr_t)right_ptr, blkseglen);
		} else
		{
			BLK_ADDR(new_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
			SET_CMPC(new_rec_hdr, EVAL_CMPC(right_ptr) - next_rec_shrink);
			GET_USHORT(temp_ushort, &right_ptr->rsiz);
			new_rec_hdr->rsiz = temp_ushort + next_rec_shrink;
			BLK_SEG(bs_ptr, (sm_uc_ptr_t)new_rec_hdr, SIZEOF(rec_hdr));
			if (targ_len)
			{
				BLK_ADDR(skb, targ_len, unsigned char);
				memcpy(skb, &search_key->base[targ_base], targ_len);
				BLK_SEG(bs_ptr, skb, targ_len);
			}
			if (prev_len)
				BLK_SEG(bs_ptr, (sm_uc_ptr_t)(right_prev_ptr + 1) , prev_len);
			right_bytptr = (sm_uc_ptr_t)(right_ptr + 1);
			blkseglen = (int)((sm_uc_ptr_t)top_of_block - right_bytptr);
			if (0 < blkseglen)
			{
				BLK_SEG(bs_ptr, right_bytptr, blkseglen);
			} else
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_rmisalign;
			}
		}
	}
	if (!BLK_FINI(bs_ptr, bs1))
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_mkblk;
	}
	cse = t_write(blkhist, (unsigned char *)bs1, 0, 0, level, first_copy, TRUE, GDS_WRITE_KILLTN);
	assert(!dollar_tlevel || !cse->high_tlevel);
	if (dollar_tlevel)
	{
		assert(cse);
		*cseptr = cse;
	}
	if (horiz_growth)
	{
		old_cse = cse->low_tlevel;
		assert(old_cse && old_cse->done);
		assert(2 == (SIZEOF(old_cse->undo_offset) / SIZEOF(old_cse->undo_offset[0])));
		assert(2 == (SIZEOF(old_cse->undo_next_off) / SIZEOF(old_cse->undo_next_off[0])));
		assert(!old_cse->undo_next_off[0] && !old_cse->undo_offset[0]);
		assert(!old_cse->undo_next_off[1] && !old_cse->undo_offset[1]);
	}
	if ((dollar_tlevel)  &&  (0 != cse->first_off))
	{	/* fix up chains in the block to account for deleted records */
		prev = NULL;
		curr = buffer + cse->first_off;
		READ_OFF_CHAIN(long_blk_id, &curr_chain, &v6_chain, curr);
		while (curr < (sm_uc_ptr_t)del_ptr)
		{	/* follow chain to first deleted record */
			if (0 == curr_chain.next_off)
				break;
			if ((right_ptr == top_of_block) && (((sm_uc_ptr_t)del_ptr - curr) == off_chain_sz))
				break;	/* special case described below: stop just before the first deleted record */
			prev = curr;
			curr += curr_chain.next_off;
			READ_OFF_CHAIN(long_blk_id, &curr_chain, &v6_chain, curr);
		}
		if ((right_ptr == top_of_block) && (((sm_uc_ptr_t)del_ptr - curr) == off_chain_sz))
		{
			/* if the right side of the block is gone and our last chain is in the last record,
			 * terminate the chain and adjust the previous entry to point at the new *-key
			 * NOTE: this assumes there's NEVER a TP delete of records in the GVT
			 */
			assert(0 != level);
			/* store next_off in old_cse before actually changing it in the buffer(for rolling back) */
			if (horiz_growth)
			{
				old_cse->undo_next_off[0] = curr_chain.next_off;
				old_cse->undo_offset[0] = (block_offset)(curr - buffer);
				assert(old_cse->undo_offset[0]);
			}
			curr_chain.next_off = 0;
			WRITE_OFF_CHAIN(long_blk_id, &curr_chain, &v6_chain, curr);
			if (NULL != prev)
			{	/* adjust previous chain next_off to reflect that the record it refers to is now a *-key */
				READ_OFF_CHAIN(long_blk_id, &prev_chain, &v6_chain, prev);
				/* store next_off in old_cse before actually changing it in the buffer(for rolling back) */
				if (horiz_growth)
				{
					old_cse->undo_next_off[1] = prev_chain.next_off;
					old_cse->undo_offset[1] = (block_offset)(prev - buffer);
					assert(old_cse->undo_offset[1]);
				}
				/* next_off is of type gtm_uint8 but is constrained by a bit field to 16-bits so the following
				 * assert is to verify that there was no precision loss.
				 */
				assert((1ULL << NEXT_OFF_MAX_BITS) >
					((sm_uc_ptr_t)left_ptr - prev + (unsigned int)(SIZEOF(rec_hdr))));
				prev_chain.next_off = (uint4)((sm_uc_ptr_t)left_ptr - prev + (unsigned int)(SIZEOF(rec_hdr)));
				WRITE_OFF_CHAIN(long_blk_id, &prev_chain, &v6_chain, prev);
			} else	/* it's the first (and only) one */
				cse->first_off = (block_offset)((sm_uc_ptr_t)left_ptr - buffer + SIZEOF(rec_hdr));
		} else if (curr >= (sm_uc_ptr_t)del_ptr)
		{	/* may be more records on the right that aren't deleted */
			while (curr < (sm_uc_ptr_t)right_ptr)
			{	/* follow chain past last deleted record */
				if (0 == curr_chain.next_off)
					break;
				curr += curr_chain.next_off;
				READ_OFF_CHAIN(long_blk_id, &curr_chain, &v6_chain, curr);
			}
			/* prev :   ptr to chain record immediately preceding the deleted area,
			 *	    or 0 if none.
			 *
			 * curr :   ptr to chain record immediately following the deleted area,
			 *	    or to last chain record.
			 */
			if (curr < (sm_uc_ptr_t)right_ptr)
			{	/* the former end of the chain is going, going, gone */
				if (NULL != prev)
				{	/* terminate the chain before the delete */
					READ_OFF_CHAIN(long_blk_id, &prev_chain, &v6_chain, prev);
					/* store next_off in old_cse before actually changing it in the buffer(for rolling back) */
					if (horiz_growth)
					{
						old_cse->undo_next_off[0] = prev_chain.next_off;
						old_cse->undo_offset[0] = (block_offset)(prev - buffer);
						assert(old_cse->undo_offset[0]);
					}
					prev_chain.next_off = 0;
					WRITE_OFF_CHAIN(long_blk_id, &prev_chain, &v6_chain, prev);
				} else
					cse->first_off = 0;		/* the whole chain is gone */
			} else
			{	/* stitch up the left and right to account for the hole in the middle */
				/* next_rec_shrink is the change in record size due to the new compression count */
				if (NULL != prev)
				{
					READ_OFF_CHAIN(long_blk_id, &prev_chain, &v6_chain, prev);
					/* ??? new compression may be less (ie +) so why are negative shrinks ignored? */
					/* store next_off in old_cse before actually changing it in the buffer(for rolling back) */
					if (horiz_growth)
					{
						old_cse->undo_next_off[0] = prev_chain.next_off;
						old_cse->undo_offset[0] = (block_offset)(prev - buffer);
						assert(old_cse->undo_offset[0]);
					}
					/* next_off is of type gtm_uint8 but is constrained by a bit field to 16-bits
					 * so the following assert is to verify that there is no precision loss.
					 */
					assert((1ULL << NEXT_OFF_MAX_BITS) >
							(curr - prev
							- ((sm_uc_ptr_t)right_ptr - (sm_uc_ptr_t)del_ptr)
							+ (next_rec_shrink > 0 ? next_rec_shrink : 0)));
					prev_chain.next_off = (block_offset)(curr - prev -
							((sm_uc_ptr_t)right_ptr - (sm_uc_ptr_t)del_ptr)
							+ (next_rec_shrink > 0 ? next_rec_shrink : 0));
					WRITE_OFF_CHAIN(long_blk_id, &prev_chain, &v6_chain, prev);
				} else	/* curr remains first: adjust the head */
					cse->first_off = (block_offset)(curr - buffer -
							((sm_uc_ptr_t)right_ptr - (sm_uc_ptr_t)del_ptr)
							+ (next_rec_shrink > 0 ? next_rec_shrink : 0));
			}
		}
	}
	horiz_growth = FALSE;
	return cdb_sc_normal;
}
