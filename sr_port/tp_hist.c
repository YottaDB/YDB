/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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
#include "memcoherency.h"
#include "wbox_test_init.h"

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


GBLREF	gv_namehead	*gv_target, *gv_target_list;
GBLREF	sgm_info	*sgm_info_ptr;
GBLREF	gd_region	*gv_cur_region;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	trans_num	local_tn;	/* transaction number for THIS PROCESS */
GBLREF	int4		n_pvtmods, n_blkmods;
GBLREF	unsigned int	t_tries;
GBLREF	uint4		t_err;
GBLREF	int4		tprestart_syslog_delta;
GBLREF	boolean_t	is_updproc;
GBLREF	boolean_t	mupip_jnl_recover;
GBLREF	boolean_t	gvdupsetnoop; /* if TRUE, duplicate SETs update journal but not database (except for curr_tn++) */
GBLREF	uint4		process_id;

#ifdef DEBUG
GBLREF	uint4		donot_commit;	/* see gdsfhead.h for the purpose of this debug-only global */
GBLREF	boolean_t	ready2signal_gvundef;	/* TRUE if GET operation is about to signal a GVUNDEF */
GBLREF	boolean_t	gtm_gvundef_fatal;
#endif

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
	ht_ent_int4	*tabent, *lookup_tabent;
	cw_set_element	*cse;
	cache_rec_ptr_t	cr;
#	ifdef DEBUG
	boolean_t	wc_blocked;
	boolean_t	ready2signal_gvundef_lcl;
