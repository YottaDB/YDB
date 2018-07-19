/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
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

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "copy.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h and cws_insert.h */
#include "tp.h"
#include "gvcst_blk_build.h"
#include "t_qread.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "hashtab.h"		/* needed for cws_insert.h */
#include "cws_insert.h"
#include "gvcst_protos.h"	/* for gvcst_search_blk,gvcst_search_tail,gvcst_search prototype */
#include "min_max.h"
#include "gvcst_expand_key.h"

GBLREF	boolean_t		mu_reorg_process;
GBLREF	char			gvcst_search_clue;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_altkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	srch_blk_status		*first_tp_srch_status;	/* overriding value of srch_blk_status given by t_qread in case of TP */
GBLREF	trans_num		local_tn;		/* transaction number for THIS PROCESS */
GBLREF	uint4			dollar_tlevel;
GBLREF	unsigned char		rdfail_detail;
GBLREF	unsigned int		t_tries;

#define	SET_GVCST_SEARCH_CLUE(X)	gvcst_search_clue = X;

/**
 * Searches for pKey and records the path through the b-tree in pHist
 *
 * In some cases, also fills out gv_altkey. It also uses "clues" to optimize, and a large chunk
 *  of the code here is to handle the verification of the clues validity (~400 lines).
 *
 * @param[in] pKey The key search the database for
 * @param[out] pHist History buffer which will contain the list of records leading the pKey;
 * 	the first element ([0]) will contain the leaf node if NULL, clue information
 * 	is filled out in gv_target
 */
