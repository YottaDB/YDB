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
#include "tp.h"
#include "t_create.h"

GBLREF	cw_set_element		cw_set[];
GBLREF	enum db_ver		upgrade_block_split_format; /* Set only in mu_split */
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	unsigned char		cw_set_depth;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	uint4			dollar_tlevel;
GBLREF	gv_namehead		*gv_target;
GBLREF	trans_num		local_tn;	/* transaction number for THIS PROCESS */

block_index t_create (
		      block_id			hint,		/*  A hint block number.  */
		      struct blk_segment_struct	*upd_addr,	/*  address of the block segment array which contains
								 *  update info for the block
								 */
		      block_offset		ins_off,	/*  offset to the position in the buffer that is to receive a block
								 *  number when one is created.
								 */
		      block_index		index,		/*  index into the create/write set.  The specified entry is always
								 *  a created entry. When the create gets assigned a block number,
								 *  the block number is inserted into this buffer at the location
								 *  specified by ins_off.
								 */
		      char			level)
{
	cw_set_element	*cse;

	if (!dollar_tlevel)
	{
		assert(cw_set_depth < CDB_CW_SET_SIZE);
		cse = &cw_set[cw_set_depth];
	} else
	{
		tp_cw_list(&cse);
		assert(gv_target);
		cse->blk_target = gv_target;
	}
	cse->mode = gds_t_create;
	cse->blk_checksum = 0;
	assert(hint);	/* various callers (particularly gvcst_put) rely on gds_t_create cse to have a non-zero "blk" */
	/* Verify that one of 3 conditions is satisfied inorder to validate hint
	 * 1) hint is a fake block ID used internally by GT.M
	 * 	-This is indicated by the MSB of hint being 1 which can be confirmed by checking if hint is negative or not
	 * 2) the current DB uses 64-bit block IDs
	 * 3) hint fits within a 32-bit value
	 */
	assert((hint < 0) || (BLK_ID_32_VER < cs_data->desired_db_format) || ((block_id_32)hint == hint));
	cse->blk = hint;
	cse->upd_addr.blk = upd_addr;
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
	/* When dealing with an index or directory tree block, take upgrade_block_split_format into account.
	 * Others use desired DB fmt.
	 */
	if ((level || (DIR_ROOT == gv_target->root)) && upgrade_block_split_format)
		cse->ondsk_blkver = upgrade_block_split_format;	/* use upgrade_block_split_format as directed by caller */
	else
		cse->ondsk_blkver = cs_data->desired_db_format;	/* Level zero blocks always use desired format */
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
