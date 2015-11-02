/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "t_create.h"

GBLREF	cw_set_element		cw_set[];
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	unsigned char		cw_set_depth;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	uint4			dollar_tlevel;
GBLREF	block_id		tp_allocation_clue;
GBLREF	gv_namehead		*gv_target;
GBLREF	trans_num		local_tn;	/* transaction number for THIS PROCESS */
GBLREF	block_id		gtm_tp_allocation_clue;	/* block# hint to start allocation for created blocks in TP */

block_index t_create (
		      block_id	hint,			/*  A hint block number.  */
		      unsigned char	*upd_addr,	/*  address of the block segment array which contains
							 *  update info for the block
							 */
		      block_offset 	ins_off,	/*  offset to the position in the buffer that is to receive a block number
							 *  when one is created.
							 */
		      block_index	index,          /*  index into the create/write set.  The specified entry is always a
							 *  created entry. When the create gets assigned a block number,
							 *  the block number is inserted into this buffer at the location
							 *  specified by ins_off.
							 */
		      char		level)
{
	cw_set_element	*cse;

	if (!dollar_tlevel)
	{
		assert(cw_set_depth < CDB_CW_SET_SIZE);
		cse = &cw_set[cw_set_depth];
	} else
	{
		if (!tp_allocation_clue)
		{
			tp_allocation_clue = gtm_tp_allocation_clue + 1;
			hint = tp_allocation_clue;
		} else
		{
			hint = ++tp_allocation_clue;
			if (tp_allocation_clue < 0)
				GTMASSERT;
		}
		tp_cw_list(&cse);
		assert(gv_target);
		cse->blk_target = gv_target;
	}

	cse->mode = gds_t_create;
	cse->blk_checksum = 0;
	cse->blk = hint;
	cse->upd_addr = upd_addr;
	cse->ins_off = ins_off;
	cse->index = index;
	cse->reference_cnt = 0;
	cse->level = level;
	cse->first_copy = TRUE;
	cse->jnl_freeaddr = 0;		/* reset jnl_freeaddr that previous transaction might have filled in */
	cse->done = FALSE;
	cse->new_buff = 0;
	cse->write_type = GDS_WRITE_PLAIN;
	cse->t_level = dollar_tlevel;
	cse->low_tlevel = NULL;
	cse->high_tlevel = NULL;
	cse->ondsk_blkver = GDSVCURR;
	if (!dollar_tlevel)
		return(cw_set_depth++);
	else
		return(sgm_info_ptr->cw_set_depth++);
}
