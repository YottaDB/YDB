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
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "copy.h"
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "t_write.h"
#include "min_max.h"
#include "jnl_get_checksum.h"

GBLREF	cw_set_element	cw_set[];
GBLREF	unsigned char	cw_set_depth;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgm_info	*sgm_info_ptr;
GBLREF	uint4		dollar_tlevel;
GBLREF	trans_num	local_tn;	/* transaction number for THIS PROCESS */
GBLREF	gv_namehead	*gv_target;
GBLREF	uint4		t_err;
GBLREF	unsigned int	t_tries;
GBLREF	boolean_t	horiz_growth;
GBLREF	int4		prev_first_off, prev_next_off;
GBLREF	boolean_t	mu_reorg_process;
GBLREF	boolean_t	dse_running;
GBLREF 	jnl_gbls_t	jgbl;

cw_set_element *t_write (
			srch_blk_status	*blkhist,	/* Search History of the block to be written. Currently the
							 *	following members in this structure are used by "t_write"
							 *	    "blk_num"		--> Block number being modified
							 *	    "buffaddr"		--> Address of before image of the block
							 *	    "cr->ondsk_blkver"	--> Actual block version on disk
							 */
			unsigned char 	*upd_addr,	/* Address of the update array that contains the changes for this block */
			block_offset 	ins_off,	/* Offset to the position in the buffer that is to receive
							 * 	a block number when one is created. */
			block_index 	index,		/* Index into the create/write set.  The specified entry is
							 * 	always a create entry. When the create gets assigned a
							 * 	block number, the block number is inserted into this
							 * 	buffer at the location specified by ins_off. */
			char		level,		/* Level of the block in the tree */
			boolean_t	first_copy,	/* Is first copy needed if overlaying same buffer? */
			boolean_t	forward,	/* Is forward processing required? */
			uint4		write_type)	/* Whether "killtn" of the bt needs to be simultaneously updated or not */
{
	cw_set_element		*cse, *tp_cse, *old_cse;
	off_chain		chain;
	uint4			iter;
	srch_blk_status		*tp_srch_status;
	ht_ent_int4		*tabent;
	block_id		blk;
	cache_rec_ptr_t		cr;
	boolean_t		new_cse;	/* TRUE if we had to create a new cse for the input block */
	jnl_buffer_ptr_t	jbbp;		/* jbbp is non-NULL only if before-image journaling */
	sgmnt_addrs		*csa;
	blk_hdr_ptr_t		old_block;
	unsigned int		bsiz;

	csa = cs_addrs;
	horiz_growth = FALSE;

	/* When the following two asserts trip, we should change the data types of prev_first_off
	 * and prev_next_off, so they satisfy the assert.
	 */
	assert(SIZEOF(prev_first_off) >= SIZEOF(block_offset));
	assert(SIZEOF(prev_next_off) >= SIZEOF(block_offset));

	blk = blkhist->blk_num;
	if (!dollar_tlevel)
	{
		assert((blk < csa->ti->total_blks) GTM_TRUNCATE_ONLY(|| (CDB_STAGNATE > t_tries)));
		cse = &cw_set[cw_set_depth];
		cse->mode = gds_t_noop;	/* initialize it to a value that is not "gds_t_committed" before incrementing
					 * cw_set_depth as secshr_db_clnup relies on it */
		cw_set_depth++;
		assert(cw_set_depth < CDB_CW_SET_SIZE);
		assert(index < (int)cw_set_depth);
		new_cse = TRUE;
		tp_cse = NULL; /* dont bother returning tp_cse for non-TP; it's almost never needed and it distiguishes the cases */
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
			if (NULL != (tabent = lookup_hashtab_int4(sgm_info_ptr->blks_in_use, (uint4 *)&blk)))
				tp_srch_status = (srch_blk_status *)tabent->value;
			else
				tp_srch_status = NULL;
			cse = tp_srch_status ? tp_srch_status->cse : NULL;
				/* tp_srch_status->cse always returns latest in the horizontal list */
	    	}
		assert(!cse || !cse->high_tlevel);
		if (cse == NULL)
		{
			tp_cw_list(&cse);
			sgm_info_ptr->cw_set_depth++;
			assert(gv_target);
			cse->blk_target = gv_target;
			new_cse = TRUE;
		} else
		{
			new_cse = FALSE;
			assert(cse->done);
			assert(dollar_tlevel >= cse->t_level);
			if (cse->t_level != dollar_tlevel)
			{
				/* this part of the code is similar to that in gvcst_delete_blk(),
				 * any changes in one should be reflected in the other */
				horiz_growth = TRUE;
				old_cse = cse;
				cse = (cw_set_element *)get_new_free_element(sgm_info_ptr->tlvl_cw_set_list);
				memcpy(cse, old_cse, SIZEOF(cw_set_element));
				cse->low_tlevel = old_cse;
				cse->high_tlevel = NULL;
				old_cse->high_tlevel = cse;
				cse->t_level = dollar_tlevel;
				assert(2 == (SIZEOF(cse->undo_offset) / SIZEOF(cse->undo_offset[0])));
				assert(2 == (SIZEOF(cse->undo_next_off) / SIZEOF(cse->undo_next_off[0])));
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
			/* cse->mode can be kill_t_create or kill_t_write only if we have a restartable situation.
			 * this is because a TP transaction should never try modifying a block that is no longer visible in the
			 * tree. the only exception is if due to concurrency issues, we read a stale copy of a buffer that
			 * incorrectly led us to this child block number. this is a restartable situation.
			 * since this routine does not return a failure code, we continue and expect tp_tend to detect this.
			 */
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
	if (new_cse)
	{
		cse->blk_checksum = 0;
		cse->blk = blk;
		cse->mode = gds_t_write;
		cse->new_buff = NULL;
		cse->old_block = blkhist->buffaddr;
		old_block = (blk_hdr_ptr_t)cse->old_block;
		assert(NULL != old_block);
		jbbp = (JNL_ENABLED(csa) && csa->jnl_before_image) ? csa->jnl->jnl_buff : NULL;
		if ((NULL != jbbp) && (old_block->tn < jbbp->epoch_tn))
		{	/* Pre-compute CHECKSUM. Since we dont necessarily hold crit at this point, ensure we never try to
			 * access the buffer more than the db blk_size.
			 */
			bsiz = MIN(old_block->bsiz, csa->hdr->blk_size);
			cse->blk_checksum = jnl_get_checksum((uint4*)old_block, csa, bsiz);
		}
		/* the buffer in shared memory holding the GDS block contents currently does not have in its block header the
		 * on-disk format of that block. if it had, we could have easily copied that over to the cw-set-element.
		 * until then, we have to use the cache-record's field "ondsk_blkver". but the cache-record is available only in BG.
		 * thankfully, in MM, we do not allow GDSV4 type blocks, so we can safely assign GDSV6 (or GDSVCURR) to this field.
		 */
		cr = blkhist->cr;
		assert((NULL != cr) || (dba_mm == csa->hdr->acc_meth));
		cse->ondsk_blkver = (NULL == cr) ? GDSVCURR : cr->ondsk_blkver;
		/* For uninitialized gv_target, initialize the in_tree status as IN_DIR_TREE */
		assert (NULL != gv_target || dse_running || jgbl.forw_phase_recovery);
		if (NULL == gv_target || 0 == gv_target->root)
			BIT_SET_DIR_TREE(cse->blk_prior_state);
		else
			(DIR_ROOT == gv_target->root)? BIT_SET_DIR_TREE(cse->blk_prior_state) :
						       BIT_SET_GV_TREE(cse->blk_prior_state);
	} else
	{	/* we did not create a new cse. assert the integrity of few fields filled in when this cse was created */
		assert(cse->blk == blk);
		assert(0 == cse->reference_cnt);
		/* If we did not create a new cse, check that the level already stored in the cse is the same as the input level.
		 * It is possible that they are different but that would mean we are in one of two situations
		 *	1) A restartable situation. Since this routine does not currently return a failure code,
		 *		we do not restart here but instead wait for some other failure-code-returning-function
		 *		(if nothing else, the function tp_tend) to catch this situation and trigger a restart.
		 *	2) This block number is the root block of a GVT or Directory Tree and the height of the tree
		 *		is increasing now. In either case cse->blk_target points to the gv_target for that tree.
		 *		The only exception to this is if the global's root is being created.
		 */
		assert(cse->level == level || (CDB_STAGNATE > t_tries) || gds_t_create == cse->mode
			|| cse->blk_target->root == cse->blk);
	}
	cse->upd_addr = upd_addr;
	cse->ins_off = ins_off;
	cse->index = index;
	cse->reference_cnt = 0;
	cse->level = level;
	/* t_write operates on BUSY blocks and hence cse->blk_prior_state's free_status is set to FALSE unconditionally */
	BIT_CLEAR_FREE(cse->blk_prior_state);
	BIT_CLEAR_RECYCLED(cse->blk_prior_state);
	if (horiz_growth)
		cse->first_copy = TRUE;
	else
		cse->first_copy = first_copy;
	cse->done = FALSE;
	cse->forward_process = forward;
	cse->jnl_freeaddr = 0;		/* reset jnl_freeaddr that previous transaction might have filled in */
	cse->t_level = dollar_tlevel;
	/* All REORG operations should disable the "indexmod" optimization (C9B11-001813/C9H12-002934). Assert that. */
	assert(!mu_reorg_process || (GDS_WRITE_KILLTN == write_type));
	if (dollar_tlevel)
		cse->write_type |= write_type;
	else
		cse->write_type = write_type;
	prev_first_off = prev_next_off = PREV_OFF_INVALID;
	blkhist->cse = cse;	/* indicate to t_end/tp_tend that this block is part of the write-set */
	return tp_cse;
}
