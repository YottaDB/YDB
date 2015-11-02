/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "hashtab.h"
#include "cws_insert.h"
#include "gvcst_protos.h"	/* for gvcst_search_blk,gvcst_search_tail,gvcst_search prototype */

GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gv_namehead	*gv_target;
GBLREF short		dollar_tlevel;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF unsigned char	rdfail_detail;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF unsigned int	t_tries;
GBLREF boolean_t        mu_reorg_process;
GBLREF srch_blk_status	*first_tp_srch_status;	/* overriding value of srch_blk_status given by t_qread in case of TP */
GBLREF trans_num	local_tn;		/* transaction number for THIS PROCESS */
GBLREF boolean_t	tp_restart_syslog;	/* for the TP_TRACE_HIST_MOD macro */

#ifdef DEBUG
GBLDEF char gvcst_search_clue;
#define	SET_GVCST_SEARCH_CLUE(X)	gvcst_search_clue = X;
#else
#define SET_GVCST_SEARCH_CLUE(X)
#endif

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
	block_id		nBlkId;
	cache_rec_ptr_t		cr;
	int			cycle;
	unsigned short		n0, nKeyLen;
	trans_num		tn;
	cw_set_element		*cse;
	off_chain		chain1, chain2;
	srch_blk_status		*tp_srch_status, *srch_status, *leaf_blk_hist;
	boolean_t		already_built, is_mm;
	ht_ent_int4		*tabent;
	sm_uc_ptr_t		buffaddr;
	trans_num		blkhdrtn;
	int			hist_size;

	pTarg = gv_target;
	assert(NULL != pTarg);
	assert(pTarg->root);
	assert(pKey != &pTarg->clue);
	nKeyLen = pKey->end + 1;

	SET_GVCST_SEARCH_CLUE(0);
	INCR_DB_CSH_COUNTER(cs_addrs, n_gvcst_srches, 1);
	pTargHist = (NULL == pHist ? &pTarg->hist : pHist);
	/* If final retry and TP then we can safely use clues of gv_targets that have already been used in this
	 * TP transaction (read_local_tn == local_tn). As for the other gv_targets, we have no easy way of
	 * determining if their clues are still uptodate (i.e. using the clue will guarantee us no restart) and
	 * since we are in the final retry, we dont want to take a risk. So dont use the clue in that case.
	 * If Non-TP, we will be dealing with only ONE gv_target so its clue would have been reset to 0 as part
	 * of the penultimate restart so we dont have any of the above issue in the non-tp case. The only exception
	 * is if we are in gvcst_kill in which case, gvcst_search will be called twice and the clue could be non-zero
	 * for the second invocation. In this case, the clue is guaranteed to be uptodate since it was set just now
	 * as part of the first invocation. So no need to do anything about clue in final retry for Non-TP.
	 */
	if ((0 != pTarg->clue.end)
		&& !((CDB_STAGNATE <= t_tries) && dollar_tlevel && (pTarg->read_local_tn != local_tn)))
	{	/* have valid clue that is also safe to use */
		INCR_DB_CSH_COUNTER(cs_addrs, n_gvcst_srch_clues, 1);
		assert(!dollar_tlevel || sgm_info_ptr);
		status = cdb_sc_normal;	/* clue is usable unless proved otherwise */
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
		} else if (0 < dollar_tlevel)
		{	/* First nullify first_tp_srch_status member in gv_target history if out-of-date. This is logically done
			 * at tp_clean_up time but delayed until the time this gv_target is used next in a transaction. This way
			 * it saves some CPU cycles. pTarg->read_local_tn tells us whether this is the first usage of this
			 * gv_target in this TP transaction and if so we need to reset the out-of-date field.
			 */
			if (pTarg->read_local_tn != local_tn)
			{
				for (srch_status = &pTarg->hist.h[0]; HIST_TERMINATOR != srch_status->blk_num; srch_status++)
					srch_status->first_tp_srch_status = NULL;
			}
			/* TP & going to use clue. check if clue path contains a leaf block with a corresponding unbuilt
			 * cse from the previous traversal. If so build it first before gvcst_search_blk/gvcst_search_tail.
			 */
			tp_srch_status = NULL;
			leaf_blk_hist = &pTarg->hist.h[0];
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
				if (leaf_blk_hist->first_tp_srch_status)
					tp_srch_status = leaf_blk_hist->first_tp_srch_status;
				else
				{
					if (NULL != (tabent = lookup_hashtab_int4(sgm_info_ptr->blks_in_use,
											(uint4 *)&leaf_blk_hist->blk_num)))
						tp_srch_status = tabent->value;
				}
				ASSERT_IS_WITHIN_TP_HIST_ARRAY_BOUNDS(tp_srch_status, sgm_info_ptr);
				cse = tp_srch_status ? tp_srch_status->cse : NULL;
			}
			assert(!cse || !cse->high_tlevel);
			leaf_blk_hist->first_tp_srch_status = tp_srch_status;
			if ((NULL != cse) && !cse->done)
			{	/* there's a private copy and it's not up to date */
				already_built = (NULL != cse->new_buff);
				gvcst_blk_build(cse, cse->new_buff, 0);
				/* Validate the block's search history right after building a private copy.  This is
				 * not needed in case gvcst_search is going to reuse the clue's search history and
				 * return (because tp_hist will do the validation of this block). But if gvcst_search
				 * decides to do a fresh traversal (because the clue does not cover the path of the
				 * current input key etc.) the block build that happened now will not get validated
				 * in tp_hist since it will instead be given the current key's search history path (a
				 * totally new path) for validation. Since a private copy of the block has been built,
				 * tp_tend would also skip validating this block so it is necessary that we validate
				 * the block right here. Since it is tricky to accurately differentiate between
				 * the two cases, we do the validation unconditionally here (besides it is only a
				 * few if checks done per block build so it is considered okay performance-wise).
				 */
				if (!already_built && !chain1.flag)
				{	/* is_mm is calculated twice, but this is done so as to speed up the most-frequent
					 * path, i.e. when there is a clue and either no cse or cse->done is TRUE
					 */
					is_mm = (dba_mm == cs_data->acc_meth);
					buffaddr = tp_srch_status->buffaddr;
					cr = tp_srch_status->cr;
					assert(tp_srch_status && (is_mm || cr) && buffaddr);
					blkhdrtn = ((blk_hdr_ptr_t)buffaddr)->tn;
					if (TP_IS_CDB_SC_BLKMOD3(cr, tp_srch_status, blkhdrtn))
					{
						assert(CDB_STAGNATE > t_tries);
						TP_TRACE_HIST_MOD(leaf_blk_hist->blk_num, gv_target, tp_blkmod_gvcst_srch, cs_data,
							tp_srch_status->tn, blkhdrtn, ((blk_hdr_ptr_t)buffaddr)->levl);
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
				leaf_blk_hist->buffaddr = cse->new_buff;
				leaf_blk_hist->cr = 0;
				leaf_blk_hist->cycle = CYCLE_PVT_COPY;
			}
			/* For TP, check level 1 of the clue -- see comment following the end of this block for an explanation */
	    		srch_status = &pTargHist->h[1];
			cr = srch_status->cr;
			if (TP_IS_CDB_SC_BLKMOD(cr, srch_status))
				status = cdb_sc_blkmod;	/* clue is no longer usable */
		}
		/* As a cheap way to avoid expensive restarts, test whether the level 0 (TP and non-TP) and level 1 (TP only)
		 * blocks have been modified. This strikes a balance between the overhead of validating the clue and the cost
		 * of doing a restart because of using an out-of-date clue. Check only for cdb_sc_blkmod. In a production
		 * environment because of sufficient global buffers we expect cdb_lostcr to be infrequent and skip that test
		 * for level 1. For level 0 we do that test as well just in case. This technique has been empirically validated
		 * to dramatically reduce the # of restarts in a highly contentious TP environment.
		 */
		srch_status = &pTargHist->h[0];
		cr = srch_status->cr;
		if (NULL != cr)
		{	/* Re-read leaf block if cr doesn't match history. Do this only for leaf-block for performance reasons. */
			assert(NULL != srch_status->buffaddr);
			if (TP_IS_CDB_SC_BLKMOD(cr, srch_status))
				status = cdb_sc_blkmod;	/* clue is no longer usable */
			else if ((srch_status->cycle != cr->cycle)
					&& (NULL == (srch_status->buffaddr =
						t_qread(srch_status->blk_num, &srch_status->cycle, &srch_status->cr))))
				status = cdb_sc_lostcr;	/* clue is no longer usable */
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
					if ((cdb_sc_normal == (status = gvcst_search_tail(pKey, pTargHist->h, &pTarg->clue)))
						&& (NULL == pHist))
					{	/* need to retain old clue for future use by gvcst_search_tail */
						COPY_CURRKEY_TO_GVTARGET_CLUE(pTarg, pKey);
					}
					SET_GVCST_SEARCH_CLUE(1);
					INCR_DB_CSH_COUNTER(cs_addrs, n_clue_used_tail, 1);
					return status;
				}
			} else if (0 > n1)
			{
				if (memcmp(pKey->base, pTarg->first_rec->base, nKeyLen) >= 0)
				{
					SET_GVCST_SEARCH_CLUE(3);
					if (NULL == pHist)
						COPY_CURRKEY_TO_GVTARGET_CLUE(pTarg, pKey);
					INCR_DB_CSH_COUNTER(cs_addrs, n_clue_used_head, 1);
					return gvcst_search_blk(pKey, pTargHist->h);
				}
			} else
			{
				SET_GVCST_SEARCH_CLUE(2);
				INCR_DB_CSH_COUNTER(cs_addrs, n_clue_used_same, 1);
				return cdb_sc_normal;
			}
		}
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
		if (cdb_sc_normal != (status = gvcst_search_blk(pKey, pCurr)))
			return status;
		if (0 == nLevl)
			break;
		if ((n0 = pCurr->curr_rec.offset) >= ((blk_hdr_ptr_t)pBlkBase)->bsiz)
			n0 = pCurr->prev_rec.offset;
		pRec = pBlkBase + n0;
		GET_USHORT(n0, &((rec_hdr_ptr_t)pRec)->rsiz);
		if (FALSE == CHKRECLEN(pRec, pBlkBase, n0))
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_rmisalign;
		}
		GET_LONG(nBlkId, (pRec + n0 - sizeof(block_id)));
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
		if ((pCurr->curr_rec.offset < sizeof(blk_hdr)) ||
			((pCurr->curr_rec.offset == sizeof(blk_hdr)) && (pCurr->curr_rec.match < nKeyLen)))
		{	/* Clue less than first rec, invalidate */
			pTarg->clue.end = 0;
			return cdb_sc_normal;
		}
		pRec = pBlkBase + sizeof(blk_hdr);
		GET_USHORT(n0, &((rec_hdr_ptr_t)pRec)->rsiz);
		if (FALSE == CHKRECLEN(pRec, pBlkBase, n0))
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_rmisalign;
		}
		c1 = pRec + sizeof(rec_hdr);
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
		assert(c2 < &pTarg->first_rec->base[pTarg->first_rec->top]);	/* make sure we don't exceed allocated bounds */
		*c2 = *c1;
		if (NULL == pNonStar)
			*((short *)pTarg->last_rec->base) = 0xffff;
		else
		{
			pRec = pNonStar->buffaddr + pNonStar->curr_rec.offset;
			GET_USHORT(n0, &((rec_hdr_ptr_t)pRec)->rsiz);
			c1 = pNonStar->buffaddr;
			if (FALSE == CHKRECLEN(pRec, c1, n0))
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_rmisalign;
			}

			if (pNonStar->curr_rec.match < ((rec_hdr_ptr_t)pRec)->cmpc)
			{
				assert(CDB_STAGNATE > t_tries);
			 	return cdb_sc_rmisalign;
			}

			if ((n1 = ((rec_hdr_ptr_t)pRec)->cmpc) > (int)(pTarg->last_rec->top))
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_keyoflow;
			}
			c2 = pTarg->last_rec->base;
			if (0 != n1)
				memcpy(c2, pKey->base, n1);
			c2 = (sm_uc_ptr_t)c2 + n1;

			c1 = pRec + sizeof(rec_hdr);
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
		}
		COPY_CURRKEY_TO_GVTARGET_CLUE(pTarg, pKey);
	}
	return cdb_sc_normal;
}
