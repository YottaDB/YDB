/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "copy.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "longcpy.h"
#include "hashtab_int4.h"	/* needed for tp.h and cws_insert.h */
#include "hashtab.h"
#include "tp.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "cws_insert.h"

#define MMBLK_OFFSET(BLK)     													\
	(cs_addrs->db_addrs[0] + (cs_addrs->hdr->start_vbn - 1) * DISK_BLOCK_SIZE + (off_t)(cs_addrs->hdr->blk_size * (BLK)))

#define	REPOSITION_PTR(ptr, type, delta, begin, end)			\
{									\
	assert((sm_uc_ptr_t)(ptr) >= (sm_uc_ptr_t)(begin));		\
	assert((sm_uc_ptr_t)(ptr) <= (sm_uc_ptr_t)(end));		\
	(ptr) = (type *)((sm_uc_ptr_t)(ptr) + delta);			\
}

#define	REPOSITION_PTR_IF_NOT_NULL(ptr, type, delta, begin, end)	\
{									\
	if (ptr)							\
		REPOSITION_PTR(ptr, type, delta, begin, end);		\
}


GBLREF gv_namehead	*gv_target, *gv_target_list;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF trans_num	local_tn;	/* transaction number for THIS PROCESS */
GBLREF int4		n_pvtmods, n_blkmods;
GBLREF unsigned int	t_tries;
GBLREF uint4		t_err;
GBLREF int4		tprestart_syslog_delta;
GBLREF	boolean_t	is_updproc;
GBLREF	boolean_t	mupip_jnl_recover;
GBLREF	boolean_t	gvdupsetnoop; /* if TRUE, duplicate SETs update journal but not database (except for curr_tn++) */

void	gds_tp_hist_moved(sgm_info *si, srch_hist *hist1);

enum cdb_sc tp_hist(srch_hist *hist1)
{
	int 		hist_index;
	srch_hist	*hist;
	off_chain	chain;
	srch_blk_status	*t1, *t2;
	block_id	blk;
	srch_blk_status	*local_hash_entry;
	enum cdb_sc	status = cdb_sc_normal;
	boolean_t	is_mm, store_history;
	sgm_info	*si;
	ht_ent_int4	*tabent;

	error_def(ERR_TRANS2BIG);
	error_def(ERR_GVKILLFAIL);
	error_def(ERR_GVPUTFAIL);

	is_mm = (dba_mm == cs_addrs->hdr->acc_meth);
	si = sgm_info_ptr;
	store_history = (!gv_target->noisolation || ERR_GVKILLFAIL == t_err || ERR_GVPUTFAIL == t_err);
	assert(hist1 != &gv_target->hist);
	/* Ideally, store_history should be computed separately for blk_targets of gv_target->hist and hist1,
	 * i.e. within the outer for loop below. But this is not needed since if t_err is ERR_GVPUTFAIL,
	 * store_history is TRUE both for gv_target->hist and hist1 (which is cs_addrs->dir_tree if non-NULL)
	 * (in the latter case, TRUE because cs_addrs->dir_tree always has NOISOLATION turned off) and if not,
	 * we should have the same blk_target for both the histories and hence the same value for
	 * !blk_target->noisolation and hence the same value for store_history. We assert this is the case below.
	 */
	assert(ERR_GVPUTFAIL == t_err && (NULL == hist1 || hist1 == &cs_addrs->dir_tree->hist && !cs_addrs->dir_tree->noisolation)
			|| !hist1 || hist1->h[0].blk_target == gv_target);

