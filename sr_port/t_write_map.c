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
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "t_write_map.h"
#include "min_max.h"
#include "jnl_get_checksum.h"

GBLREF cw_set_element	cw_set[];
GBLREF unsigned char	cw_set_depth;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF uint4		dollar_tlevel;
#ifdef GTM_TRUNCATE
GBLREF unsigned int	t_tries;
#endif

void t_write_map (
		srch_blk_status	*blkhist,	/* Search History of the block to be written. Currently the
						 *	following members in this structure are used by "t_write_map"
						 *	    "blk_num"		--> Block number being modified
						 *	    "buffaddr"		--> Address of before image of the block
						 *	    "cr"		--> cache-record that holds the block (BG only)
						 *	    "cycle"		--> cycle when block was read by t_qread (BG only)
						 *	    "cr->ondsk_blkver"	--> Actual block version on disk
						 */
		unsigned char 	*upd_addr,	/* Address of the update array containing list of blocks to be cleared in bitmap */
		trans_num	tn,		/* Transaction Number when this block was read. Used for cdb_sc_blkmod validation */
		int4		reference_cnt)	/* Same meaning as cse->reference_cnt (see gdscc.h for comments) */
{
	cw_set_element		*cs;
	cache_rec_ptr_t		cr;
	jnl_buffer_ptr_t	jbbp;		/* jbbp is non-NULL only if before-image journaling */
	sgmnt_addrs		*csa;
	blk_hdr_ptr_t		old_block;
	unsigned int		bsiz;
	block_id		blkid;
	uint4			*updptr;

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
	assert((cs->blk < csa->ti->total_blks) GTM_TRUNCATE_ONLY(|| (CDB_STAGNATE > t_tries)));
		cs->old_block = blkhist->buffaddr;
	/* t_write_map operates on BUSY blocks and hence cs->blk_prior_state's free_status is set to FALSE unconditionally */
	BIT_CLEAR_FREE(cs->blk_prior_state);
	BIT_CLEAR_RECYCLED(cs->blk_prior_state);
	old_block = (blk_hdr_ptr_t)cs->old_block;
	assert(NULL != old_block);
	jbbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
	if ((NULL != jbbp) && (old_block->tn < jbbp->epoch_tn))
	{	/* Pre-compute CHECKSUM. Since we dont necessarily hold crit at this point, ensure we never try to
		 * access the buffer more than the db blk_size.
		 */
		bsiz = MIN(old_block->bsiz, csa->hdr->blk_size);
		cs->blk_checksum = jnl_get_checksum((uint4*)old_block, csa, bsiz);
	}
	cs->cycle = blkhist->cycle;
	cr = blkhist->cr;
	cs->cr = cr;
	/* the buffer in shared memory holding the GDS block contents currently does not have in its block header the
	 * on-disk format of that block. if it had, we could have easily copied that over to the cw-set-element.
	 * until then, we have to use the cache-record's field "ondsk_blkver". but the cache-record is available only in BG.
	 * thankfully, in MM, we do not allow GDSV4 type blocks, so we can safely assign GDSV6 (or GDSVCURR) to this field.
	 */
	assert((NULL != cr) || (dba_mm == csa->hdr->acc_meth));
	cs->ondsk_blkver = (NULL == cr) ? GDSVCURR : cr->ondsk_blkver;
	cs->ins_off = 0;
	cs->index = 0;
	assert(reference_cnt < csa->hdr->bplmap);	/* Cannot allocate more blocks than a bitmap holds */
	assert(reference_cnt > -csa->hdr->bplmap);	/* Cannot free more blocks than a bitmap holds */
	cs->reference_cnt = reference_cnt;
	cs->jnl_freeaddr = 0;		/* reset jnl_freeaddr that previous transaction might have filled in */
	cs->upd_addr = upd_addr;
	DEBUG_ONLY(
		if (reference_cnt < 0)
			reference_cnt = -reference_cnt;
		/* Check that all block numbers are relative to the bitmap block number (i.e. bit number) */
		updptr = (uint4 *)upd_addr;
		while (reference_cnt--)
		{
			blkid = *updptr;
			assert((int4)blkid < csa->hdr->bplmap);
			updptr++;
		}
	)
	cs->tn = tn;
	cs->level = LCL_MAP_LEVL;
	cs->done = FALSE;
	cs->write_type = GDS_WRITE_PLAIN;
}
