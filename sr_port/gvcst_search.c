/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "cert_blk.h"
#include "gvcst_protos.h"	/* for gvcst_search_blk,gvcst_search_tail,gvcst_search prototype */

GBLREF bool             certify_all_blocks;
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

	pTarg = gv_target;
	assert(NULL != pTarg);
	assert(pTarg->root);
	assert(pKey != &pTarg->clue);
	nKeyLen = pKey->end + 1;

	SET_GVCST_SEARCH_CLUE(0);
	INCR_DB_CSH_COUNTER(cs_addrs, n_gvcst_srches, 1);
	pTargHist = (NULL == pHist ? &pTarg->hist : pHist);

	if (0 != pTarg->clue.end)
	{	/* have valid clue */
		INCR_DB_CSH_COUNTER(cs_addrs, n_gvcst_srch_clues, 1);
		assert(!dollar_tlevel || sgm_info_ptr);
		status = cdb_sc_normal;
		if (NULL != pHist)
		{	/* Copy the full srch_hist and set loop terminator flag in unused srch_blk_status entry */
			memcpy(pHist, &pTarg->hist, HIST_SIZE(pTarg->hist));
			((srch_blk_status *)((char *)pHist + HIST_SIZE(pTarg->hist)))->blk_num = 0;
		}
		if (0 < dollar_tlevel)
		{
			if (pTarg->read_local_tn != local_tn)
			{	/* Nullify out-of-date first_tp_srch_status members of histories.
				 * Note that tp_restarts() cause no change in local_tn but yet nullify the
				 * 	hashtable (sgm_info_ptr->blks_in_use). Therefore even in that case, we
				 *	should be nullifying first_tp_srch_status members. But in that case, the
				 *	clue would have been nullified (in tp_clean_up) and we wouldn't be in this
				 *	part of the code at all. Hence the check just for read_local_tn != local_tn.
				 */
				for (srch_status = pTargHist->h; HIST_TERMINATOR != srch_status->blk_num; srch_status++)
					srch_status->first_tp_srch_status = NULL;
			}
			tp_srch_status = NULL;
			leaf_blk_hist = &pTarg->hist.h[0];
			assert(0 == leaf_blk_hist->level);
			chain1 = *(off_chain *)&leaf_blk_hist->blk_num;
			if (chain1.flag == 1)
			{
				if ((int)chain1.cw_index >= sgm_info_ptr->cw_set_depth)
				{
					assert(&FILE_INFO(sgm_info_ptr->gv_cur_region)->s_addrs == cs_addrs);
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
				cse = tp_srch_status ? tp_srch_status->ptr : NULL;
			}
			assert(!cse || !cse->high_tlevel);
			leaf_blk_hist->first_tp_srch_status = tp_srch_status;
			if ((cse != NULL) && !cse->done)
			{	/* there's a private copy and it's not up to date */
				already_built = (NULL != cse->new_buff);
				gvcst_blk_build(cse, cse->new_buff, 0);
				if (!already_built && !chain1.flag)
				{	/* is_mm is calculated twice, but this is done so as to speed up the most-frequent path,
					 * i.e. when there is a clue and either no cse or cse->done is TRUE
					 */
					is_mm = (dba_mm == cs_data->acc_meth);
					assert(tp_srch_status && (is_mm || tp_srch_status->cr) && tp_srch_status->buffaddr);
					if (tp_srch_status->tn <= ((blk_hdr_ptr_t)(tp_srch_status->buffaddr))->tn)
					{
						assert(CDB_STAGNATE > t_tries);
						TP_TRACE_HIST_MOD(leaf_blk_hist->blk_num, gv_target, tp_blkmod_gvcst_srch, cs_data,
							tp_srch_status->tn, ((blk_hdr_ptr_t)(tp_srch_status->buffaddr))->tn,
								((blk_hdr_ptr_t)(tp_srch_status->buffaddr))->levl);
						return cdb_sc_blkmod;
					}
					if ((!is_mm) && (tp_srch_status->cycle != tp_srch_status->cr->cycle
						|| tp_srch_status->blk_num != tp_srch_status->cr->blk))
					{
						assert(CDB_STAGNATE > t_tries);
						return cdb_sc_lostcr;
					}
					if (certify_all_blocks)
						cert_blk(gv_cur_region, leaf_blk_hist->blk_num, (blk_hdr_ptr_t)cse->new_buff,
								cse->blk_target->root, TRUE);	/* will GTMASSERT on integ error */
				}
				cse->done = TRUE;
				leaf_blk_hist->buffaddr = cse->new_buff;
				leaf_blk_hist->cr = 0;
				leaf_blk_hist->cycle = CYCLE_PVT_COPY;
			}
			/* For now we are testing whether the 0th (always) and 1st level (TP only) blocks have been modified
			 * instead of doing it for the whole tree. That too the 1st only in TP. This is to strike a balance between
			 * the overhead of doing a restart if an out-of-date clue is taken and the overhead of validating the clue.
			 * Also note that only the cdb_sc_blkmod check is done. The cdb_lostcr check is not done in anticipation
			 * that it is quite infrequent assuming a lot of global buffers in a production environment.
			 */
	    		srch_status = &pTargHist->h[1];
			if (srch_status->cr && (srch_status->tn <= ((blk_hdr_ptr_t)srch_status->buffaddr)->tn))
				status = cdb_sc_blkmod;
			srch_status--;
		} else
			srch_status = &pTargHist->h[0];
		if (CDB_STAGNATE <= t_tries)
		{	/* Validate every level in the clue before using since this is the final retry. */
			assert(CDB_STAGNATE == t_tries);
			is_mm = (dba_mm == cs_data->acc_meth);
			for (srch_status = &pTargHist->h[0]; HIST_TERMINATOR != srch_status->blk_num; srch_status++)
			{
				assert(srch_status->level == srch_status - &pTargHist->h[0]);
				if ((is_mm || srch_status->cr) && srch_status->tn <= ((blk_hdr_ptr_t)(srch_status->buffaddr))->tn)
				{
					status = cdb_sc_blkmod;
					break;
				}
				if (srch_status->cr)
				{
					if (srch_status->cycle != srch_status->cr->cycle)
					{
						status = cdb_sc_lostcr;
						break;
					}
					if (CDB_STAGNATE <= t_tries || mu_reorg_process)
						CWS_INSERT(srch_status->cr->blk);
					srch_status->cr->refer = TRUE;
				}
			}
		} else if (srch_status->cr)
		{	/* Re-read leaf block if cr doesn't match history. Do this only for leaf-block for performance reasons. */
			if (srch_status->tn <= ((blk_hdr_ptr_t)srch_status->buffaddr)->tn)
				status = cdb_sc_blkmod;
			else if ((srch_status->cycle != srch_status->cr->cycle)
					&& (NULL == (srch_status->buffaddr =
						t_qread(srch_status->blk_num, &srch_status->cycle, &srch_status->cr))))
				status = cdb_sc_lostcr;
		}
		if (cdb_sc_normal == status)	/* avoid using clue if you know it is out of date */
		{
			/* Put more-likely case earlier in the if then else sequence.
			 * For sequential reads of globals, we expect the tail of the clue to be much more used than the head.
			 * For random reads, both are equally probable and hence it doesn't matter.
			 * The case (0 == n1) is not expected a lot (relatively) since the application may be able to optimize
			 *	a number of reads of the same key into one read by using a local-variable to store the value.
			 */
			if (0 < (n1 = memcmp(pKey->base, pTarg->clue.base, nKeyLen)))
			{
				if (memcmp(pKey->base, pTarg->last_rec->base, nKeyLen) <= 0)
				{
					if ((cdb_sc_normal == (status = gvcst_search_tail(pKey, pTargHist->h, &pTarg->clue))) &&
						(NULL == pHist))
					{	/* need to retain old clue for future use by gvcst_search_tail */
						memcpy(&pTarg->clue, pKey, KEY_COPY_SIZE(pKey));
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
						memcpy(&pTarg->clue, pKey, KEY_COPY_SIZE(pKey));
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
		pCurr->ptr = NULL;
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
		memcpy(&pTarg->clue, pKey, KEY_COPY_SIZE(pKey));
	}
	return cdb_sc_normal;
}
