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

#define TP_ALLOCATION_CLUE_INCREMENT	1	/* This is the default value. Defined below (under a #ifdef) is another value
						 * Enable below #ifdef/#endif block if you want that value instead.
						 */
#ifdef TP_ALLOCATION_CLUE_BUMP_BY_512
#define	TP_ALLOCATION_CLUE_INCREMENT	512	/* Change this to some other number if you want a different bump to the
						 * tp allocation clue each time it is used.
						 */
#endif

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
			tp_allocation_clue = gtm_tp_allocation_clue + 1; /* + 1 so we dont start out with 0 value for "hint" */
			hint = tp_allocation_clue;			 /* this is copied over to cse->blk which is asserted
									  * in gvcst_put as never being 0.
									  */
		} else
		{
			tp_allocation_clue += TP_ALLOCATION_CLUE_INCREMENT;
			hint = tp_allocation_clue;
			/* What if hint becomes greater than total_blks. Should we wrap back to 0? */
			if (tp_allocation_clue < 0)
				GTMASSERT;
		}
		tp_cw_list(&cse);
		assert(gv_target);
		cse->blk_target = gv_target;
	}
	cse->mode = gds_t_create;
	cse->blk_checksum = 0;
	assert(hint);	/* various callers (particularly gvcst_put) rely on gds_t_create cse to have a non-zero "blk" */
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
	assert (NULL != gv_target);	/* t_create is called by gvcst kill/put,mu split/swap_blk, where gv_target can't be NULL */
	/* For uninitialized gv_target, initialize the in_tree status as IN_DIR_TREE, which later may be modified by t_write */
	if (0 == gv_target->root)
		BIT_SET_DIR_TREE(cse->blk_prior_state);
	else
		(DIR_ROOT == gv_target->root) ? BIT_SET_DIR_TREE(cse->blk_prior_state) : BIT_SET_GV_TREE(cse->blk_prior_state);
	if (!dollar_tlevel)
		return(cw_set_depth++);
	else
		return(sgm_info_ptr->cw_set_depth++);
}
