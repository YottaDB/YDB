/****************************************************************
 *								*
 *	Copyright 2004, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "gdskill.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "jnl.h"
#include "copy.h"
#include "gvcst_protos.h"	/* for gvcst_search_blk prototypes */
#include "op.h"			/* for add_mvals prototype */
#include "jnl_get_checksum.h"

GBLREF	short			dollar_tlevel;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	mval			*post_incr_mval;	/* mval pointing to the post-$INCR value */
GBLREF	jnl_format_buffer       *non_tp_jfb_ptr;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	int			gv_fillfactor, rc_set_fragment;	/* Contains offset within data at which data fragment starts */
GBLREF	unsigned char		cw_set_depth;
GBLREF	gv_key			*gv_currkey;
GBLREF	unsigned int		t_tries;
GBLREF	uint4			update_array_size;

/* --------------------------------------------------------------------------------------------
 * This code is very similar to the code in gvcst_put for the non-block-split case as well as
 * the code in recompute_upd_array in tp_tend.c. All of these need to be maintained in sync.
 * --------------------------------------------------------------------------------------------
 */

enum cdb_sc	gvincr_recompute_upd_array(srch_blk_status *bh, struct cw_set_element_struct *cse, cache_rec_ptr_t cr)
{
	blk_segment		*bs1, *bs_ptr;
	char			*va;
	enum cdb_sc		status;
	int4			blk_size, blk_fill_size, cur_blk_size, blk_seg_cnt, delta, tail_len, new_rec_size;
	int4			target_key_size, data_len;
	mstr			value;
	rec_hdr_ptr_t		curr_rec_hdr, rp;
	sm_uc_ptr_t		cp1, buffaddr;
	unsigned short		rec_size;
	jnl_format_buffer	*jfb;
	jnl_action		*ja;
	blk_hdr_ptr_t		old_block;
	sgmnt_addrs		*csa;

