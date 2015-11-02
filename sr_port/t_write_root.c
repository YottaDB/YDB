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
#include "t_write_root.h"

GBLREF cw_set_element	cw_set[];
GBLREF unsigned char	cw_set_depth;
GBLREF uint4		dollar_tlevel;

/* Create a sibling cw-set-element to store ins_off and index for new root's right child */

void	t_write_root (
			 block_offset 	ins_off,	/*  Offset to the position in the buffer of the previous cw_set-element
							 *  that is to receive a block number when one is created. */
			 block_index 	index         	/*  Index into the create/write set.  The specified entry is always
							 *  a create entry. When the create gets assigned a block number,
							 *  the block number is inserted into the buffer of the previous
							 *  cw-set-element at the location specified by ins_off. */
			 )
{
	cw_set_element	*cse;

	assert(!dollar_tlevel);
	cse = &cw_set[cw_set_depth++];
	assert(CDB_CW_SET_SIZE > cw_set_depth);
	cse->blk_checksum = 0;
	cse->blk = -1;
	cse->mode = gds_t_write_root;
	cse->ins_off = ins_off;
	cse->index = index;
	cse->reference_cnt = 0;
	cse->forward_process = FALSE;
	cse->first_copy = TRUE;
	cse->jnl_freeaddr = 0;		/* reset jnl_freeaddr that previous transaction might have filled in */
	cse->write_type = GDS_WRITE_PLAIN;
}