enum cdb_sc 	gvcst_search(gv_key *pKey,		/* Key to search for */
			     srch_hist *pHist)		/* History to fill in*/
{
	unsigned char		nLevl;
	enum cdb_sc		status;
	register int		n1;
	register uchar_ptr_t	c1, c2;
	register sm_uc_ptr_t	pRec, pBlkBase;
	register gv_namehead	*pTarg;	/* Local copy of gv_target;  hope it gets put into register */
	register srch_blk_status *pCurr;
	register srch_blk_status *pNonStar;
	register srch_hist	*pTargHist;
	int			tmp_cmpc;
	block_id		nBlkId;
	cache_rec_ptr_t		cr;
	int			cycle;
	unsigned short		n0, nKeyLen;
	trans_num		tn;
	cw_set_element		*cse;
	off_chain		chain1, chain2;
	srch_blk_status		*tp_srch_status, *srch_status, *leaf_blk_hist;
	boolean_t		already_built, expand_prev_key, is_mm, skip_search_blk;
	ht_ent_int4		*tabent;
	sm_uc_ptr_t		buffaddr;
	trans_num		blkhdrtn, oldest_hist_tn;
	int			hist_size;
	unsigned short		bsiz;
#	ifdef DEBUG
	boolean_t		save_donot_commit;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	pTarg = gv_target;
	assert(NULL != pTarg);
	assert(pTarg->root);
	assert(pKey != &pTarg->clue);
	nKeyLen = pKey->end + 1;

	assert(!dollar_tlevel || ((NULL != sgm_info_ptr) && (cs_addrs->sgm_info_ptr == sgm_info_ptr)));
	SET_GVCST_SEARCH_CLUE(0);
	INCR_DB_CSH_COUNTER(cs_addrs, n_gvcst_srches, 1);
	pTargHist = ((NULL == pHist) ? &pTarg->hist : pHist);
	expand_prev_key = TREF(expand_prev_key);
	/* If FINAL RETRY and TP then we can safely use clues of gv_targets that have been referenced in this
	 * TP transaction (read_local_tn == local_tn). While that is guaranteed to be true for all updates, it
	 * does not hold good for READs since we allow a lot more reads to be done inside a transaction compared
	 * to the # of updates allowed. We allow the same global to be read multiple times inside the same transaction
	 * using different global buffers for each read. This means that we need to validate any clues from the first
	 * read before using it for the second read even if it is in the final retry. This validation is done inside
	 * the below IF block. As for gv_targets which are referenced for the very first time in this TP transaction,
	 * we have no easy way of determining if their clues are still uptodate (i.e. using the clue will guarantee us
	 * no restart) and since we are in the final retry, we dont want to take a risk. So dont use the clue in that case.
	 *
	 * If FINAL RETRY and Non-TP, we will be dealing with only ONE gv_target so its clue would have been reset as
	 * part of the penultimate restart so we dont have any of the above issue in the non-tp case. The only exception
	 * is if we are in gvcst_kill in which case, gvcst_search will be called twice and the clue could be non-zero
	 * for the second invocation. In this case, the clue is guaranteed to be uptodate since it was set just now
	 * as part of the first invocation. So no need to do anything about clue in final retry for Non-TP.
	 */
	if ((0 != pTarg->clue.end) && ((CDB_STAGNATE > t_tries) || !dollar_tlevel || (pTarg->read_local_tn == local_tn)))
	{	/* Have non-zero clue. Check if it is usable for the current search key. If so validate clue then and use it. */
		/* In t_end, we skipped validating the clue in case of reorg due to the assumption that reorg never uses the clue
		 * i.e. it nullifies the clue before calling gvcst_search. However, it doesn't reset the clue for directory tree
		 * and so continue using the clue if called for root search. Assert accordingly.
		 */
		assert(!mu_reorg_process UNIX_ONLY(|| (pTarg->gd_csa->dir_tree == pTarg)));
		INCR_DB_CSH_COUNTER(cs_addrs, n_gvcst_srch_clues, 1);
		status = cdb_sc_normal;	/* clue is usable unless proved otherwise */
		DEBUG_ONLY(save_donot_commit = TREF(donot_commit);)
		if (NULL != pHist)
		{	/* Copy the full srch_hist and set loop terminator flag in unused srch_blk_status entry.
			 * If in TP and if leaf block in history has cse, we are guaranteed that it is built by the
			 * immediately previous call to "gvcst_search" (called by gvcst_kill which does two calls to
			 * gvcst_search of which this invocation is the second) so no need to build the block like
			 * is done for the (NULL == pHist) case below. Assert that and some more.
			 */
			hist_size = HIST_SIZE(pTarg->hist);
			memcpy(pHist, &pTarg->hist, hist_size);
			((srch_blk_status *)((char *)pHist + hist_size))->blk_num = 0;
#			ifdef DEBUG
			if (dollar_tlevel)
			{
				leaf_blk_hist = &pHist->h[0];
				assert(0 == leaf_blk_hist->level);
				chain1 = *(off_chain *)&leaf_blk_hist->blk_num;
				if (chain1.flag == 1)
				{
					assert((int)chain1.cw_index < sgm_info_ptr->cw_set_depth);
					tp_get_cw(sgm_info_ptr->first_cw_set, (int)chain1.cw_index, &cse);
				} else
				{
					tp_srch_status = leaf_blk_hist->first_tp_srch_status;
					ASSERT_IS_WITHIN_TP_HIST_ARRAY_BOUNDS(tp_srch_status, sgm_info_ptr);
					cse = (NULL != tp_srch_status) ? tp_srch_status->cse : NULL;
				}
				assert((NULL == cse) || cse->done);
			}
#			endif
		} else if (dollar_tlevel)
		{	/* First nullify first_tp_srch_status member in gv_target history if out-of-date. This is logically done
			 * at tp_clean_up time but delayed until the time this gv_target is used next in a transaction. This way
			 * it saves some CPU cycles. pTarg->read_local_tn tells us whether this is the first usage of this
			 * gv_target in this TP transaction and if so we need to reset the out-of-date field.
			 */
			if (pTarg->read_local_tn != local_tn)
				GVT_CLEAR_FIRST_TP_SRCH_STATUS(pTarg);
			/* TP & going to use clue. check if clue path contains a leaf block with a corresponding unbuilt
			 * cse from the previous traversal. If so build it first before gvcst_search_blk/gvcst_search_tail.
			 */
			tp_srch_status = NULL;
			leaf_blk_hist = &pTarg->hist.h[0];
			assert(leaf_blk_hist->blk_target == pTarg);
			assert(0 == leaf_blk_hist->level);
			chain1 = *(off_chain *)&leaf_blk_hist->blk_num;
			if (chain1.flag == 1)
			{
				if ((int)chain1.cw_index >= sgm_info_ptr->cw_set_depth)
				{
					assert(sgm_info_ptr->tp_csa == cs_addrs);
					assert(FALSE == cs_addrs->now_crit);
					return cdb_sc_blknumerr;
				}
				tp_get_cw(sgm_info_ptr->first_cw_set, (int)chain1.cw_index, &cse);
			} else
			{
				nBlkId = (block_id)leaf_blk_hist->blk_num;
				tp_srch_status = leaf_blk_hist->first_tp_srch_status;
				if ((NULL == tp_srch_status)
						&& (NULL != (tabent = lookup_hashtab_int4(sgm_info_ptr->blks_in_use,
											(uint4 *)&leaf_blk_hist->blk_num))))
					tp_srch_status = tabent->value;
				ASSERT_IS_WITHIN_TP_HIST_ARRAY_BOUNDS(tp_srch_status, sgm_info_ptr);
				cse = (NULL != tp_srch_status) ? tp_srch_status->cse : NULL;
			}
			assert(!cse || !cse->high_tlevel);
			if ((NULL == tp_srch_status) || (tp_srch_status->blk_target == pTarg))
			{	/* Either the leaf level block in clue is not already present in the current TP transaction's
				 * hashtable OR it is already present and the corresponding globals match. If they dont match
				 * we know for sure the clue is out-of-date (i.e. using it will lead to a transaction restart)
				 * and hence needs to be discarded.
				 */
				leaf_blk_hist->first_tp_srch_status = tp_srch_status;
				if (NULL != cse)
				{
					if (!cse->done)
					{	/* there's a private copy and it's not up to date */
						already_built = (NULL != cse->new_buff);
						gvcst_blk_build(cse, cse->new_buff, 0);
						/* Validate the block's search history right after building a private copy.
						 * This is not needed in case gvcst_search is going to reuse the clue's search
						 * history and return (because tp_hist will do the validation of this block).
						 * But if gvcst_search decides to do a fresh traversal (because the clue does not
						 * cover the path of the current input key etc.) the block build that happened now
						 * will not get validated in tp_hist since it will instead be given the current
						 * key's search history path (a totally new path) for validation. Since a private
						 * copy of the block has been built, tp_tend would also skip validating this block
						 * so it is necessary that we validate the block right here. Since it is tricky to
						 * accurately differentiate between the two cases, we do the validation
						 * unconditionally here (besides it is only a few if checks done per block build
						 * so it is considered okay performance-wise).
						 */
						if (!already_built && !chain1.flag)
						{	/* is_mm is calculated twice, but this is done so as to speed up the
							 * most-frequent path, i.e. when there is a clue and either no cse or
							 * cse->done is TRUE
							 */
							is_mm = (dba_mm == cs_data->acc_meth);
							buffaddr = tp_srch_status->buffaddr;
							cr = tp_srch_status->cr;
							assert(tp_srch_status && (is_mm || cr) && buffaddr);
							blkhdrtn = ((blk_hdr_ptr_t)buffaddr)->tn;
							if (TP_IS_CDB_SC_BLKMOD3(cr, tp_srch_status, blkhdrtn))
							{
								assert(CDB_STAGNATE > t_tries);
								assert(0 == leaf_blk_hist->level);
								assert(0 == tp_srch_status->level);
								TP_TRACE_HIST_MOD(leaf_blk_hist->blk_num, gv_target,
									tp_blkmod_gvcst_srch, cs_data, tp_srch_status->tn,
									blkhdrtn, leaf_blk_hist->level);
								return cdb_sc_blkmod;
							}
							if (!is_mm && ((tp_srch_status->cycle != cr->cycle)
										|| (tp_srch_status->blk_num != cr->blk)))
							{
								assert(CDB_STAGNATE > t_tries);
								return cdb_sc_lostcr;
							}
						}
						cse->done = TRUE;
						leaf_blk_hist->cr = 0;
						leaf_blk_hist->cycle = CYCLE_PVT_COPY;
						leaf_blk_hist->buffaddr = cse->new_buff;
					} else
					{	/* Keep leaf_blk_hist->buffaddr and cse->new_buff in sync if they are not already.
						 * They will be mostly in sync except for two cases.
						 * a) If a TP transaction updates two different globals and the second update
						 *	invoked t_qread (invoked from outside gvcst_search) for a leaf block
						 *	corresponding to the first global and ended up constructing a private
						 *	block. However, the transaction validation done in tp_hist/tp_tend
						 *	should detect this and restart.
						 * b) If a gvcst_kill happens in a TP transaction and it does a gvcst_search
						 *	call without going through tp_hist (GTM-8120). In this case there is no
						 *	restartable situation.
						 * Since it is hard to distinguish (a) from (b), we do not set the donot_commit
						 * variable to indicate this is a restartable situation in case of just (a).
						 */
						leaf_blk_hist->buffaddr = cse->new_buff; /* sync the buffers in pro, just in case */
					}
				}
			} else
			{	/* Two different gv_targets point to same block; discard out-of-date clue. */
#				ifdef DEBUG
				if ((pTarg->read_local_tn >= local_tn) && (NULL != leaf_blk_hist->first_tp_srch_status))
				{	/* Since the clue was used in *this* transaction, it cannot successfully complete. Set
					 * donot_commit to verify that a restart happens (either in tp_hist or tp_tend)
					 */
					assert(pTarg->read_local_tn == local_tn);
					TREF(donot_commit) |= DONOTCOMMIT_GVCST_SEARCH_BLKTARGET_MISMATCH;
				}
#				endif
				status = cdb_sc_lostcr;
			}
		}
		/* Validate EVERY level in the clue before using it for ALL retries. This way we avoid unnecessary restarts.
		 * This is NECESSARY for the final retry (e.g. in a TP transaction that does LOTS of reads of different globals,
		 * it is possible that one global's clue is invalidated by a later read of another global) and is DESIRABLE (for
		 * performance reasons) in the other tries. The cost of a restart (particularly in TP) is very high that it is
		 * considered okay to take the hit of validating the entire clue before using it even if it is not the final retry.
		 */
		if (cdb_sc_normal == status)
		{
			is_mm = (dba_mm == cs_data->acc_meth);
			if (!is_mm)
				oldest_hist_tn = OLDEST_HIST_TN(cs_addrs);
			for (srch_status = &pTargHist->h[0]; HIST_TERMINATOR != srch_status->blk_num; srch_status++)
			{	/* Do the actual verification of each history block */
				assert(srch_status->level == srch_status - &pTargHist->h[0]);
				assert(is_mm || (NULL == srch_status->cr) || (NULL != srch_status->buffaddr));
				cr = srch_status->cr;
				assert(!is_mm || (NULL == cr));
				if (TP_IS_CDB_SC_BLKMOD(cr, srch_status))
				{
					status = cdb_sc_blkmod;
					break;
				}
				if (NULL != cr)
				{
					assert(NULL != srch_status->buffaddr);
					if (srch_status->cycle != cr->cycle)
					{
						status = cdb_sc_lostcr;
						break;
					}
					if ((CDB_STAGNATE <= t_tries) || mu_reorg_process)
					{
						CWS_INSERT(cr->blk);
						if ((CDB_STAGNATE <= t_tries) && (srch_status->tn <= oldest_hist_tn))
						{	/* The tn at which the history was last validated is before the earliest
							 * transaction in the BT. The clue can no longer be relied upon.
							 */
							status = cdb_sc_losthist;
							break;
						}
					}
					cr->refer = TRUE;
				}
			}
		}
		if (cdb_sc_normal == status)
		{	/* Now that we are ready to use the clue, put more-likely case earlier in the if then else sequence.
			 * For sequential reads of globals, we expect the tail of the clue to be much more used than the head.
			 * For random reads, both are equally probable and hence it doesn't matter.
			 * The case (0 == n1) is not expected a lot (relatively) since the application may be able to optimize
			 *	a number of reads of the same key into one read by using a local-variable to store the value.
			 */
			if (0 < (n1 = memcmp(pKey->base, pTarg->clue.base, nKeyLen)))
			{
				if (memcmp(pKey->base, pTarg->last_rec->base, nKeyLen) <= 0)
				{
					SET_GVCST_SEARCH_CLUE(1);
					status = gvcst_search_tail(pKey, pTargHist->h, &pTarg->clue);
					if (cdb_sc_normal == status)
					{
						if (NULL == pHist)
						{	/* Implies the search history is being filled in pTarg->hist so we can
							 * safely update pTarg->clue to reflect the new search key. It is important
							 * that this clue update be done AFTER the gvcst_search_tail invocation
							 * (as that needs to pass the previous clue key).
							 */
							COPY_CURR_AND_PREV_KEY_TO_GVTARGET_CLUE(pTarg, pKey, expand_prev_key);
						}
						INCR_DB_CSH_COUNTER(cs_addrs, n_clue_used_tail, 1);
						return cdb_sc_normal;
					}
					/* Else clue is not usable. Fall through to do full traversal. */
				}
			} else if (0 > n1)
			{
				if (memcmp(pKey->base, pTarg->first_rec->base, nKeyLen) >= 0)
				{
					SET_GVCST_SEARCH_CLUE(3);
					status = gvcst_search_blk(pKey, pTargHist->h);
					if (cdb_sc_normal == status)
					{
						if (NULL == pHist)
						{	/* Implies the search history is being filled in pTarg->hist so we can
							 * safely update pTarg->clue to reflect the new search key. It does not
							 * matter if we update the clue BEFORE or AFTER the gvcst_search_blk
							 * invocation but for consistency with the gvcst_search_tail invocation
							 * we keep it AFTER.
							 */
							COPY_CURR_AND_PREV_KEY_TO_GVTARGET_CLUE(pTarg, pKey, expand_prev_key);
						}
						INCR_DB_CSH_COUNTER(cs_addrs, n_clue_used_head, 1);
						return cdb_sc_normal;
					}
					/* Else clue is not usable. Fall through to do full traversal. */
				}
			} else
			{
				SET_GVCST_SEARCH_CLUE(2);
				/* If $zprevious is the current operation, and we have a clue based on a prior search but
				 * dont have a prev_key computed as part of that search (because it was not a $zprevious
				 * operation as well), we cannot use the clue. Instead we need to fall through.
				 */
				if (!expand_prev_key)
				{
					INCR_DB_CSH_COUNTER(cs_addrs, n_clue_used_same, 1);
					return cdb_sc_normal;
				}
				if ((NULL != pTarg->prev_key) && (PREV_KEY_NOT_COMPUTED != pTarg->prev_key->end))
				{
					COPY_KEY(gv_altkey, pTarg->prev_key);
					INCR_DB_CSH_COUNTER(cs_addrs, n_clue_used_same, 1);
					return cdb_sc_normal;
				}
				leaf_blk_hist = &pTarg->hist.h[0];
				status = gvcst_expand_prev_key(leaf_blk_hist, &pTarg->clue, gv_altkey);
				if (cdb_sc_normal == status)
				{
					COPY_PREV_KEY_TO_GVT_CLUE(pTarg, TRUE);
					/* gv_altkey already contains the prev_key so no need to do the following.
					 *	COPY_KEY(gv_altkey, pTarg->prev_key);
					 */
					INCR_DB_CSH_COUNTER(cs_addrs, n_clue_used_same, 1);
					return cdb_sc_normal;
				}
				/* Else clue is not usable. Fall through to do full traversal. */
			}
		}
#		ifdef DEBUG
		/* If we are not going to use this clue (i.e. going to read the entire GVT afresh),
		 * restore global variable donot_commit in case it got set to a non-zero value above.
		 * This is needed because we are no longer relying on the out-of-date (and restart causing) clue.
		 */
		TREF(donot_commit) = save_donot_commit;
#		endif
	}
	nBlkId = pTarg->root;
	tn = cs_addrs->ti->curr_tn;
	if (NULL == (pBlkBase = t_qread(nBlkId, (sm_int_ptr_t)&cycle, &cr)))
		return (enum cdb_sc)rdfail_detail;
	nLevl = ((blk_hdr_ptr_t)pBlkBase)->levl;
	if (MAX_BT_DEPTH < (int)nLevl)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_maxlvl;
	}
	if (0 == (int)nLevl)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_badlvl;
	}
	assert(FALSE != ZPREVIOUS_NULL_SUBS_LEVEL1);
	assert(TRUE != ZPREVIOUS_NULL_SUBS_LEVEL1);
	assert((FALSE == expand_prev_key) || (TRUE == expand_prev_key) || (ZPREVIOUS_NULL_SUBS_LEVEL1 == expand_prev_key));
	/* If we are inside a $zprevious(^gblname("")) operation, gv_currkey has already been set to correspond to the highest
	 * first subscript in the GVT. Therefore we are guaranteed that the highest existing first subscript can be found by
	 * traversing down the rightmost path in the GVT i.e. descending down the *-records (last record) of index blocks.
	 * And so we can go directly to the last record (*-record) in each index block as we descend down the tree and do a
	 * "gvcst_search_blk" only for the leaf block. Set "skip_search_blk" accordingly. Note that this optimization means
	 * pCurr->prev_rec.offset and pCurr->prev_rec.match would be set to a special value PREV_REC_UNINITIALIZED for index
	 * blocks (i.e. pCurr->level > 0). If a caller of this function later needs to access prev_rec.match or prev_rec.offset
	 * for an index block (e.g. gvcst_put as part of a block-split operation), they need to do the "gvcst_search_blk" call
	 * then. This basically defers the call as much as possible and therefore can perform better in case the caller does not
	 * need prev_rec for index blocks.
	 */
	skip_search_blk = (ZPREVIOUS_NULL_SUBS_LEVEL1 == expand_prev_key);
	is_mm = (dba_mm == cs_data->acc_meth);
	pTargHist->depth = (int)nLevl;
	pCurr = &pTargHist->h[nLevl];
	(pCurr + 1)->blk_num = 0;
	pCurr->tn = tn;
	pCurr->first_tp_srch_status = first_tp_srch_status;
	pCurr->cycle = cycle;
	pCurr->cr = cr;
	pNonStar = NULL;
	for (;;)
	{
		assert(pCurr->level == nLevl);
		pCurr->cse = NULL;
		pCurr->blk_num = nBlkId;
		pCurr->buffaddr = pBlkBase;
		bsiz = ((blk_hdr_ptr_t)pBlkBase)->bsiz;
		if (!skip_search_blk || (0 == nLevl))
		{
			if (cdb_sc_normal != (status = gvcst_search_blk(pKey, pCurr)))
				return status;
			if (0 == nLevl)
				break;
		} else
		{
			assert(PREV_REC_UNINITIALIZED == MAXUINT2);
			assert(2 == SIZEOF(pCurr->prev_rec.offset));
			assert(2 == SIZEOF(pCurr->prev_rec.match));
			pCurr->prev_rec.offset = PREV_REC_UNINITIALIZED;
			pCurr->prev_rec.match = PREV_REC_UNINITIALIZED;
			pCurr->curr_rec.offset = bsiz - BSTAR_REC_SIZE;
			pCurr->curr_rec.match = 0;
		}
		if ((n0 = pCurr->curr_rec.offset) >= bsiz)	/* Warning: Assignment */
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_blklenerr;
		}
		pRec = pBlkBase + n0;
		GET_USHORT(n0, &((rec_hdr_ptr_t)pRec)->rsiz);
		if (FALSE == CHKRECLEN(pRec, pBlkBase, n0))
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_rmisalign;
		}
		GET_LONG(nBlkId, (pRec + n0 - SIZEOF(block_id)));
		if (is_mm)
		{
			PUT_LONG(&chain2, nBlkId);
			if ((0 == chain2.flag) && (nBlkId > cs_addrs->total_blks))
			{	/* private copy should be taken care of by .flag */
				if (cs_addrs->total_blks < cs_addrs->ti->total_blks)
					return cdb_sc_helpedout;
				else
					return cdb_sc_blknumerr;
			}
		}
		if (BSTAR_REC_SIZE != n0)
			pNonStar = pCurr;
		pCurr--;
		pCurr->tn = cs_addrs->ti->curr_tn;
		if (NULL == (pBlkBase = t_qread(nBlkId, (sm_int_ptr_t)&pCurr->cycle, &pCurr->cr)))
			return (enum cdb_sc)rdfail_detail;
		pCurr->first_tp_srch_status = first_tp_srch_status;
		if (((blk_hdr_ptr_t)pBlkBase)->levl != --nLevl)
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_badlvl;
		}
	}
	if (NULL == pHist)
	{
		assert(pCurr->curr_rec.offset >= SIZEOF(blk_hdr));
		if ((pCurr->curr_rec.offset > SIZEOF(blk_hdr)) || (pCurr->curr_rec.match >= nKeyLen))
		{
			pRec = pBlkBase + SIZEOF(blk_hdr);
			GET_USHORT(n0, &((rec_hdr_ptr_t)pRec)->rsiz);
			if (FALSE == CHKRECLEN(pRec, pBlkBase, n0))
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_rmisalign;
			}
			c1 = pRec + SIZEOF(rec_hdr);
			c2 = pTarg->first_rec->base;
			if (n0 > (pTarg->first_rec->top))
			{
				n0 = pTarg->first_rec->top;
				status = cdb_sc_keyoflow;
			} else
				status = cdb_sc_rmisalign;
			if (0 != n0)
			{
				do
				{
					--n0;
					if ((0 == (*c2++ = *c1++)) && (0 == *c1))
						break;
				} while (n0);
			}
			if (0 == n0)
			{
				assert(CDB_STAGNATE > t_tries);
				return status;
			}
			assert(c2 < &pTarg->first_rec->base[pTarg->first_rec->top]); /* ensure we don't exceed allocated bounds */
			*c2 = *c1;
			DEBUG_ONLY(pTarg->first_rec->end = c2 - pTarg->first_rec->base;)
		} else
		{	/* Clue less than first rec, invalidate first_rec */
			DEBUG_ONLY(pTarg->clue.end = pKey->end);	/* needed by an assert in GVT_CLUE_INVALIDATE_FIRST_REC.
									 * in pro, this will be done later as part of the
									 * COPY_CURR_AND_PREV_KEY_TO_GVTARGET_CLUE call.
									 */
			GVT_CLUE_INVALIDATE_FIRST_REC(pTarg);
			/* Note that even though first rec is invalidated, last_rec is still usable so continue with setting it */
		}
		if (NULL == pNonStar)
		{
			*((short *)pTarg->last_rec->base) = GVT_CLUE_LAST_REC_MAXKEY;
			DEBUG_ONLY(pTarg->last_rec->end = SIZEOF(short);)
		} else
		{
			pRec = pNonStar->buffaddr + pNonStar->curr_rec.offset;
			GET_USHORT(n0, &((rec_hdr_ptr_t)pRec)->rsiz);
			c1 = pNonStar->buffaddr;
			if (FALSE == CHKRECLEN(pRec, c1, n0))
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_rmisalign;
			}
			EVAL_CMPC2((rec_hdr_ptr_t)pRec, n1);
			if (pNonStar->curr_rec.match < n1)
			{
				assert(CDB_STAGNATE > t_tries);
			 	return cdb_sc_rmisalign;
			}
			if (n1 > (int)(pTarg->last_rec->top))
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_keyoflow;
			}
			c2 = pTarg->last_rec->base;
			if (0 != n1)
				memcpy(c2, pKey->base, n1);
			c2 = (sm_uc_ptr_t)c2 + n1;

			c1 = pRec + SIZEOF(rec_hdr);
			if ((int)n0 > (int)(pTarg->last_rec->top) - n1)
			{
				n0 = pTarg->last_rec->top - n1;
				status = cdb_sc_keyoflow;
			} else
				status = cdb_sc_rmisalign;
			if (0 != n0)
			{
				do
				{
					--n0;
					if ((0 == (*c2++ = *c1++)) && (0 == *c1))
						break;
				} while (n0);
			}
			if (0 == n0)
			{
				assert(CDB_STAGNATE > t_tries);
				return status;
			}
			assert(c2 < &pTarg->last_rec->base[pTarg->last_rec->top]); /* make sure we don't exceed allocated bounds */
			*c2 = *c1;
			DEBUG_ONLY(pTarg->last_rec->end = c2 - pTarg->last_rec->base;)
		}
		COPY_CURR_AND_PREV_KEY_TO_GVTARGET_CLUE(pTarg, pKey, expand_prev_key);
	}
	return cdb_sc_normal;
}