#	endif

	error_def(ERR_TRANS2BIG);
	error_def(ERR_GVKILLFAIL);
	error_def(ERR_GVPUTFAIL);

	DEBUG_ONLY(
		/* Store global variable ready2signal_gvundef in a local variable and reset the global right away to ensure that
		 * the global value does not incorrectly get carried over to the next call of "t_end".
		 */
		ready2signal_gvundef_lcl = ready2signal_gvundef;
		ready2signal_gvundef = FALSE;
	)
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
	/* We are going to reference fields from shared memory including cr->in_tend and t1->buffaddr->tn in that order (as part
	 * of the TP_IS_CDB_SC_BLKMOD macro below). To ensure we read an uptodate value of cr->in_tend, we do a read memory
	 * barrier here. Note that to avoid out-of-order reads, the read memory barrier needs to be done AFTER the reference
	 * that sets t1->tn (in a caller function usually gvcst_search.c) and the reference that accesses t1->buffaddr->tn
	 * (in this function below as part of the TP_IS_CDB_SC_BLKMOD macro). Therefore it is ok to do this at the start of
	 * this function. If cr->in_tend changes concurrently AFTER we do the read memory barrier and we dont see the change
	 * in the cr->in_tend reference below, then, as long as the values we see pass the cdb_sc_blkmod check, it is ok to
	 * consider this transaction as good because whatever change happened concurrently occurred only after we reached this
	 * part of the code which means that while the caller functions gvcst_put.c,gvcst_search.c etc. were traversing the
	 * tree and accessing the buffers, they were guaranteed to have seen a consistent state of the database.
	 */
	SHM_READ_MEMORY_BARRIER;
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
			{	/* We need to ensure the shared copy hasn't changed since the beginning of the
				 * transaction since not checking that can cause at least false UNDEFs.
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
				cr = t2->cr;
				/* Assert that cr->in_tend is never equal to our process_id since at this point we should
				 * never have locked a buffer for phase2 commit. The only exception is if the previous
				 * transaction by this process had a commit-time error and secshr_db_clnup finished the
				 * transaction and had set csd->wc_blocked to TRUE. It is possible in that case no process
				 * has yet done a cache-recovery by the time we come to tp_hist as part of the next transaction
				 * in which case cr->in_tend could still be pointing to our process_id. Note that
				 * csd->wc_blocked could be changing concurrently so need to note it down in a local variable
				 * BEFORE checking the value of cr->in_tend.
				 */
				DEBUG_ONLY(wc_blocked = cs_addrs->hdr->wc_blocked;)
				assert((NULL == cr) || (process_id != cr->in_tend) || wc_blocked);
				if (TP_IS_CDB_SC_BLKMOD(cr, t2))
				{
					cse = t1->cse;
					/* Check if the cse already has a recompute list (i.e. NOISOLATION turned ON)
					 * If so no need to restart the transaction even though the block changed.
					 * We will detect this change in tp_tend and recompute the update array anyways.
					 */
					if ((NULL == cse) || !cse->recompute_list_head || cse->write_type)
					{
						assert((CDB_STAGNATE > t_tries)
							|| (gtm_white_box_test_case_enabled
							    && (WBTEST_TP_HIST_CDB_SC_BLKMOD == gtm_white_box_test_case_number)));
						if (tprestart_syslog_delta)
						{
							n_blkmods++;
							if (t2->cse || t1->cse)
								n_pvtmods++;
							if (1 != n_blkmods)
								continue;
						}
						TP_TRACE_HIST_MOD(t2->blk_num, t2->blk_target, tp_blkmod_tp_hist,
							cs_addrs->hdr, t2->tn, ((blk_hdr_ptr_t)t2->buffaddr)->tn, t2->level);
						status = cdb_sc_blkmod;
						BREAK_IN_PRO__CONTINUE_IN_DBG;
					}
#					ifdef DEBUG
					else
					{	/* Assert that this is a leaf level block of a global with NOISOLATION
						 * and that this is a SET operation (not a GET or KILL). The only exception
						 * we know of to this is that the global corresponding to the incoming history
						 * (t1) could have NOISOLATION turned OFF but yet contain a block which is
						 * already part of the first_tp_hist structure (t2) for a different global that
						 * has NOISOLATION turned ON. This is possible if MUPIP REORG swapped the block
						 * AFTER the tp_hist was done for the NOISOLATION-ON-global but BEFORE the
						 * tp_hist (the current one) for the NOISOLATION-OFF-global. This is definitely
						 * a restartable situation that will be detected in tp_tend. Even though
						 * we know t1->blk_target != t2->blk_target in this case, we choose to defer
						 * the restart because we suspect this case is very infrequent that it is better
						 * to avoid the performance hit of a check in this frequently used code.
						 */
						assert((ERR_GVPUTFAIL == t_err) && (0 == t1->level)
							&& (t1->blk_target->noisolation || t2->blk_target->noisolation));
						if (t1->blk_target != t2->blk_target)
							donot_commit |= DONOTCOMMIT_TPHIST_BLKTARGET_MISMATCH;
					}
#					endif
				}
				/* Although t1->first_tp_srch_status (i.e. t2) is used for doing blkmod check,
				 * we need to use BOTH t1 and t1->first_tp_srch_status to do the cdb_sc_lostcr check.
				 *
				 * Need to use t1 for the cdb_sc_lostcr check
				 * -------------------------------------------
				 * We allow for recycled crs within a transaction but not within a mini-action (one that
				 * ends up calling tp_hist e.g. SET/KILL/GET etc.). An example of how recycled crs within
				 * a mini-action are possible is the M-kill. It does gvcst_search twice one for the left
				 * path and one for the right path that identifies the subtree of blocks to be killed.
				 * It is possible that the same block gets included in both the paths but with different
				 * state of the "cr" implying a block recycle within the KILL and hence the need for a restart.
				 *
				 * Need to also use t1->first_tp_srch_status (i.e. t2) for the cdb_sc_lostcr check
				 * --------------------------------------------------------------------------------
				 * tp_tend does not validate those blocks that have already been built privately
				 * (i.e. cse->new_buff is non-NULL). Instead it relies on tp_hist (called for every mini-action)
				 * to have done the validation right after the block was built. Hence we need to validate
				 * t1->first_tp_srch_status (right now) against the current state of the cache to ensure whatever
				 * shared memory buffer we used to privately build the block has not been recycled since.
				 */
				if ((t1 != t2) && t1->cr)
				{
					assert((sm_long_t)GDS_REL2ABS(t1->cr->buffaddr) == (sm_long_t)t1->buffaddr);
					if (t1->cycle != t1->cr->cycle)
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_lostcr;
						break;
					}
					t1->cr->refer = TRUE;
				}
				if (cr)
				{
					assert((sm_long_t)GDS_REL2ABS(cr->buffaddr) == (sm_long_t)t2->buffaddr);
					if (t2->cycle != cr->cycle)
					{
						assert(CDB_STAGNATE > t_tries);
						status = cdb_sc_lostcr;
						break;
					}
					cr->refer = TRUE;
				}
			}
			/* Note that blocks created within the transaction (chain.flag != 0) are NOT counted
			 * in the read-set of this transaction. They are still counted against the write-set.
			 * si->num_of_blks is the count of the read-set (the si->blks_in_use has the actual block numbers)
			 * while si->cw_set_depth is the count of the write-set (si->first_cw_set has the block numbers).
			 * We have a limit on the maximum number of buffers that a TP transaction can have both in
			 * its read-set (64K buffers) and write-set (1/2 the number of global buffers). The former
			 * limit check is done here. The latter is done in tp_cw_list.c.
			 *
			 * Since created blocks are not added into the hashtable, we expect the "first_tp_srch_status"
			 * members of those block's srch_blk_status to be NULL. That is asserted below.
			 *
			 * For an existing block, we copy its search history into si->last_tp_hist.
			 * For a created block, we dont since it doesn't need to be validated at commit.
			 */
			assert(!chain.flag || !t1->first_tp_srch_status);
			if (store_history && !chain.flag)
			{
				assert(si->cw_set_depth || !t1->cse);
				local_hash_entry = t1->first_tp_srch_status;
				DEBUG_ONLY(lookup_tabent = lookup_hashtab_int4(si->blks_in_use, (uint4 *)&blk);)
				ASSERT_IS_WITHIN_TP_HIST_ARRAY_BOUNDS(local_hash_entry, si);
				if ((NULL == local_hash_entry) && add_hashtab_int4(si->blks_in_use, (uint4 *)&blk,
									(void *)(si->last_tp_hist), &tabent))
				{	/* not a duplicate block */
					assert(NULL == lookup_tabent);
					if (++si->num_of_blks > si->tp_hist_size)
					{	/* catch the case where MUPIP recover or update process gets into this situation */
						assert(!mupip_jnl_recover && !is_updproc);
						rts_error(VARLSTCNT(4) ERR_TRANS2BIG, 2, REG_LEN_STR(gv_cur_region));
					}
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
					assert(!t1->blk_target->noisolation || t1->level || t1->cse
						|| ((ERR_GVKILLFAIL == t_err)
							|| ((ERR_GVPUTFAIL == t_err) && gvdupsetnoop)));
				} else
				{	/* While it is almost always true that local_hash_entry is non-NULL here, there
					 * is a very rare case when it can be NULL. That is when two histories are passed
					 * each containing the same block. In that case, when the loop is executed
					 * for the first history, the block would have been added into the hashtable.
					 * During the second history's loop, the first_tp_srch_status member in
					 * the srch_blk_status for the second history will be NULL although the
					 * block is already in the hashtable. But that can be only because of a
					 * call from gvcst_kill() or one of the $order,$data,$query routines. Among
					 * these, gvcst_kill() is the only one that can create a condition which
					 * will require us to update the "cse" field in the first_tp_hist array
					 * (i.e. the block may have a null "cse" field in the first_tp_hist array
					 * when it would have been modified as part of the kill in which case we
					 * need to modify the first_tp_hist array appropriately). In that
					 * rare case (i.e. local_hash_entry is NULL), we don't need to change
					 * anything in the first_tp_hist array anyway since we would have updated
					 * it during the previous for-loop for the first history. Hence the check
					 * for if (local_hash_entry) below.
					 */
					/* Ensure that "first_tp_srch_status" reflects what is in the hash-table.
					 * The only exception is when two histories are passed (explained above)
					 */
					assert(local_hash_entry && lookup_tabent && (local_hash_entry == lookup_tabent->value)
						|| !local_hash_entry && (hist == hist1));
					/* Ensure we don't miss updating "cse" */
					assert((NULL != local_hash_entry) || ((srch_blk_status *)(tabent->value))->cse|| !t1->cse);
					assert(!local_hash_entry || !local_hash_entry->cse || !t1->cse ||
						t1->cse == local_hash_entry->cse  ||
						t1->cse->low_tlevel == local_hash_entry->cse);
					if (local_hash_entry && t1->cse)
					{
						assert(is_mm || local_hash_entry->cse
								|| t1->first_tp_srch_status == local_hash_entry
									&& t1->cycle == local_hash_entry->cycle
									&& t1->cr == local_hash_entry->cr
									&& t1->buffaddr == local_hash_entry->buffaddr);
						local_hash_entry->cse = t1->cse;
					}
				}
			}
			t1->cse = NULL;
		}
