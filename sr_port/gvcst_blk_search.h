/****************************************************************
 *								*
 * Copyright (c) 2015-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
/*
 * --------------------------------------------------
 * Search for a key in the block
 *
 * Return:
 *	cdb_sc_normal	 - success
 *	cdb_sc_badoffset - record with 0 length encountered,
 *			   possibly a corrupt block
 *	cdb_sc_blklenerr - end of block reached without match
 * --------------------------------------------------
 */

GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	gv_namehead		*gv_target;
GBLREF	uint4			dollar_tlevel;

#ifndef GVCST_SEARCH_EXPAND_PREVKEY
#	ifdef GVCST_SEARCH_BLK
	enum cdb_sc 	gvcst_search_blk(gv_key *pKey, srch_blk_status *pStat)
#	endif
#	ifdef GVCST_SEARCH_TAIL
	/* gvcst_search_tail is the "start anywhere" version of gvcst_search_blk.
	 * Currently this is called only for level-0 blocks. The below logic is coded with this assumption.
	 * Getting started is a bit awkward, so excuse the gotos.
	 */
	enum cdb_sc	gvcst_search_tail(gv_key *pKey, srch_blk_status *pStat, gv_key *pOldKey)
#	endif
#else
#	ifdef GVCST_SEARCH_BLK
	GBLREF	gv_key	*gv_altkey;
	/* "gvcst_search_blk_expand_prevkey" is the same as "gvcst_search_blk" except this search happens on a level0 block
	 * and sets gv_altkey to fully expanded key corresponding to pStat->prev_rec. This avoids a later call to gvcst_expand_key,
	 * which would imply TWO searches of the block, and instead does the job with ONE search.
	 */
	enum cdb_sc 	gvcst_search_blk_expand_prevkey(gv_key *pKey, srch_blk_status *pStat)
#	endif
#	ifdef GVCST_SEARCH_TAIL
	/* "gvcst_search_tail_expand_prevkey" is the same as "gvcst_search_tail" except this search sets gv_altkey to the fully
	 * expanded key corresponding to pStat->prev_rec. This avoids a later call to gvcst_expand_key, which would imply
	 * TWO searches of the block, and instead does the job with ONE search.
	 */
	enum cdb_sc	gvcst_search_tail_expand_prevkey(gv_key *pKey, srch_blk_status *pStat, gv_key *pOldKey)