	for (hist = &gv_target->hist; hist != NULL && cdb_sc_normal == status; hist = (hist == &gv_target->hist) ? hist1 : NULL)
	{	/* this loop execute once or twice: 1st for gv_target and then for hist1, if any */
		if (tprestart_syslog_delta)
			n_blkmods = n_pvtmods = 0;
		for (t1 = hist->h; HIST_TERMINATOR != (blk = t1->blk_num); t1++)
		{
			if (si->start_tn > t1->tn)
				si->start_tn = t1->tn;

			/* Since this validation is done out of crit, it is crucial to perform the blkmod check
			 * ahead of the cycle check. There is a subtle chance that, if we do the check the other
			 * way round, the block may pass the cycle check (although it still has been modified)
			 * and just before the blkmod check, the buffer gets reused for another block whose block
			 * transaction number is less than the history's tn. The same care should be taken in
			 * all the other out-of-crit places which are (as of 1/15/2000)
			 * 	t_qread, gvcst_search, gvcst_put    --- nars.
			 *
			 * Note that the correctness of this out-of-crit-validation mechanism also relies on the
			 * fact that the block-transaction-number is modified before the contents of the block
			 * in gvcst_blk_build and other places in bg_update and mm_update. This is needed because
			 * our decision here is based on looking at the transaction number in the block-header
			 * rather than the BT table (done in tp_tend and t_end). If the block-tn got updated
			 * after the block contents get modified, then we may incorrectly decide (looking at the
			 * tn) that the block hasn't changed when actually its contents have.
			 */
			chain = *(off_chain *)&blk;
			if (!chain.flag)
			{
				/* We need to ensure the shared copy hasn't changed since the beginning of the
				 * transaction since not checking that can cause atleast false UNDEFs.
				 * e.g. Say earlier in this TP transaction we had gone down
				 * a level-1 root-block 4 (say) to reach leaf-level block 10 (say) to get at a
				 * particular key (^x). Let reorg have swapped the contents of block 10 with the
				 * block 20. Before the start of this particular mini-action in TP, block 4 would
				 * lead to block 20 (for the same key ^x). If block 20 happens to be in the cw_set
				 * of this region for this transaction, we would be using that instead of the shared
				 * memory copy. Now, the private copy of block 20 would not contain the key ^x. But
				 * we shouldn't incorrectly conclude that ^x is not present in the database. For this
				 * we should check whether the shared memory copy of block 20 is still not changed.
				 */
				assert(is_mm || t1->cr || t1->first_tp_srch_status);
				ASSERT_IS_WITHIN_TP_HIST_ARRAY_BOUNDS(t1->first_tp_srch_status, si);
				t2 = t1->first_tp_srch_status ? t1->first_tp_srch_status : t1;
				/* Note that we are comparing transaction numbers directly from the buffer instead of
				 * going through the bt or blk queues. This is done to speed up processing. But the effect
				 * is that we might encounter a situation where the buffer's contents hasn't been modified,
				 * but the block might actually have been changed i.e. in VMS a twin buffer might have been
				 * created or the "blk" field in the cache-record corresponding to this buffer might have
				 * been made CR_BLKEMPTY etc. In these cases, we rely on the fact that the cycle for the
				 * buffer would have been incremented thereby saving us in the cycle check later.
				 */
				if (t2->tn <= ((blk_hdr_ptr_t)t2->buffaddr)->tn)
				{
					assert(CDB_STAGNATE > t_tries);
					if (tprestart_syslog_delta)
					{
						n_blkmods++;
						if (t2->ptr || t1->ptr)
							n_pvtmods++;
						if (1 != n_blkmods)
							continue;
					}
					TP_TRACE_HIST_MOD(t2->blk_num, t2->blk_target, tp_blkmod_tp_hist,
						cs_addrs->hdr, t2->tn, ((blk_hdr_ptr_t)t2->buffaddr)->tn, t2->level);
					status = cdb_sc_blkmod;
					BREAK_IN_PRO__CONTINUE_IN_DBG;
				}
				if (t2->cr)
				{
					assert((sm_long_t)GDS_REL2ABS(t2->cr->buffaddr) == (sm_long_t)t2->buffaddr);
					if (t2->cycle != t2->cr->cycle)
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_lostcr;
						break;
					}
					t2->cr->refer = TRUE;
				}
			}
			if (store_history)
			{
				assert(si->cw_set_depth || !t1->ptr);
				local_hash_entry = t1->first_tp_srch_status;
				ASSERT_IS_WITHIN_TP_HIST_ARRAY_BOUNDS(local_hash_entry, si);
				/* Note that blocks created within the transaction (chain.flag != 0) are counted
				 * in the num_of_blks calculation since the created block still uses up a buffer at
				 * commit time and we have a limit on the maximum number of buffers that a
				 * TP transaction can have in its read-set (although it is a huge value of 64K
				 * even for BG due to the recent big-read TP changes).
				 *
				 * Also note that created and existing blocks are added in the hashtable
				 * "sgm_info_ptr->blks_in_use". Since the add_hashtab_int4() routine expects
				 * a pointer to the srch_blk_status element in the first_tp_hist array
				 * where we are going to copy over the block's history, and since we want to have
				 * common code wherever possible for both created and existing blocks, we pass in
				 * "si->last_tp_hist" in both cases. For an existing block, we actually
				 * copy its history into last_tp_hist. For a created block, we don't since it
				 * doesn't need to be validated at commit. This creates a dangling pointer from the
				 * created block's hashtable entry into last_tp_hist. Note that the next existing
				 * block which needs to be added into the hashtable will also have its hashtable
				 * entry pointing to the same "last_tp_hist". We now have two pointers to the same
				 * location, the first of which (that of the created block) is not valid.
				 * Still we are safe because any future t_qread done on the created block will
				 * return only a NULL value in the "first_tp_srch_status" member of the block's
				 * srch_blk_status. We assert this below. It is only because of this (that a created
				 * block's first_tp_srch_status member will be NULL even though it may be added
				 * already) that we need to do a check of the return value of add_hashtab_int4().
				 * Otherwise, we should be guaranteed that add_hashtab_int4() always returns FALSE.
				 * We assert this in the else block.
				 */
				assert(!chain.flag || !local_hash_entry);
				if (!local_hash_entry && add_hashtab_int4(si->blks_in_use, (uint4 *)&blk,
									(void *)(si->last_tp_hist), &tabent))
				{	/* not a duplicate block */
					if (++si->num_of_blks > si->tp_hist_size)
					{	/* catch the case where MUPIP recover or update process gets into this situation */
						assert(!mupip_jnl_recover && !is_updproc);
						rts_error(VARLSTCNT(4) ERR_TRANS2BIG, 2, REG_LEN_STR(gv_cur_region));
					}
					if (!chain.flag)
					{
						/* Either history has a clue or not.
						 * If yes, then it could have been constructed in an earlier
						 * 		local_tn or the current local_tn.
						 *	If an earlier local_tn, then the logic in gvcst_search would
						 *		have nullified first_tp_srch_status members.
						 * 	If the current local_tn, then this block should have been
						 *		added in the hashtable already and we shouldn't be
						 *		in this part of the code.
						 * If no, then we t_qread all the blocks in the search path. The
						 *		blocks may have been read already in this local_tn or not.
						 *	If already read in this local_tn, then this block should have
						 *		been added in the hashtable already and we shouldn't be
						 *		in this part of the code.
						 *	If not already read in this local_tn, then t_qread of this block
						 *		should have returned a NULL for first_tp_srch_status.
						 * In either case, the first_tp_srch_status member should be NULL.
						 */
						assert(NULL == t1->first_tp_srch_status);
						/* If history array is full, allocate more */
						if (si->last_tp_hist - si->first_tp_hist == si->cur_tp_hist_size)
							gds_tp_hist_moved(si, hist1);
						memcpy(si->last_tp_hist, t1, sizeof(srch_blk_status));
						t1->first_tp_srch_status = si->last_tp_hist;
						si->last_tp_hist++;
						/* Ensure that we are doing an M-kill if for a non-isolated global we end up
						 *	adding to the history a leaf-level block which has no corresponding cse.
						 * To be more specific we are doing an M-kill that freed up the data block from
						 * 	the B-tree, but that checking involves more coding.
						 * The only exception to this is if we are in an M-set and the set was a duplicate
						 *	set. Since we do not have that information (gvcst_put the caller has it),
						 *	we do a less stringent check and ensure the optimization ("gvdupsetnoop"
						 *	global variable) is turned on at the very least.
						 */
						assert(!t1->blk_target->noisolation || t1->level || t1->ptr ||
							((ERR_GVKILLFAIL == t_err)
								|| ((ERR_GVPUTFAIL == t_err) && gvdupsetnoop)));
					}
				} else
				{	/* While it is always true that in this part of the code atmost one of
					 * local_hash_entry and chain.flag will be non-NULL, it is almost always
					 * true that exactly one of them is non-NULL. There is a very rare case
					 * when both of them can be NULL. That is when two histories are passed
					 * each containing the same block. In that case, when the loop is executed
					 * for the first history, the block would have been added into the hashtable.
					 * During the second history's loop, the first_tp_srch_status member in
					 * the srch_blk_status for the second history will be NULL although the
					 * block is already in the hashtable. But that can be only because of a
					 * call from gvcst_kill() or one of the $order,$data,$query routines. Among
					 * these, gvcst_kill() is the only one that can create a condition which
					 * will require us to update the "ptr" field in the first_tp_hist array
					 * (i.e. the block may have a null "ptr" field in the first_tp_hist array
					 * when it would have been modified as part of the kill in which case we
					 * need to modify the first_tp_hist array appropriately). In that
					 * rare case (i.e. local_hash_entry is NULL), we don't need to change
					 * anything in the first_tp_hist array anyway since we would have updated
					 * it during the previous for-loop for the first history. Hence the check
					 * for if (local_hash_entry) below.
					 */
					assert(!local_hash_entry || !chain.flag);
					assert(chain.flag || local_hash_entry 	/* ensure we don't miss updating "ptr" */
						|| ((srch_blk_status *)(tabent->value)) || !t1->ptr);
					assert(!local_hash_entry || !local_hash_entry->ptr || !t1->ptr ||
						t1->ptr == local_hash_entry->ptr  ||
						t1->ptr->low_tlevel == local_hash_entry->ptr);
					if (local_hash_entry && t1->ptr)
					{
						assert(is_mm || local_hash_entry->ptr ||
								t1->first_tp_srch_status == local_hash_entry
									&& t1->cycle == local_hash_entry->cycle
									&& t1->cr == local_hash_entry->cr
									&& t1->buffaddr == local_hash_entry->buffaddr);
						local_hash_entry->ptr = t1->ptr;
					}
				}
			}
			t1->ptr = NULL;
		}
	}
	gv_target->read_local_tn = local_tn;
	CWS_RESET;
	return status;
}

