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
#include "gdsbml.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "t_write_map.h"
#include "min_max.h"
#include "jnl_get_checksum.h"

GBLREF	cw_set_element		cw_set[];
GBLREF	unsigned char		cw_set_depth;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			dollar_tlevel;
GBLREF	unsigned int		t_tries;

void t_write_map (
		srch_blk_status	*blkhist,	/* Search History of the block to be written. Currently the
						 *	following members in this structure are used by "t_write_map"
						 *	    "blk_num"		--> Block number being modified
						 *	    "buffaddr"		--> Address of before image of the block
						 *	    "cr"		--> cache-record that holds the block (BG only)
						 *	    "cycle"		--> cycle when block was read by t_qread (BG only)
						 */
		block_id	*upd_addr,	/* Address of the update array containing list of blocks to be cleared in bitmap */
		trans_num	tn,		/* Transaction Number when this block was read. Used for cdb_sc_blkmod validation */
		int4		reference_cnt)	/* Same meaning as cse->reference_cnt (see gdscc.h for comments) */
{
	cw_set_element		*cs;
	cache_rec_ptr_t		cr;
	jnl_buffer_ptr_t	jbbp;		/* jbbp is non-NULL only if before-image journaling */
	sgmnt_addrs		*csa;
	blk_hdr_ptr_t		old_block;
	unsigned int		bsiz;

#ifdef DEBUG
	block_id		blkid;
	block_id		*updptr;
#endif

	csa = cs_addrs;
	if (!dollar_tlevel)
	{
		assert(cw_set_depth < CDB_CW_SET_SIZE);
		cs = &cw_set[cw_set_depth];
		cs->mode = gds_t_noop;	/* initialize it to a value that is not "gds_t_committed" before incrementing
					 * cw_set_depth as secshr_db_clnup relies on it */
		cw_set_depth++;
	} else
	{
		tp_cw_list(&cs);
		sgm_info_ptr->cw_set_depth++;
	}
	cs->mode = gds_t_writemap;
	cs->blk_checksum = 0;
	cs->blk = blkhist->blk_num;
	assert((cs->blk < csa->ti->total_blks) || (CDB_STAGNATE > t_tries));
	cs->old_block = blkhist->buffaddr;
	BIT_CLEAR_FREE(cs->blk_prior_state);	/* t_write_map operates on BUSY blocks and hence
						 * cs->blk_prior_state's free_status is set to FALSE unconditionally */
	BIT_CLEAR_RECYCLED(cs->blk_prior_state);
	old_block = (blk_hdr_ptr_t)cs->old_block;
	assert(NULL != old_block);
	jbbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
	if ((NULL != jbbp) && (old_block->tn < jbbp->epoch_tn))
	{	/* Pre-compute CHECKSUM. Since we don't necessarily hold crit at this point, ensure we never try to
		 * access the buffer more than the db blk_size.
		 */
		bsiz = MIN(old_block->bsiz, csa->hdr->blk_size);
		cs->blk_checksum = jnl_get_checksum(old_block, csa, bsiz);
	}
	cs->cycle = blkhist->cycle;
	cr = blkhist->cr;
	cs->cr = cr;
	assert((NULL != cr) || (dba_mm == csa->hdr->acc_meth));
	cs->ondsk_blkver = cs_data->desired_db_format;	/* bitmaps are always the desired DB format V6/V6p/V7m/V7 */
	cs->ins_off = 0;
	cs->index = 0;
	assert(reference_cnt < csa->hdr->bplmap);	/* Cannot allocate more blocks than a bitmap holds */
	assert(reference_cnt > -csa->hdr->bplmap);	/* Cannot free more blocks than a bitmap holds */
	cs->reference_cnt = reference_cnt;
	cs->jnl_freeaddr = 0;		/* reset jnl_freeaddr that previous transaction might have filled in */
	cs->upd_addr.map = upd_addr;
	DEBUG_ONLY(
		if (reference_cnt < 0)
			reference_cnt = -reference_cnt;
		/* Check that all block numbers are relative to the bitmap block number (i.e. bit number) */
		updptr = (block_id *)upd_addr;
		while (reference_cnt--)
		{
			blkid = *updptr;
			assert(blkid == (int4)blkid);
			assert((int4)blkid < csa->hdr->bplmap);
			updptr++;
		}
	)
	cs->tn = tn;
	cs->level = LCL_MAP_LEVL;
	cs->done = FALSE;
	cs->write_type = GDS_WRITE_PLAIN;
}