#	endif
#endif
{
	/* register variables named in perceived order of declining impact */
	register int		nFlg, nTargLen, nMatchCnt, nTmp;
	sm_uc_ptr_t		pBlkBase, pRecBase, pTop, pRec, pPrevRec;
	unsigned char		*pCurrTarg, *pTargKeyBase;
#	ifdef GVCST_SEARCH_TAIL
	unsigned char		*pOldKeyBase, *pCurrTargPos;
	int			tmp_cmpc;
#		ifdef GVCST_SEARCH_EXPAND_PREVKEY
		gv_key		*prevKey;
		enum cdb_sc	status;
#		endif
#	endif
	unsigned short		nRecLen;
#	ifdef GVCST_SEARCH_BLK
	boolean_t		level0;
#	endif
#	ifdef GVCST_SEARCH_EXPAND_PREVKEY
	int			prevKeyCmpLen;	/* length of compressed portion of prevKey stored in gv_altkey->base */
	int			prevKeyUnCmpLen;/* Length of uncompressed portion of prevKey */
	sm_uc_ptr_t		prevKeyUnCmp;	/* pointer to beginning of uncompressed portion of prevKey */
	unsigned char		*prevKeyStart;	/* pointer to &gv_altkey->base[0] */
	unsigned char		*prevKeyTop;	/* pointer to allocated end of gv_altkey */
	unsigned char		*tmpPtr;
#	else
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
#	if defined(GVCST_SEARCH_TAIL) && !defined(GVCST_SEARCH_EXPAND_PREVKEY)
	assert(0 < memcmp(pKey->base, pOldKey->base, pKey->end + 1));	/* below code assumes this is ensured by caller */
	if (0 == pStat->prev_rec.offset)
		return gvcst_search_blk(pKey, pStat);	/* nice clean start at the begining of a block */
#	endif
	/* The following load code (and code in a few other places) is coded in a "assember" style
	 * in an attempt to encourage the compiler to get it efficient.
	 * For instance, memory and non-memory instructions are interlaced to encourge pipelining.
	 * Of course a great compiler doesn't need help, but this is portable code and ...
	 */
	DBG_CHECK_SRCH_HIST_AND_CSE_BUFFER_MATCH(pStat);
	pBlkBase = pStat->buffaddr;
#	ifndef GVCST_SEARCH_EXPAND_PREVKEY
#		ifdef GVCST_SEARCH_BLK
		level0 = (0 == ((blk_hdr_ptr_t)pBlkBase)->levl);
		if (level0 && TREF(expand_prev_key))
			return gvcst_search_blk_expand_prevkey(pKey, pStat);
#		endif
#		ifdef GVCST_SEARCH_TAIL
		if (TREF(expand_prev_key))
			return gvcst_search_tail_expand_prevkey(pKey, pStat, pOldKey);
#		endif
#	else
		prevKeyStart = &gv_altkey->base[0];
		prevKeyTop = &gv_altkey->base[gv_altkey->top];
#		ifdef GVCST_SEARCH_BLK
		level0 = TRUE; /* We are in "gvcst_search_blk_expand_prevkey" so we should have been called for a level0 block */
		prevKeyCmpLen = 0;
		prevKeyUnCmp = NULL;
#		endif
#		ifdef GVCST_SEARCH_TAIL
		/* Note: "level0" variable is guaranteed to be TRUE since gvcst_search_tail is currently invoked only for
		 * leaf blocks. We therefore do not compute it like we do in gvcst_search_blk. Assert this assumption.
		 */
		assert(0 == pStat->level);
#		endif
#	endif
	pTop = pBlkBase + MIN(((blk_hdr_ptr_t)pBlkBase)->bsiz, cs_data->blk_size);
	pCurrTarg = pKey->base;
	pTargKeyBase = pCurrTarg;
#	ifdef GVCST_SEARCH_BLK
	pRecBase = pBlkBase;
	nRecLen = SIZEOF(blk_hdr);
	nMatchCnt = 0;
	nTargLen = (int)pKey->end;
	nTargLen++;	/* for the terminating NUL on the key */
#	endif
#	ifdef GVCST_SEARCH_TAIL
	pRecBase = pBlkBase + pStat->curr_rec.offset;
	pRec = pRecBase;
	nMatchCnt = pStat->prev_rec.match;
	pOldKeyBase = pOldKey->base;
	pPrevRec = pBlkBase + pStat->prev_rec.offset;
#		ifdef GVCST_SEARCH_EXPAND_PREVKEY
		prevKey = pStat->blk_target->prev_key;
		if ((NULL == prevKey) || (PREV_KEY_NOT_COMPUTED == prevKey->end))
		{
			status = gvcst_expand_prev_key(pStat, pOldKey, gv_altkey);
			if (cdb_sc_normal != status)
				return status;
			prevKey = gv_altkey;
		} else
		{	/* Since gv_altkey is used elsewhere, ensure that it is in sync with prevKey before performing the search
			 * and returning to the caller.
			 */
			memcpy(gv_altkey->base, prevKey->base, prevKey->end - 1);
		}
		assert(prevKey->end);
		prevKeyCmpLen = prevKey->end - 1;
		prevKeyUnCmp = &prevKey->base[prevKeyCmpLen];
		assert(KEY_DELIMITER == prevKeyUnCmp[0]);
		assert(KEY_DELIMITER == prevKeyUnCmp[1]);
#		endif
	if (pRec >= pTop)
	{	/* Terminated at end of block */
		if (pRec > pTop)
		{
			INVOKE_GVCST_SEARCH_FAIL_IF_NEEDED(pStat);
			return cdb_sc_blklenerr;
		}
		if (0 != (nTargLen = nMatchCnt))
		{
			do
			{
				if (*pCurrTarg++ != *pOldKeyBase++)
					break;
			} while (--nTargLen);
		}
		nMatchCnt -= nTargLen;
		nTargLen = 0;
#		ifdef GVCST_SEARCH_EXPAND_PREVKEY
		/* Normally pTop points to an offset in the GDS block. But in this case, we are terminating the
		 * search without a search of the actual block and so we need to point pTop to the same location
		 * where prevKeyUnCmp points and that is prevKey.
		 */
		pTop = &prevKey->base[prevKey->end + 1];	/* + 1 needed to balance pTop-- done at function end */
#		endif
	} else
	{
		nTargLen = pKey->end;
		nTargLen++;		/* for the NUL that terminates the key */
		GET_USHORT(nRecLen, &((rec_hdr_ptr_t)pRec)->rsiz);
		EVAL_CMPC2((rec_hdr_ptr_t)pRec, nTmp);
		tmp_cmpc = nTmp;
		nFlg = tmp_cmpc;
		if (0 != nFlg)
		{
			do
			{
				if (0 != (nFlg = *pCurrTarg - *pOldKeyBase++))
					break;
				pCurrTarg++;
			} while (--tmp_cmpc);
			assert(0 <= nFlg); /* because gvcst_search_tail is called ONLY if pTarg->clue.key < pKey */
			if (0 < nFlg)
			{
				nMatchCnt = (int)(pCurrTarg - pTargKeyBase);
				nTargLen -= nMatchCnt;
			}
		}
		if (0 == nFlg)
		{
			tmp_cmpc = nMatchCnt;
			nMatchCnt = (int)(pCurrTarg - pTargKeyBase);
			nTargLen -= nMatchCnt;
			tmp_cmpc -= nMatchCnt;
			if (0 < tmp_cmpc)
			{
				pCurrTargPos = pCurrTarg;
				do
				{
					if (*pCurrTargPos++ != *pOldKeyBase++)
						break;
					nMatchCnt++;
				} while (--tmp_cmpc);
			}
			goto alt_loop_entry;
		}
#	endif
		for (;;)
		{
			pRec = pRecBase + nRecLen;
#			ifdef GVCST_SEARCH_EXPAND_PREVKEY
			if (pRecBase != pBlkBase)
			{	/* nTmp points to the compression count corresponding to pPrevRec */
				if (nTmp > prevKeyCmpLen)
				{
					if (((prevKeyStart + nTmp) >= prevKeyTop) || (NULL == prevKeyUnCmp))
					{
						if (dollar_tlevel)
							TP_TRACE_HIST_MOD(pStat->blk_num, pStat->blk_target, tp_blkmod_gvcst_srch,
									  cs_data, pStat->tn, ((blk_hdr_ptr_t)pBlkBase)->tn,
									  pStat->level)
						else
							NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_srch)
						return cdb_sc_blkmod;
					}
#					ifdef GVCST_SEARCH_TAIL
					assert((prevKeyUnCmp > pBlkBase)
						|| ((prevKeyUnCmp == &prevKey->base[prevKeyCmpLen])
							&& (prevKeyCmpLen == (prevKey->end - 1))));
#					else
					assert(prevKeyUnCmp > pBlkBase);
#					endif
					memcpy(prevKeyStart + prevKeyCmpLen, prevKeyUnCmp, nTmp - prevKeyCmpLen);
				}
				prevKeyCmpLen = nTmp;
				prevKeyUnCmp = pRecBase + SIZEOF(rec_hdr);
			}
#			endif
			if (pRec >= pTop)
			{	/* Terminated at end of block */
				if (pRec > pTop)	/* If record goes off the end, then block must be bad */
				{
					INVOKE_GVCST_SEARCH_FAIL_IF_NEEDED(pStat);
					return cdb_sc_blklenerr;
				}
				nTargLen = 0;
#				ifdef GVCST_SEARCH_BLK
				if (!level0)
					nMatchCnt = 0;	/* star key */
				else
#				endif
				{	/* data block */
					pPrevRec = pRecBase;
					pRecBase = pRec;
				}
				break;
			}
			GET_USHORT(nRecLen, &((rec_hdr_ptr_t)pRec)->rsiz);
			if (0 == nRecLen)	/* If record length is 0, then block must be bad */
			{
				INVOKE_GVCST_SEARCH_FAIL_IF_NEEDED(pStat);
				return cdb_sc_badoffset;
			}
			pPrevRec = pRecBase;
			pRecBase = pRec;
			/* If current compression count > last match, then this record also matches on 'last match' characters.
			 * Keep looping.
			 */
			EVAL_CMPC2((rec_hdr_ptr_t)pRec, nTmp)
			if (nTmp > nMatchCnt)
				continue;
			if (nTmp < nMatchCnt)
			{	/* Terminate on compression count < previous match, this key is after the target */
#				ifdef GVCST_SEARCH_BLK
				if ((BSTAR_REC_SIZE == nRecLen) && !level0)
					/* Star key has size of SIZEOF(rec_hdr) + SIZEOF(block_id), make match = 0 */
					nTargLen = 0;
				else
#				endif
					/* Data block, make match = current compression count */
					nTargLen = nTmp;
				break;
			}
#			ifdef GVCST_SEARCH_TAIL
			alt_loop_entry:
#			endif
			/* Compression count == match count;  Compare current target with current record */
			pRec += SIZEOF(rec_hdr);
			do
			{
				if ((nFlg = *pCurrTarg - *pRec++) != 0)
					break;
				pCurrTarg++;
			} while (--nTargLen);
			if (0 < nFlg)
				nMatchCnt = (int)(pCurrTarg - pTargKeyBase);
			else
			{	/* Key is after target*/
#				ifdef GVCST_SEARCH_BLK
				if ((BSTAR_REC_SIZE == nRecLen) && !level0)
					/* Star key has size of SIZEOF(rec_hdr) + SIZEOF(block_id), make match = 0 */
					nTargLen = 0;
				else
#				endif
					nTargLen = (int)(pCurrTarg - pTargKeyBase);
				break;
			}
		}
#	ifdef GVCST_SEARCH_TAIL
	}