void	gds_tp_hist_moved(sgm_info *si, srch_hist *hist1)
{
	gv_namehead		*gvnh;
	ht_ent_int4 		*tabent, *topent;
	sm_long_t               delta;
	srch_blk_status 	*new_first_tp_hist, *t1;
	tlevel_info		*tli;
	srch_blk_status		*srch_stat;

	assert(si->cur_tp_hist_size < si->tp_hist_size);
	si->cur_tp_hist_size <<= 1;
	new_first_tp_hist = (srch_blk_status *)malloc(sizeof(srch_blk_status) * si->cur_tp_hist_size);
	longcpy((uchar_ptr_t)new_first_tp_hist, (uchar_ptr_t)si->first_tp_hist,
		(sm_uc_ptr_t)si->last_tp_hist - (sm_uc_ptr_t)si->first_tp_hist);
	delta = (sm_uc_ptr_t)new_first_tp_hist - (sm_uc_ptr_t)si->first_tp_hist;
	for (tabent = si->blks_in_use->base, topent = si->blks_in_use->top; tabent < topent; tabent++)
	{
		if (HTENT_VALID_INT4(tabent, srch_blk_status, srch_stat))
		{
			REPOSITION_PTR(srch_stat, srch_blk_status, delta, si->first_tp_hist, si->last_tp_hist);
			tabent->value = (void *)srch_stat;
		}
	}
	for (tli = si->tlvl_info_head; tli; tli = tli->next_tlevel_info)
		REPOSITION_PTR_IF_NOT_NULL(tli->tlvl_tp_hist_info, srch_blk_status, delta, si->first_tp_hist, si->last_tp_hist);
	for (gvnh = gv_target_list; NULL != gvnh; gvnh = gvnh->next_gvnh)
	{
		/* Bypass gv_targets not used in this transaction. Note that the gvnh == gv_target check done below is
		 * to take care of the case where gv_target's read_local_tn is not yet local_tn although it will be, at the end
		 * of the current invocation of tp_hist. In this case too gv_target's pointers need to be repositioned.
		 */
		if (gvnh->read_local_tn != local_tn && gvnh != gv_target && gvnh != cs_addrs->dir_tree)
			continue;		/* Bypass gv_targets not used in this transaction */
		if (gvnh->gd_reg == si->gv_cur_region)
		{	/* reposition pointers only if global is of current region */
			for (t1 = &gvnh->hist.h[0]; t1->blk_num; t1++)
				REPOSITION_PTR_IF_NOT_NULL(t1->first_tp_srch_status, struct srch_blk_status_struct,
								delta, si->first_tp_hist, si->last_tp_hist);
		} else
			assert((gvnh != gv_target) && (gvnh != cs_addrs->dir_tree));
	}
	assert(NULL == hist1 || hist1 != &gv_target->hist);		/* ensure that gv_target isn't doubly repositioned */
	if (NULL != hist1 && hist1 != &cs_addrs->dir_tree->hist)		/* ensure don't reposition directory tree again */
	{
		for (t1 = &hist1->h[0]; t1->blk_num; t1++)
			REPOSITION_PTR_IF_NOT_NULL(t1->first_tp_srch_status, struct srch_blk_status_struct,
									delta, si->first_tp_hist, si->last_tp_hist);
	}
	free(si->first_tp_hist);
	si->first_tp_hist = new_first_tp_hist;
	si->last_tp_hist = (srch_blk_status *)((sm_uc_ptr_t)si->last_tp_hist + delta);
}
