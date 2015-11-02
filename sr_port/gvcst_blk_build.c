/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "gdsblk.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "copy.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "gvcst_blk_build.h"

#ifdef DEBUG
GBLDEF	boolean_t		skip_block_chain_tail_check;
#endif

GBLREF	unsigned char		cw_set_depth;
GBLREF	short			dollar_tlevel;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	bool			certify_all_blocks;
GBLREF	boolean_t		mu_reorg_upgrd_dwngrd_in_prog;	/* TRUE if MUPIP REORG UPGRADE/DOWNGRADE is in progress */

void gvcst_blk_build(cw_set_element *cse, sm_uc_ptr_t base_addr, trans_num ctn)
{
	blk_segment	*seg, *stop_ptr, *array;
	off_chain	chain;
	sm_uc_ptr_t	ptr, ptrtop;
	sm_ulong_t	n;
	int4		offset;

	assert(dollar_tlevel || cs_addrs->now_crit);
	assert(cse->mode != gds_t_writemap);
	array = (blk_segment *)cse->upd_addr;
	assert(array->len >= sizeof(blk_hdr));
	assert(array->len <= cs_data->blk_size);
	assert((cse->ins_off + sizeof(block_id)) <= array->len);
	assert((short)cse->index >= 0);
	assert(!cse->undo_next_off[0] && !cse->undo_offset[0]);
	assert(!cse->undo_next_off[1] && !cse->undo_offset[1]);

	if (base_addr == NULL)
	{	/* it's the first private TP build */
		assert(dollar_tlevel);
		assert(cse->blk_target);
		base_addr = cse->new_buff = ((new_buff_buddy_list *)get_new_free_element(sgm_info_ptr->new_buff_list))->new_buff;
		cse->first_copy = TRUE;
	} else
   		assert(0 == ((sm_ulong_t)base_addr & 3));	/* word aligned at least */

	/* The block-transaction-number is modified before the contents of the block are modified. This is
	 *     done so as to allow a cdb_sc_blkmod check (done in t_qread, gvcst_search, gvcst_put and tp_hist)
	 *     to be done out-of-crit by just checking for the transaction numbers. If the contents of the block
	 *     were modified first, there is a possibility that the block-transaction number didn't get updated
	 *     although the contents of the block may have changed and basing the decision of block-modified on
	 *     just the transaction numbers may not always be correct.
	 * Note that in mm_update and bg_update there is an else block where instead of gvcst_blk_build(),
	 *     a memcpy is done. To effect the above change, we also need to switch the order of memcpy and
	 *     block-transaction-number-updation in those places.
	 * Note that a similar change is not needed in gvcst_map_build() because that will never be in the
	 *     search history for any key.
	 */
	if (!ctn && !mu_reorg_upgrd_dwngrd_in_prog)
	{
		assert(dollar_tlevel);
		/* Subtract one so will pass concurrency control for mm databases.
		 * This block is guaranteed to be in an earlier history from when it was first read,
		 * so this history is superfluous for concurrency control.
		 * The correct tn is put in the block in mm_update or bg_update when the block is copied to the database.
		 */
		ctn = cs_addrs->ti->curr_tn - 1;
	}
	((blk_hdr_ptr_t)base_addr)->bver = GDSVCURR;
	((blk_hdr_ptr_t)base_addr)->tn = ctn;
	((blk_hdr_ptr_t)base_addr)->bsiz = UINTCAST(array->len);
	((blk_hdr_ptr_t)base_addr)->levl = cse->level;

	if (cse->forward_process)
	{
		stop_ptr = (blk_segment *)array->addr;
		seg = cse->first_copy ? array + 1: array + 2;
		ptr = base_addr + sizeof(blk_hdr);
		if (!cse->first_copy)
			ptr += ((blk_segment *)(array + 1))->len;
		for (  ;seg <= stop_ptr;)
		{
			assert(0L <= ((INTPTR_T)seg->len));
			memmove(ptr, seg->addr, seg->len);
			ptr += seg->len;
			seg++;
		}
	} else
	{
		stop_ptr = cse->first_copy ? array : array + 1;
		seg = (blk_segment *)array->addr;
		ptr = base_addr + array->len;
		while (seg != stop_ptr)
		{
			assert(0L <= ((INTPTR_T)seg->len));
			ptr -= (n = seg->len);
			memmove(ptr, seg->addr, n);
			seg--;
		}
	}
	if (dollar_tlevel)
	{
		if (cse->ins_off)
		{	/* if the cw set has a reference to resolve, move it to the block */
			assert(cse->index < sgm_info_ptr->cw_set_depth);
			assert((int)cse->ins_off >= (int)(sizeof(blk_hdr) + sizeof(rec_hdr)));
			assert((int)(cse->next_off + cse->ins_off + sizeof(block_id)) <= array->len);
			if (cse->first_off == 0)
				cse->first_off = cse->ins_off;
			chain.flag = 1;
			chain.cw_index = cse->index;
			chain.next_off = cse->next_off;
			ptr = base_addr + cse->ins_off;
			GET_LONGP(ptr, &chain);
			cse->index = 0;
			cse->ins_off = 0;
			cse->next_off = 0;
		}
		DEBUG_ONLY(
			if (cse->first_off)
			{	/* verify the integrity of the TP chains within a newly created block */
				ptr = base_addr;
				ptrtop = ptr + cs_data->blk_size;
				for (offset = cse->first_off; (0 < offset); offset = chain.next_off)
				{
					ptr = ptr + offset;
					assert(ptr < ptrtop);	/* ensure we have not overrun the buffer */
					GET_LONGP(&chain, ptr);
					assert(1 == chain.flag || (skip_block_chain_tail_check && (0 == chain.next_off)));
					assert(chain.cw_index < sgm_info_ptr->cw_set_depth);
				}
				assert(0 == offset);	/* ensure the chain is NULL terminated */
			}
		)
	} else
		assert(dollar_tlevel || (cse->index < (int)cw_set_depth));
}