#		ifdef DEBUG
		/* Now that this history has been successfully validated, check that the current history transaction
		 * numbers can never be older than their corresponding first_tp_srch_status values.
		 */
		if (cdb_sc_normal == status)
		{
			for (t1 = hist->h; HIST_TERMINATOR != (blk = t1->blk_num); t1++)
			{
				t2 = t1->first_tp_srch_status ? t1->first_tp_srch_status : t1;
				assert(t2->tn <= t1->tn);
				/* Take this time to also check that gv_targets match between t1 and t2.
				 * If they dont, the restart should be detected in tp_tend.
				 * Set the flag donot_commit to catch the case this does not restart.
				 */
				if (t2->blk_target != t1->blk_target)
					donot_commit |= DONOTCOMMIT_TPHIST_BLKTARGET_MISMATCH;
			}
		}
#		endif
	}
	/* If validation has succeeded, assert that if gtm_gvundef_fatal is non-zero, then we better not signal a GVUNDEF */
	assert((cdb_sc_normal != status) || !gtm_gvundef_fatal || !ready2signal_gvundef_lcl);
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
	sgmnt_addrs		*csa;

	assert(si->cur_tp_hist_size < si->tp_hist_size);
	si->cur_tp_hist_size <<= 1;
	new_first_tp_hist = (srch_blk_status *)malloc(sizeof(srch_blk_status) * si->cur_tp_hist_size);
	longcpy((uchar_ptr_t)new_first_tp_hist, (uchar_ptr_t)si->first_tp_hist,
		(sm_uc_ptr_t)si->last_tp_hist - (sm_uc_ptr_t)si->first_tp_hist);
	delta = (sm_long_t)((sm_uc_ptr_t)new_first_tp_hist - (sm_uc_ptr_t)si->first_tp_hist);
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
	csa = si->tp_csa;
	for (gvnh = gv_target_list; NULL != gvnh; gvnh = gvnh->next_gvnh)
	{
		/* Bypass gv_targets not used in this transaction. Note that the gvnh == gv_target check done below is
		 * to take care of the case where gv_target's read_local_tn is not yet local_tn although it will be, at the end
		 * of the current invocation of tp_hist. In this case too gv_target's pointers need to be repositioned.
		 */
		if (gvnh->read_local_tn != local_tn && gvnh != gv_target && gvnh != cs_addrs->dir_tree)
			continue;		/* Bypass gv_targets not used in this transaction */
		if (gvnh->gd_csa == csa)
		{	/* reposition pointers only if global corresponds to current database file */
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