#	endif
	pStat->prev_rec.offset = (short)(pPrevRec - pBlkBase);
	pStat->prev_rec.match = (short)nMatchCnt;
	pStat->curr_rec.offset = (short)(pRecBase - pBlkBase);
	pStat->curr_rec.match = (short)nTargLen;
#	ifdef GVCST_SEARCH_EXPAND_PREVKEY
	if (NULL != (tmpPtr = prevKeyUnCmp))	/* Note: Assignment */
	{	/* gv_altkey->base[0] thru gv_altkey->base[prevKeyCmpLen] already holds the compressed portion of prevKey.
		 * Copy over uncompressed portion of prevKey into gv_altkey->base and update gv_altkey->end before returning.
		 */
		pTop--;	/* to check for double KEY_DELIMITER byte sequence without exceeding buffer allocation bounds */
		do
		{
			if (tmpPtr >= pTop)
			{
				if (dollar_tlevel)
					TP_TRACE_HIST_MOD(pStat->blk_num, pStat->blk_target, tp_blkmod_gvcst_srch, cs_data,
							  pStat->tn, ((blk_hdr_ptr_t)pBlkBase)->tn, pStat->level)
				else
					NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_srch)
				return cdb_sc_blkmod;
			}
			/* It is now safe to do *tmpPtr and *++tmpPtr without worry about exceeding array bounds */
			if ((KEY_DELIMITER == *tmpPtr++) && (KEY_DELIMITER == *tmpPtr))
				break;
		} while (TRUE);
		tmpPtr++;	/* go past second KEY_DELIMITER so that gets copied over to gv_altkey too */
		prevKeyUnCmpLen = tmpPtr - prevKeyUnCmp;
		prevKeyStart += prevKeyCmpLen;
		if (prevKeyStart + prevKeyUnCmpLen > prevKeyTop)
		{
			if (dollar_tlevel)
				TP_TRACE_HIST_MOD(pStat->blk_num, pStat->blk_target, tp_blkmod_gvcst_srch, cs_data, pStat->tn,
						  ((blk_hdr_ptr_t)pBlkBase)->tn, pStat->level)
			else
				NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_srch)
			return cdb_sc_blkmod;
		}
		memcpy(prevKeyStart, prevKeyUnCmp, prevKeyUnCmpLen);
		gv_altkey->end = prevKeyCmpLen + prevKeyUnCmpLen - 1;	/* remove 2nd KEY_DELIMITER from "end" calculation */
	} else
		gv_altkey->end = 0;
#	endif
	return cdb_sc_normal;
}