	csa = cs_addrs;
	assert(!dollar_tlevel);	/* this recomputation is currently supported only for non-TP */
	assert(0 == cse->level);	/* better be a leaf-level block */
	assert(csa->now_crit);
	assert(!cse->level && (gds_t_write == cse->mode) && (NULL == cse->new_buff) && (GDS_WRITE_PLAIN == cse->write_type));
	blk_size = cs_data->blk_size;	/* "blk_size" is also used by the BLK_FINI macro below */
	blk_fill_size = (blk_size * gv_fillfactor) / 100 - cs_data->reserved_bytes;
	/* clues for gv_target involved in recomputation need not be nullified since only the value changes (not the key) */
	assert(CR_NOTVALID != (sm_long_t)cr);
	if (NULL == cr || CR_NOTVALID == (sm_long_t)cr || 0 <= cr->read_in_progress)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_lostcr;
	}
	buffaddr = bh->buffaddr;
	target_key_size = gv_currkey->end + 1;
	if (cdb_sc_normal != (status = gvcst_search_blk(gv_currkey, bh)))
	{
		assert(CDB_STAGNATE > t_tries);
		return status;
	}
	if (target_key_size != bh->curr_rec.match)	/* key does not exist, nothing doable here, restart transaction */
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_blkmod;
	}
	cur_blk_size = ((blk_hdr_ptr_t)buffaddr)->bsiz;
	rp = (rec_hdr_ptr_t)(buffaddr + bh->curr_rec.offset);
	GET_USHORT(rec_size, &rp->rsiz);
	data_len = rec_size + rp->cmpc - sizeof(rec_hdr) - target_key_size;
	if (cdb_sc_normal != (status = gvincr_compute_post_incr(bh)))
	{
		assert(CDB_STAGNATE > t_tries);
		return status;
	}
	assert(MV_IS_STRING(post_incr_mval));	/* gvincr_recompute_post_incr should have set it to be a of type MV_STR */
	value = post_incr_mval->str;
	new_rec_size = rec_size - data_len + value.len;
	delta = new_rec_size - rec_size;
	if ((cur_blk_size + delta) > blk_fill_size)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_blksplit;
	}
	if (0 != rc_set_fragment)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_mkblk;	/* let gvcst_put do the recomputation out of crit in case of rc_set */
	}
	/* Note that a lot of the code below relies on the fact that we are in non-TP. For TP we need to do extra stuff */
	assert(NULL != update_array);
	assert(NULL != update_array_ptr);
	assert(0 != update_array_size);
	assert(update_array + update_array_size >= update_array_ptr);
	assert(1 == cw_set_depth);
	/* since cw_set_depth is guaranteed to be 1 (by the above assert), we can be sure that the only update array space we would
	 * have used is for the current (and only) cw_set_element "cse" and hence can reuse the space by resetting update_array_ptr
	 */
	assert(ROUND_UP2((int)update_array, UPDATE_ELEMENT_ALIGN_SIZE) == (int)cse->upd_addr);
	update_array_ptr = update_array;
	BLK_INIT(bs_ptr, bs1);
	BLK_SEG(bs_ptr, buffaddr + sizeof(blk_hdr), bh->curr_rec.offset - sizeof(blk_hdr));
	BLK_ADDR(curr_rec_hdr, sizeof(rec_hdr), rec_hdr);
	curr_rec_hdr->rsiz = new_rec_size;
	curr_rec_hdr->cmpc = bh->prev_rec.match;
	BLK_SEG(bs_ptr, (sm_uc_ptr_t)curr_rec_hdr, sizeof(rec_hdr));
	BLK_ADDR(cp1, target_key_size - bh->prev_rec.match, unsigned char);
	memcpy(cp1, gv_currkey->base + bh->prev_rec.match, target_key_size - bh->prev_rec.match);
	BLK_SEG(bs_ptr, cp1, target_key_size - bh->prev_rec.match);
	assert(0 != value.len);
	BLK_ADDR(va, value.len, char);
	memcpy(va, value.addr, value.len);
	BLK_SEG(bs_ptr, (unsigned char *)va, value.len);
	rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + rec_size);
	tail_len = cur_blk_size - ((sm_uc_ptr_t)rp - buffaddr);
	assert(tail_len >= 0); /* else gvincr_recompute_post_incr would have returned cdb_sc_rmisalign and we will not be here */
	if (tail_len > 0)
	{
		BLK_SEG(bs_ptr, (sm_uc_ptr_t)rp, tail_len);
	}
	if (0 == BLK_FINI(bs_ptr, bs1))
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_mkblk;
	}
	cse->upd_addr = (unsigned char *)bs1;
	/* assert that cse->old_block is indeed pointing to the buffer that the cache-record is pointing to.
	 * this is necessary to ensure that we are copying "ondsk_blkver" from the correct cache-record.
	 * there is a possibility that this assert might not hold true which is if we are in a restartable situation.
	 * but in that case do the same check that t_end will perform to determine this.
	 */
	assert((cse->old_block == (sm_uc_ptr_t)GDS_REL2ABS(cr->buffaddr)) || (bh->cycle != cr->cycle) || (bh->cr != cr));
	cse->ondsk_blkver = cr->ondsk_blkver;
	cse->done = FALSE;
	if (JNL_ENABLED(csa))
	{	/* Re-format the logical SET jnl-record */
		jfb = non_tp_jfb_ptr;
		DEBUG_ONLY(jgbl.cumul_index = jgbl.cu_jnl_index = 0;)
		ja = &(jfb->ja);
		ja->key = gv_currkey;
		ja->val = post_incr_mval;
		ja->operation = JNL_SET;
		jnl_format(jfb);
		jgbl.cumul_jnl_rec_len = jfb->record_size;
		assert(0 == jgbl.cumul_jnl_rec_len % JNL_REC_START_BNDRY);
		DEBUG_ONLY(jgbl.cumul_index++;)
		if (csa->jnl_before_image && (NULL != cse->old_block))
		{
			old_block = (blk_hdr_ptr_t)cse->old_block;
			if (old_block->tn < csa->jnl->jnl_buff->epoch_tn)
				cse->blk_checksum = jnl_get_checksum(INIT_CHECKSUM_SEED, (uint4 *)old_block, old_block->bsiz);
			else
				cse->blk_checksum = 0;
		}
	}
	return cdb_sc_normal;
}
