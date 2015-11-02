/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"

/* Include prototypes */
#include "t_write.h"
#include "t_create.h"
#include "gvcst_delete_blk.h"
#include "gvcst_kill_blk.h"

GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF gv_namehead	*gv_target;
GBLREF char		*update_array, *update_array_ptr;
GBLREF uint4		update_array_size;	/* for the BLK_* macros */
GBLREF sgmnt_addrs      *cs_addrs;
GBLREF unsigned int	t_tries;
GBLREF uint4		dollar_tlevel;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF boolean_t	horiz_growth;

/* delete all records greater than low and less than high in blkhist->blk_num */

enum cdb_sc	gvcst_kill_blk(srch_blk_status	*blkhist,
			       char		level,
			       gv_key  		*search_key,
			       srch_rec_status	low,
			       srch_rec_status	high,
			       boolean_t	right_extra,
			       cw_set_element	**cseptr)
{
	typedef sm_uc_ptr_t		bytptr;

	unsigned short			temp_ushort;
	int4				temp_long;
	int				tmp_cmpc;
	int				blk_size, blk_seg_cnt, lmatch, rmatch, targ_len, prev_len, targ_base, next_rec_shrink,
					temp_int, blkseglen;
	bool				kill_root, first_copy;
	blk_hdr_ptr_t			old_blk_hdr;
	rec_hdr_ptr_t			left_ptr;	/*pointer to record before first record to delete*/
	rec_hdr_ptr_t			del_ptr;	/*pointer to first record to delete*/
	rec_hdr_ptr_t	       		right_ptr;	/*pointer to record after last record to delete*/
	rec_hdr_ptr_t			right_prev_ptr;
	rec_hdr_ptr_t			rp, rp1;	/*scratch record pointer*/
	rec_hdr_ptr_t			first_in_blk, top_of_block, new_rec_hdr, star_rec_hdr;
	blk_segment			*bs1, *bs_ptr;
	block_index			new_block_index;
	unsigned char			*skb;
	static readonly block_id	zeroes = 0;
	cw_set_element			*cse, *old_cse;
	bytptr				curr, prev, right_bytptr;
	off_chain			chain1, curr_chain, prev_chain;
	block_id			blk;
	sm_uc_ptr_t			buffer;

	*cseptr = NULL;
	if (low.offset == high.offset)
		return cdb_sc_normal;
	blk = blkhist->blk_num;
	if (dollar_tlevel)
	{
		PUT_LONG(&chain1, blk);
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
	first_in_blk = (rec_hdr_ptr_t)((bytptr)old_blk_hdr + SIZEOF(blk_hdr));
	top_of_block = (rec_hdr_ptr_t)((bytptr)old_blk_hdr + old_blk_hdr->bsiz);
	left_ptr = (rec_hdr_ptr_t)((bytptr)old_blk_hdr + low.offset);
	right_ptr = (rec_hdr_ptr_t)((bytptr)old_blk_hdr + high.offset);
	if (right_extra && right_ptr < top_of_block)
	{
		right_prev_ptr = right_ptr;
		GET_USHORT(temp_ushort, &right_ptr->rsiz);
		right_ptr = (rec_hdr_ptr_t)((bytptr)right_ptr + temp_ushort);
	}
	if ((bytptr)left_ptr < (bytptr)old_blk_hdr ||
		(bytptr)right_ptr > (bytptr)top_of_block ||
		(bytptr)left_ptr >= (bytptr)right_ptr)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_rmisalign;
	}
	if ((bytptr)left_ptr == (bytptr)old_blk_hdr)
	{
		if ((bytptr)right_ptr == (bytptr)top_of_block)
		{
			if ((bytptr)first_in_blk == (bytptr)top_of_block)
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
				return cdb_sc_delete_parent;
		}
		del_ptr = first_in_blk;
	} else
	{
		GET_USHORT(temp_ushort, &left_ptr->rsiz);
		del_ptr = (rec_hdr_ptr_t)((bytptr)left_ptr + temp_ushort);
		if ((bytptr)del_ptr <= (bytptr)(left_ptr + 1)  ||  (bytptr)del_ptr > (bytptr)right_ptr)
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_rmisalign;
		}
	}
	if ((bytptr)del_ptr == (bytptr)right_ptr)
		return cdb_sc_normal;
	lmatch = low.match;
	rmatch = high.match;
	if (level)
	{
		for (rp = del_ptr ;  rp < right_ptr ;  rp = rp1)
		{
			GET_USHORT(temp_ushort, &rp->rsiz);
			rp1 = (rec_hdr_ptr_t)((bytptr)rp + temp_ushort);
			if (((bytptr)rp1 < (bytptr)(rp + 1) + SIZEOF(block_id)) ||
				((bytptr)rp1 < buffer) || ((bytptr)rp1 > (buffer + blk_size)))
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_rmisalign;
			}
			GET_LONG(temp_long, ((bytptr)rp1 - SIZEOF(block_id)));
			if (dollar_tlevel)
			{
				chain1 = *(off_chain *)&temp_long;
				if ((1 == chain1.flag) && ((int)chain1.cw_index >= sgm_info_ptr->cw_set_depth))
				{
					assert(sgm_info_ptr->tp_csa == cs_addrs);
					assert(FALSE == cs_addrs->now_crit);
					return cdb_sc_blknumerr;
				}
			}
			gvcst_delete_blk(temp_long, level - 1, FALSE);
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
		new_rec_hdr->rsiz = SIZEOF(rec_hdr) + SIZEOF(block_id);
		SET_CMPC(new_rec_hdr, 0);
		BLK_INIT(bs_ptr, bs1);
		BLK_SEG(bs_ptr, (bytptr)new_rec_hdr, SIZEOF(rec_hdr));
		BLK_SEG(bs_ptr, (bytptr)&zeroes, SIZEOF(block_id));
		if (!BLK_FINI(bs_ptr, bs1))
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_mkblk;
		}
		cse = t_write(blkhist, (unsigned char *)bs1, SIZEOF(blk_hdr) + SIZEOF(rec_hdr), new_block_index, 1,
			TRUE, FALSE, GDS_WRITE_KILLTN);
		assert(!dollar_tlevel || !cse->high_tlevel);
		*cseptr = cse;
		if (NULL != cse)
			cse->first_off = 0;
		return cdb_sc_normal;
	}
	next_rec_shrink = (int)(old_blk_hdr->bsiz + ((bytptr)del_ptr - (bytptr)right_ptr));
	if (SIZEOF(blk_hdr) >= next_rec_shrink)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_rmisalign;
	}
	if ((bytptr)right_ptr == (bytptr)top_of_block)
	{
		if (level)
		{
			GET_USHORT(temp_ushort, &left_ptr->rsiz);
			next_rec_shrink += SIZEOF(rec_hdr) + SIZEOF(block_id) - temp_ushort;
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
	blkseglen = (int)((bytptr)del_ptr - (bytptr)first_in_blk);
	if (0 < blkseglen)
	{
		if (((bytptr)right_ptr != (bytptr)top_of_block)  ||  (0 == level))
		{
			BLK_SEG(bs_ptr, (bytptr)first_in_blk, blkseglen);
			first_copy = FALSE;
		} else
		{
			blkseglen = (int)((bytptr)left_ptr - (bytptr)first_in_blk);
			if (0 < blkseglen)
			{
				BLK_SEG(bs_ptr, (bytptr)first_in_blk, blkseglen);
				first_copy = FALSE;
			}
			BLK_ADDR(star_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
			SET_CMPC(star_rec_hdr, 0);
			star_rec_hdr->rsiz = (unsigned short)(SIZEOF(rec_hdr) + SIZEOF(block_id));
			BLK_SEG(bs_ptr, (bytptr)star_rec_hdr, SIZEOF(rec_hdr));
			GET_USHORT(temp_ushort, &left_ptr->rsiz);
			BLK_SEG(bs_ptr, ((bytptr)left_ptr + temp_ushort - SIZEOF(block_id)), SIZEOF(block_id));
		}
	}
	blkseglen = (int)((bytptr)top_of_block - (bytptr)right_ptr);
	assert(0 <= blkseglen);
	if (0 != blkseglen)
	{
		next_rec_shrink = targ_len + prev_len;
		if (0 >= next_rec_shrink)
		{
			BLK_SEG(bs_ptr, (bytptr)right_ptr, blkseglen);
		} else
		{
			BLK_ADDR(new_rec_hdr, SIZEOF(rec_hdr), rec_hdr);
			SET_CMPC(new_rec_hdr, EVAL_CMPC(right_ptr) - next_rec_shrink);
			GET_USHORT(temp_ushort, &right_ptr->rsiz);
			new_rec_hdr->rsiz = temp_ushort + next_rec_shrink;
			BLK_SEG(bs_ptr, (bytptr)new_rec_hdr, SIZEOF(rec_hdr));
			if (targ_len)
			{
				BLK_ADDR(skb, targ_len, unsigned char);
				memcpy(skb, &search_key->base[targ_base], targ_len);
				BLK_SEG(bs_ptr, skb, targ_len);
			}
			if (prev_len)
				BLK_SEG(bs_ptr, (bytptr)(right_prev_ptr + 1) , prev_len);
			right_bytptr = (bytptr)(right_ptr + 1);
			blkseglen = (int)((bytptr)top_of_block - right_bytptr);
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
	*cseptr = cse;
	if (horiz_growth)
	{
		old_cse = cse->low_tlevel;
		assert(old_cse && old_cse->done);
		assert(2 == (SIZEOF(old_cse->undo_offset) / SIZEOF(old_cse->undo_offset[0])));
		assert(2 == (SIZEOF(old_cse->undo_next_off) / SIZEOF(old_cse->undo_next_off[0])));
		assert(!old_cse->undo_next_off[0] && !old_cse->undo_offset[0]);
		assert(!old_cse->undo_next_off[1] && !old_cse->undo_offset[1]);
	}
        if ((NULL != cse)  &&  (0 != cse->first_off))
	{	/* fix up chains in the block to account for deleted records */
		prev = NULL;
		curr = buffer + cse->first_off;
		GET_LONGP(&curr_chain, curr);
		while (curr < (bytptr)del_ptr)
		{	/* follow chain to first deleted record */
			if (0 == curr_chain.next_off)
				break;
			if (right_ptr == top_of_block  &&  (bytptr)del_ptr - curr == SIZEOF(off_chain))
				break;	/* special case described below: stop just before the first deleted record */
			prev = curr;
			curr += curr_chain.next_off;
			GET_LONGP(&curr_chain, curr);
		}
		if (right_ptr == top_of_block  &&  (bytptr)del_ptr - curr == SIZEOF(off_chain))
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
			GET_LONGP(curr, &curr_chain);
			if (NULL != prev)
			{	/* adjust previous chain next_off to reflect the fact that the record it refers to is now a *-key */
				GET_LONGP(&prev_chain, prev);
				/* store next_off in old_cse before actually changing it in the buffer(for rolling back) */
				if (horiz_growth)
				{
					old_cse->undo_next_off[1] = prev_chain.next_off;
					old_cse->undo_offset[1] = (block_offset)(prev - buffer);
					assert(old_cse->undo_offset[1]);
				}
				prev_chain.next_off = (unsigned int)((bytptr)left_ptr - prev + (unsigned int)(SIZEOF(rec_hdr)));
				GET_LONGP(prev, &prev_chain);
			} else	/* it's the first (and only) one */
				cse->first_off = (block_offset)((bytptr)left_ptr - buffer + SIZEOF(rec_hdr));
		} else if (curr >= (bytptr)del_ptr)
		{	/* may be more records on the right that aren't deleted */
			while (curr < (bytptr)right_ptr)
			{	/* follow chain past last deleted record */
				if (0 == curr_chain.next_off)
					break;
				curr += curr_chain.next_off;
				GET_LONGP(&curr_chain, curr);
			}
			/* prev :   ptr to chain record immediately preceding the deleted area,
			 *	    or 0 if none.
			 *
			 * curr :   ptr to chain record immediately following the deleted area,
			 *	    or to last chain record.
			 */
			if (curr < (bytptr)right_ptr)
			{	/* the former end of the chain is going, going, gone */
				if (NULL != prev)
				{	/* terminate the chain before the delete */
					GET_LONGP(&prev_chain, prev);
					/* store next_off in old_cse before actually changing it in the buffer(for rolling back) */
					if (horiz_growth)
					{
						old_cse->undo_next_off[0] = prev_chain.next_off;
						old_cse->undo_offset[0] = (block_offset)(prev - buffer);
						assert(old_cse->undo_offset[0]);
					}
					prev_chain.next_off = 0;
					GET_LONGP(prev, &prev_chain);
				} else
					cse->first_off = 0;		/* the whole chain is gone */
			} else
			{	/* stitch up the left and right to account for the hole in the middle */
				/* next_rec_shrink is the change in record size due to the new compression count */
				if (NULL != prev)
				{
					GET_LONGP(&prev_chain, prev);
					/* ??? new compression may be less (ie +) so why are negative shrinks ignored? */
					/* store next_off in old_cse before actually changing it in the buffer(for rolling back) */
					if (horiz_growth)
					{
						old_cse->undo_next_off[0] = prev_chain.next_off;
						old_cse->undo_offset[0] = (block_offset)(prev - buffer);
						assert(old_cse->undo_offset[0]);
					}
					prev_chain.next_off = (unsigned int)(curr - prev - ((bytptr)right_ptr - (bytptr)del_ptr)
						+ (next_rec_shrink > 0 ? next_rec_shrink : 0));
					GET_LONGP(prev, &prev_chain);
				} else	/* curr remains first: adjust the head */
					cse->first_off = (block_offset)(curr - buffer - ((bytptr)right_ptr - (bytptr)del_ptr)
						+ (next_rec_shrink > 0 ? next_rec_shrink : 0));
			}
		}
	}
	horiz_growth = FALSE;
	return cdb_sc_normal;
}
