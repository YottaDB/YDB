/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "tp.h"
#include "gvcst_blk_build.h"
#include "gtmimagename.h"
#include "spec_type.h"
#include "memcoherency.h"

#ifdef DEBUG
GBLREF	boolean_t		skip_block_chain_tail_check;
#endif

GBLREF	unsigned char		cw_set_depth;
GBLREF	uint4			dollar_tlevel;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	boolean_t		mu_reorg_upgrd_dwngrd_in_prog, write_after_image;
GBLREF	unsigned int		t_tries;
GBLREF	jnl_gbls_t		jgbl;

void gvcst_blk_build(cw_set_element *cse, sm_uc_ptr_t base_addr, trans_num ctn)
{
	blk_segment	*seg, *stop_ptr, *array;
	boolean_t	long_blk_id;
	int4		offset;
	off_chain	chain;
	v6_off_chain	v6_chain;
	sm_uc_ptr_t	ptr, ptrtop, c;
	sm_ulong_t	n;
	trans_num	blktn;
	boolean_t	is_mm;
#	ifdef DEBUG
	boolean_t	integ_error_found;
	int4		blk_id_sz, off_chain_sz;
	rec_hdr_ptr_t	rp;
	sm_uc_ptr_t	chainptr, input_base_addr;
	unsigned short	nRecLen;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* For a TP transaction we should reach here with crit as the only function that invokes this is bg_update_phase2
	 * which operates outside crit. The exceptions to this are DSE (write_after_image is TRUE) or ONLINE ROLLBACK
	 * which holds crit for the entire duration
	 */
<<<<<<< HEAD
	is_mm = (dba_mm == cs_data->acc_meth);
	assert((dba_bg != cs_data->acc_meth) || dollar_tlevel || !cs_addrs->now_crit || write_after_image| jgbl.onlnrlbk
		|| ((NULL != cse->recompute_list_head) && (gds_t_write == cse->mode)));
	assert(!is_mm || dollar_tlevel || cs_addrs->now_crit);
=======
	assert((dba_bg != cs_data->acc_meth) || dollar_tlevel || !cs_addrs->now_crit || write_after_image || jgbl.onlnrlbk
		|| mu_reorg_upgrd_dwngrd_in_prog || ((NULL != cse->recompute_list_head) && (gds_t_write == cse->mode)));
	assert((dba_mm != cs_data->acc_meth) || dollar_tlevel || cs_addrs->now_crit);
>>>>>>> 52a92dfd (GT.M V7.0-001)
	assert(cse->mode != gds_t_writemap);
	assert(cse->ondsk_blkver);
	array = (blk_segment *)cse->upd_addr;
	long_blk_id = BLK_ID_32_VER < cse->ondsk_blkver;
#	ifdef DEBUG
	blk_id_sz = SIZEOF_BLK_ID(long_blk_id);
	off_chain_sz = blk_id_sz; /* the off_chain struct should be the same size as the block_id in the current block */
#	endif
	assert(array->len >= SIZEOF(blk_hdr));
	assert(array->len <= cs_data->blk_size);
	assert((cse->ins_off + blk_id_sz) <= array->len);
	assert((short)cse->index >= 0);
	assert(!cse->undo_next_off[0] && !cse->undo_offset[0]);
	assert(!cse->undo_next_off[1] && !cse->undo_offset[1]);
	DEBUG_ONLY(input_base_addr = base_addr;)

	if (base_addr == NULL)
	{	/* it's the first private TP build */
		assert(dollar_tlevel);
		assert(cse->blk_target);
		base_addr = cse->new_buff = (unsigned char *)get_new_free_element(sgm_info_ptr->new_buff_list);
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
	if (!ctn && dollar_tlevel)
	{	/* Subtract one so will pass concurrency control for mm databases.
		 * This block is guaranteed to be in an earlier history from when it was first read,
		 * so this history is superfluous for concurrency control.
		 * The correct tn is put in the block in mm_update or bg_update when the block is copied to the database.
		 */
		ctn = cs_addrs->ti->curr_tn - 1;
	}
	/* Assert that the block's transaction number is LESS than the transaction number corresponding to the blk build.
	 * i.e. no one else should have touched the block contents in shared memory from the time we locked this in phase1
	 * to the time we build it in phase2.
	 * There are a few exceptions.
	 *	a) With DSE, it is possible to change the block transaction number and then a DSE or MUPIP command can run
	 *		on the above block with the above condition not true.
	 *	b) tp_tend calls gvcst_blk_build for cse's with mode kill_t_write/kill_t_create. For them we build a private
	 *		copy of the block for later use in phase2 of the M-kill. In this case, blktn could be
	 *		uninitialized so cannot do any checks using this value.
	 *	c) For MM, we dont have two phase commits so dont do any checks in that case.
	 *	d) For acquired blocks, it is possible that some process had read in the uninitialized block from disk
	 *		outside of crit (due to concurrency issues). Therefore the buffer could contain garbage. So we cannot
	 *		rely on the buffer contents to determine the block's transaction number.
	 *	e) If a twin is created, we explicitly set its buffer tn to be equal to ctn in phase1.
	 *		But since we are not passed the "cr" in this routine, it is not easily possible to check that.
	 *		Hence in case of twinning, we relax the check so buffertn == ctn is allowed.
	 */
	DEBUG_ONLY(blktn = ((blk_hdr_ptr_t)base_addr)->tn);
	assert(!IS_MCODE_RUNNING || !cs_addrs->t_commit_crit || (dba_bg != cs_data->acc_meth) || (n_gds_t_op < cse->mode)
	       || (cse->mode == gds_t_acquired) || ((!cs_data->asyncio && (blktn < ctn)) || (cs_data->asyncio && (blktn <= ctn))));
	/* Assert that "curr_tn" is always less than "early_tn" in the database. There are a few exceptions.
	 * a) With memory instruction reordering it is possible the early_tn we read in the assert below gets executed
	 *    BEFORE the curr_tn read that happens a few lines above. That could then fail this assert (GTM-8523).
	 *    Account for that with the is_mm check below.
	 * b) A concurrent online rollback can take curr_tn back in time so account for that by the
	 *    onln_rlbk_pid (non-zero implies online rollback is still running) and
	 *    MISMATCH_ONLN_RLBK_CYCLES (TRUE return implies online rollback is finished) checks.
	 */
	assert((ctn < cs_addrs->ti->early_tn) || write_after_image || is_mm
		|| (cs_addrs->nl->onln_rlbk_pid || MISMATCH_ONLN_RLBK_CYCLES(cs_addrs, cs_addrs->nl)));
	((blk_hdr_ptr_t)base_addr)->bver = cse->ondsk_blkver;
	((blk_hdr_ptr_t)base_addr)->tn = ctn;
	((blk_hdr_ptr_t)base_addr)->bsiz = UINTCAST(array->len);
	((blk_hdr_ptr_t)base_addr)->levl = cse->level;
	/* At this point, we have already changed the transaction number in the block header. And are now
	 * going to modify the block contents. It is possible a concurrent process is in "t_end.c" validating a read transaction
	 * using the fast path. In that case, it would not get crit but instead invoke the "TP_IS_CDB_SC_BLKMOD" macro to check
	 * if a block that it had in its read-set had changed state. On architectures where memory accesses can be reordered at
	 * runtime (e.g. ARMV7L etc.) it is possible that the concurrent process sees the changes to the block contents BEFORE
	 * it sees changes to the block header. This means that the validation logic would incorrectly conclude that it saw a
	 * consistent copy of the block. To avoid this issue, insert a write memory barrier here. This is necessary for MM.
	 * For BG though, "cr->in_tend" is checked inside the "TP_IS_CDB_SC_BLKMOD" macro and is set only after pinning the
	 * cache-record. The act of pinning uses compswap which does the needed memory barriers so no need of a memory barrier in
	 * the BG case. Also see similar comment in tp_tend before MM_WRITE_MEMORY_BARRIER.
	 */
#	ifndef MM_WRITE_MEMORY_BARRIER_IS_NO_OP
	if (is_mm)
		MM_WRITE_MEMORY_BARRIER;
#	else
	UNUSED(is_mm);
#	endif
	if (cse->forward_process)
	{
		stop_ptr = (blk_segment *)array->addr;
		seg = cse->first_copy ? array + 1: array + 2;
		ptr = base_addr + SIZEOF(blk_hdr);
		if (!cse->first_copy)
		{
			ptr += ((blk_segment *)(array + 1))->len;
			/* Assert that if this is not a private block build (i.e. this is a block build that is happening in the
			 * commit logic), and "cse->first_copy" is TRUE, the corresponding update array points to valid database
			 * shared memory (in case of BG) or mapped memory (in case of MM).
			 */
			assert((NULL != base_addr) || ASSERT_IS_WITHIN_SHM_BOUNDS(((blk_segment *)(array + 1))->addr, cs_addrs));
		}
		for ( ; seg <= stop_ptr; )
		{
			assert(0L <= ((INTPTR_T)seg->len));
			DBG_BG_PHASE2_CHECK_CR_IS_PINNED(cs_addrs, seg);
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
			DBG_BG_PHASE2_CHECK_CR_IS_PINNED(cs_addrs, seg);
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
			assert((int)cse->ins_off >= (int)(SIZEOF(blk_hdr) + SIZEOF(rec_hdr)));
			assert((int)(cse->next_off + cse->ins_off + blk_id_sz) <= array->len);
			if (cse->first_off == 0)
				cse->first_off = cse->ins_off;
			ptr = base_addr + cse->ins_off;
			chain.flag = 1;
			assert((1 << CW_INDEX_MAX_BITS) > cse->index);
			chain.cw_index = cse->index;
			assert((1LL << NEXT_OFF_MAX_BITS) > cse->next_off);
			chain.next_off = cse->next_off;
			WRITE_OFF_CHAIN(long_blk_id, &chain, &v6_chain, ptr);
			cse->index = 0;
			cse->ins_off = 0;
			cse->next_off = 0;
		}
#		ifdef DEBUG
		if (offset = cse->first_off)
		{	/* Verify the integrity of the TP chains within a newly created block.
			 * If it is the first TP private build, the update array could have referenced
			 * shared memory global buffers which could have been concurrently updated.
			 * So the integrity of the chain cannot be easily verified. If ever we find
			 * an integ error in the chain, we check if this is the first private TP build
			 * and if so allow it but set a debug flag donot_commit so we never ever commit
			 * this transaction. The hope is that it will instead restart after validation.
			 */
			ptr = base_addr;
			ptrtop = ptr + ((blk_hdr_ptr_t)ptr)->bsiz;
			chainptr = ptr + offset;
			ptr += SIZEOF(blk_hdr);
			integ_error_found = FALSE;
			for ( ; ptr < ptrtop; )
			{
				do
				{
					GET_USHORT(nRecLen, &((rec_hdr_ptr_t)ptr)->rsiz);
					if (0 == nRecLen)
					{
						assert(NULL == input_base_addr);
						integ_error_found = TRUE;
						break;
					}
					c = ptr;
					c += SIZEOF(rec_hdr);
					/* The *-key does not have a key. Everything else has one. Account for that. */
					if (bstar_rec_size(long_blk_id) != nRecLen)
					{
						for ( ; (c < ptrtop) && ((*c++ != KEY_DELIMITER) || (*c != KEY_DELIMITER)); )
							;
						if (c >= ptrtop)
						{
							assert(NULL == input_base_addr);
							integ_error_found = TRUE;
							break;
						}
						c++;
					}
					ptr += nRecLen;
					if (c == chainptr)
					{
						if (((ptr - off_chain_sz) != chainptr)
							&& ((ptr - off_chain_sz - COLL_SPEC_LEN) != chainptr))
						{
							assert(NULL == input_base_addr);
							integ_error_found = TRUE;
						}
						break;
					}
					if (c > chainptr)
					{
						assert(NULL == input_base_addr);
						integ_error_found = TRUE;
						break;
					}
					READ_OFF_CHAIN(long_blk_id, &chain, &v6_chain, c);
					if (chain.flag)
					{
						assert(NULL == input_base_addr);
						integ_error_found = TRUE;
						break;
					}
				} while (ptr < ptrtop);
				if (integ_error_found)
					break;
				if (chainptr < ptrtop)
				{
					READ_OFF_CHAIN(long_blk_id, &chain, &v6_chain, chainptr);
					assert(1 == chain.flag || (skip_block_chain_tail_check && (0 == chain.next_off)));
					assert(chain.cw_index < sgm_info_ptr->cw_set_depth);
					offset = chain.next_off;
					if (0 == offset)
						chainptr = ptrtop;
					else
					{
						chainptr = chainptr + offset;
						assert(chainptr < ptrtop);	/* ensure we have not overrun the buffer */
					}
				}
			}
			if (integ_error_found)
				TREF(donot_commit) |= DONOTCOMMIT_GVCST_BLK_BUILD_TPCHAIN;
			else
				assert(0 == offset);	/* ensure the chain is NULL terminated */
		}
#		endif
	} else
		assert(dollar_tlevel || (cse->index < (int)cw_set_depth));
}
