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

#include "mdef.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "copy.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "t_write.h"

GBLREF cw_set_element	cw_set[];
GBLREF unsigned char	cw_set_depth;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF short		dollar_tlevel;
GBLREF trans_num	local_tn;	/* transaction number for THIS PROCESS */
GBLREF gv_namehead	*gv_target;
GBLREF uint4		t_err;
GBLREF unsigned int	t_tries;
GBLREF boolean_t	horiz_growth;
GBLREF int4		prev_first_off, prev_next_off;

error_def(ERR_GVKILLFAIL);
cw_set_element *t_write (
			 block_id 	blk,	   	/*  Block number being written */
			 unsigned char 	*upd_addr,	/*  Address of the local buffer which contains
							 *  the block to be written */
			 block_offset 	ins_off,	/*  Offset to the position in the buffer that is to receive
							 *  a block number when one is created. */
			 block_index 	index,         	/*  Index into the create/write set.  The specified entry is
							 *  always a create entry. When the create gets assigned a
							 *  block number, the block number is inserted into this
							 *  buffer at the location specified by ins_off. */
			 sm_uc_ptr_t	old_addr,	/* address of before image of the block */
			 char           level,
			 bool		first_copy,	/* Is first copy needed if overlaying same buffer? */
			 bool		forward)	/* Is forward processing required? */
{
	cw_set_element	*cse, *tp_cse, *old_cse;
	off_chain	chain;
	uint4		dummy, iter;
	srch_blk_status	*tp_srch_status;

	horiz_growth = FALSE;

	/* When the following two asserts trip, we should change the data types of prev_first_off
	 * and prev_next_off, so they satisfy the assert.
	 */
	assert(sizeof(prev_first_off) > sizeof(block_offset));
	assert(sizeof(prev_next_off) > sizeof(block_offset));

	if (dollar_tlevel == 0)
	{
		if (blk >= cs_addrs->ti->total_blks)
			GTMASSERT;
		cse = &cw_set[cw_set_depth++];
		assert(cw_set_depth < CDB_CW_SET_SIZE);
		assert(index < (int)cw_set_depth);
		cse->mode = gds_t_write;
		cse->new_buff = NULL;
		cse->old_block = old_addr;
		tp_cse = NULL;	/* don't bother returning dse for non-TP; it's almost never needed and it distiguishes the cases */
	} else
	{
		assert(!index || index < sgm_info_ptr->cw_set_depth);
		chain = *(off_chain *)&blk;
		if (chain.flag == 1)
		{
			tp_get_cw(sgm_info_ptr->first_cw_set, (int)chain.cw_index, &cse);
			blk = cse->blk;
		} else
		{
			tp_srch_status = (srch_blk_status *)lookup_hashtab_ent(sgm_info_ptr->blks_in_use, (void *)blk, &dummy);
			cse = tp_srch_status ? tp_srch_status->ptr : NULL;
				/* tp_srch_status->ptr always returns latest in the horizontal list */
	    	}
		assert(!cse || !cse->high_tlevel);
		if (cse == NULL)
		{
			tp_cw_list(&cse);
			sgm_info_ptr->cw_set_depth++;
			assert(gv_target);
			cse->blk_target = gv_target;
			gv_target->write_local_tn = local_tn;
			cse->mode = gds_t_write;
			cse->new_buff = NULL;
			cse->old_block = old_addr;
		} else
		{
			assert(cse->done);
			assert(dollar_tlevel >= cse->t_level);
			if (cse->t_level != dollar_tlevel)
			{
				/* this part of the code is similar to that in gvcst_delete_blk(),
				 * any changes in one should be reflected in the other */
				horiz_growth = TRUE;
				old_cse = cse;
				cse = (cw_set_element *)get_new_free_element(sgm_info_ptr->tlvl_cw_set_list);
				memcpy(cse, old_cse, sizeof(cw_set_element));
				cse->low_tlevel = old_cse;
				cse->high_tlevel = NULL;
				old_cse->high_tlevel = cse;
				cse->t_level = dollar_tlevel;
				assert(2 == (sizeof(cse->undo_offset) / sizeof(cse->undo_offset[0])));
				assert(2 == (sizeof(cse->undo_next_off) / sizeof(cse->undo_next_off[0])));
				for (iter = 0; iter < 2; iter++)
					cse->undo_next_off[iter] = cse->undo_offset[iter] = 0;
				assert(old_cse->new_buff);
				assert(old_cse->done);
				cse->new_buff = NULL;
				if (PREV_OFF_INVALID != prev_first_off)
					old_cse->first_off = prev_first_off;
				if (PREV_OFF_INVALID != prev_next_off)
					old_cse->next_off = prev_next_off;
			}
			assert(cse->blk == blk);
			assert(cse->reference_cnt == 0);
			switch (cse->mode)
			{
			case kill_t_create:
				assert(CDB_STAGNATE > t_tries);
				cse->mode = gds_t_create;
				break;
			case kill_t_write:
				assert(CDB_STAGNATE > t_tries);
				cse->mode = gds_t_write;
				break;
			default:
				;
			}
		}
		tp_cse = cse;
	}
	cse->blk = blk;
	cse->upd_addr = upd_addr;
	cse->ins_off = ins_off;
	cse->index = index;
	cse->reference_cnt = 0;
	cse->level = level;
	if (horiz_growth)
		cse->first_copy = TRUE;
	else
		cse->first_copy = first_copy;
	cse->done = FALSE;
	cse->forward_process = forward;
	cse->t_level = dollar_tlevel;
	cse->write_type |= (ERR_GVKILLFAIL == t_err) ? GDS_WRITE_KILL : GDS_WRITE_PLAIN;
	prev_first_off = prev_next_off = PREV_OFF_INVALID;
	return tp_cse;
}
