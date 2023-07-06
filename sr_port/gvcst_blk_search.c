/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "copy.h"
#include "cdb_sc.h"
#include "gvcst_protos.h"	/* for gvcst_search_tail,gvcst_search_blk prototype */
#include "min_max.h"
#include "gvcst_expand_key.h"
#include "send_msg.h"
#include "cert_blk.h"

/*
 * -------------------------------------------------------------------
 * Search a single gvcst block
 *
 *	function definition
 *		enum cdb_sc gvcst_search_blk(pKey,pStat)
 *		gv_key *pKey;		- target key
 *		srch_blk_status *pStat;	- status block for this buffer
 *
 *	function returns cdb_sc_normal if successful;  otherwise,
 *	one of the failure codes cdb_sc_badoffset or cdb_sc_blklenerr.
 *
 *	function definition
 *		enum cdb_sc gvcst_search_tail(pKey,pStat,pOldKey)
 *		gv_key *pKey;		- target key
 *		srch_blk_status *pStat;	- status block for this buffer
 *		gv_key *pOldKey;	- key for status block
 *
 *	gvcst_search_tail is identical to gvcst_search_blk,
 *	except instead of starting with the beginning
 *	of the block, it starts where the previous gvcst_search_blk
 *	left off, using the srch_blk_status to set-up.
 *
 *	if successful, fills in srch_blk_status as follows:
 *
 *		---------------------------------
 *		| Block number (untouched)	|
 *		---------------------------------
 *		| Buffer address (input param)	|
 *		---------------------------------
 *		| Transaction number (untouched)|
 *		---------------------------------
 *		| match		| offset	|	previous record
 *		---------------------------------
 *		| match		| offset	|	current record
 *		---------------------------------
 *
 * if the match is not found, or is found at the top or bottom of the
 * block, then the values of the return fields are as follows:
 *
 *				PREVIOUS REC	CURRENT REC
 * CONDITION			MATCH	OFFSET	MATCH	OFFSET
 * ---------			----	------	-----	------
 * Buffer empty			0	0	0	7
 * Hit first key in block	0	0	a	7
 * Hit star key			b	c	0	x
 * Hit last key (leaf)		b	c	a	x
 * Went past last key (leaf)	b	x	0	y
 *
 * where:
 *	a = size of target key
 *	b = number of characters which match on the previous
 *	    key (including those which have been compressed away)
 *	c = offset of previous record
 *	x = offset for last record in the block
 *	y = top of buffer (same as block bsize)
 *
 *
 * Block structure
 *	block : <blk_hdr> <block data>
 *	blk_hdr : <block size> <block level> <transaction number>
 *	blk_data : record...record
 *	record : <rec_hdr> [rec_data]
 *	rec_hdr : <record size> <compression count>
 *	rec_data : [byte...byte]
 * -------------------------------------------------------------------
 */

GBLREF unsigned int	t_tries;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF gv_namehead	*gv_target;
GBLREF uint4		dollar_tlevel;
GBLREF gv_key		*gv_altkey;

error_def(ERR_TEXT);

#ifdef DEBUG
#include "gdscc.h"

#define	DBG_CHECK_SRCH_HIST_AND_CSE_BUFFER_MATCH(pStat)				\
{										\
	GBLREF	uint4		dollar_tlevel;					\
										\
	srch_blk_status		*tp_srch_status;				\
	cw_set_element		*cse;						\
										\
	if (dollar_tlevel)							\
	{									\
		tp_srch_status = pStat->first_tp_srch_status;			\
		if (NULL != tp_srch_status)					\
		{								\
			cse = tp_srch_status->cse;				\
			if (NULL != cse)					\
				assert(cse->new_buff == pStat->buffaddr);	\
		}								\
	}									\
}
#else
#define	DBG_CHECK_SRCH_HIST_AND_CSE_BUFFER_MATCH(pStat)
#endif

#define	INVOKE_GVCST_SEARCH_FAIL_IF_NEEDED(pStat)	if (CDB_STAGNATE <= t_tries) gvcst_search_fail(pStat);
#define	OUT_LINE	1024 + 1

static	void	gvcst_search_fail(srch_blk_status *pStat)
{
	char		buff[OUT_LINE], crbuff[SIZEOF(blk_hdr_ptr_t) + 1], regbuff[MAX_RN_LEN + 1];
	uint4		len;

	assert(CDB_STAGNATE <= t_tries);
	assert((NULL != pStat) && ((NULL != pStat->cr) || (dba_mm == gv_cur_region->dyn.addr->acc_meth)) && (NULL != cs_addrs));
	if (NULL != pStat)
	{
		if (NULL != pStat->cr)
		{
			SNPRINTF(crbuff, OUT_LINE, ": crbuff = 0x%lX", pStat->cr->buffaddr);
			cert_blk(gv_cur_region, pStat->cr->blk, (blk_hdr_ptr_t)GDS_ANY_REL2ABS(cs_addrs, pStat->cr->buffaddr),
				0,  SEND_MSG_ON_CERT_FAIL, NULL);
		} else
			crbuff[0] = '\0';
		len = (6 * SIZEOF(long unsigned int)) + gv_cur_region->rname_len + 1
			+ STRLEN("Possible data corruption in region ")
			+ STRLEN(" : blk = 0x : buff = 0x : cr = 0x%  : csa = 0x% : csalock = 0x%");
		memcpy(regbuff, gv_cur_region->rname, gv_cur_region->rname_len);
		regbuff[gv_cur_region->rname_len] = '\0';
		SNPRINTF(buff, len, "Possible data corruption in region %s : blk = 0x%lX : buff = 0x%lX : cr = 0x%lX %s : "
				"csa = 0x%lX : csalock = 0x%lX",
				regbuff, (long unsigned int)pStat->blk_num, (long unsigned int)pStat->buffaddr,
				(long unsigned int)pStat->cr, crbuff, (long unsigned int)cs_addrs,
				(long unsigned int)cs_addrs->mlkctl);
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(buff));
	}
	assert(t_tries);				/* assert here so we don't have to do it when this returns */
}

/* "gvcst_search_blk_expand_prevkey" is the same as "gvcst_search_blk" except this search happens on a level0 block
 * and sets gv_altkey to fully expanded key corresponding to pStat->prev_rec. This avoids a later call to gvcst_expand_key,
 * which would imply TWO searches of the block, and instead does the job with ONE search.
 */
enum cdb_sc 	gvcst_search_blk_expand_prevkey(gv_key *pKey, srch_blk_status *pStat)
{
	/* register variables named in perceived order of declining impact */
	register int		nFlg, nTargLen, nMatchCnt, nTmp = 0;
	sm_uc_ptr_t		pBlkBase, pRecBase, pTop, pRec, pPrevRec;
	unsigned char		*pCurrTarg, *pTargKeyBase;
	boolean_t		long_blk_id;
	unsigned short		nRecLen;
	boolean_t		level0;
	int			prevKeyCmpLen;	/* length of compressed portion of prevKey stored in gv_altkey->base */
	int			prevKeyUnCmpLen;/* Length of uncompressed portion of prevKey */
	sm_uc_ptr_t		prevKeyUnCmp;	/* pointer to beginning of uncompressed portion of prevKey */
	unsigned char		*prevKeyStart;	/* pointer to &gv_altkey->base[0] */
	unsigned char		*prevKeyTop;	/* pointer to allocated end of gv_altkey */
	unsigned char		*tmpPtr;

	/* The following load code (and code in a few other places) is coded in a "assembler" style
	 * in an attempt to encourage the compiler to get it efficient.
	 * For instance, memory and non-memory instructions are interlaced to encourage pipelining.
	 * Of course a great compiler doesn't need help, but this is portable code and ...
	 */
	DBG_CHECK_SRCH_HIST_AND_CSE_BUFFER_MATCH(pStat);
	pBlkBase = pStat->buffaddr;
	long_blk_id = IS_64_BLK_ID(pBlkBase);
	prevKeyStart = &gv_altkey->base[0];
	prevKeyTop = &gv_altkey->base[gv_altkey->top];
	level0 = TRUE; /* We are in "gvcst_search_blk_expand_prevkey" so we should have been called for a level0 block */
	prevKeyCmpLen = 0;
	prevKeyUnCmp = NULL;
	pTop = pBlkBase + MIN(((blk_hdr_ptr_t)pBlkBase)->bsiz, cs_data->blk_size);
	pCurrTarg = pKey->base;
	pTargKeyBase = pCurrTarg;
	pRecBase = pBlkBase;
	nRecLen = SIZEOF(blk_hdr);
	nMatchCnt = 0;
	nTargLen = (int)pKey->end;
	nTargLen++;	/* for the terminating NUL on the key */
	for (;;)
	{
		pRec = pRecBase + nRecLen;
		if (pRecBase != pBlkBase)
		{	/* nTmp points to the compression count corresponding to pPrevRec */
			if (nTmp > prevKeyCmpLen)
			{
				if (((prevKeyStart + nTmp) >= prevKeyTop) || (NULL == prevKeyUnCmp))
				{
					if (dollar_tlevel)
						TP_TRACE_HIST_MOD(pStat->blk_num, pStat->blk_target, tp_blkmod_gvcst_srch,
								  cs_data, pStat->tn, ((blk_hdr_ptr_t)pBlkBase)->tn,
								  pStat->level);
					else
						NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_srch);
					return cdb_sc_blkmod;
				}
				assert(prevKeyUnCmp > pBlkBase);
				memcpy(prevKeyStart + prevKeyCmpLen, prevKeyUnCmp, nTmp - prevKeyCmpLen);
			}
			prevKeyCmpLen = nTmp;
			prevKeyUnCmp = pRecBase + SIZEOF(rec_hdr);
		}
		if (pRec >= pTop)
		{	/* Terminated at end of block */
			if (pRec > pTop)	/* If record goes off the end, then block must be bad */
			{
				INVOKE_GVCST_SEARCH_FAIL_IF_NEEDED(pStat);
				return cdb_sc_blklenerr;
			}
			nTargLen = 0;
			if (!level0)
				nMatchCnt = 0;	/* star key */
			else
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
			if ((bstar_rec_size(long_blk_id) == nRecLen) && !level0)
			{	/* Star key has size of SIZEOF(rec_hdr) + SIZEOF(block_id), make match = 0 */
				nTargLen = 0;
			} else
			{	/* Data block, make match = current compression count */
				nTargLen = nTmp;
			}
			break;
		}
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
			if ((bstar_rec_size(long_blk_id) == nRecLen) && !level0)
				/* Star key has size of SIZEOF(rec_hdr) + SIZEOF(block_id), make match = 0 */
				nTargLen = 0;
			else
				nTargLen = (int)(pCurrTarg - pTargKeyBase);
			break;
		}
	}
	pStat->prev_rec.offset = (short)(pPrevRec - pBlkBase);
	pStat->prev_rec.match = (short)nMatchCnt;
	pStat->curr_rec.offset = (short)(pRecBase - pBlkBase);
	pStat->curr_rec.match = (short)nTargLen;
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
							  pStat->tn, ((blk_hdr_ptr_t)pBlkBase)->tn, pStat->level);
				else
					NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_srch);
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
						  ((blk_hdr_ptr_t)pBlkBase)->tn, pStat->level);
			else
				NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_srch);
			return cdb_sc_blkmod;
		}
		memcpy(prevKeyStart, prevKeyUnCmp, prevKeyUnCmpLen);
		gv_altkey->end = prevKeyCmpLen + prevKeyUnCmpLen - 1;	/* remove 2nd KEY_DELIMITER from "end" calculation */
	} else
		gv_altkey->end = 0;
	return cdb_sc_normal;
}

/* "gvcst_search_tail_expand_prevkey" is the same as "gvcst_search_tail" except this search sets gv_altkey to the fully
 * expanded key corresponding to pStat->prev_rec. This avoids a later call to gvcst_expand_key, which would imply
 * TWO searches of the block, and instead does the job with ONE search.
 */
enum cdb_sc	gvcst_search_tail_expand_prevkey(gv_key *pKey, srch_blk_status *pStat, gv_key *pOldKey)
{
	/* register variables named in perceived order of declining impact */
	register int		nFlg, nTargLen, nMatchCnt, nTmp;
	sm_uc_ptr_t		pBlkBase, pRecBase, pTop, pRec, pPrevRec;
	unsigned char		*pCurrTarg, *pTargKeyBase;
	boolean_t		long_blk_id;
	unsigned char		*pOldKeyBase, *pCurrTargPos;
	int			tmp_cmpc;
	gv_key		*prevKey;
	enum cdb_sc	status;
	unsigned short		nRecLen;
	int			prevKeyCmpLen;	/* length of compressed portion of prevKey stored in gv_altkey->base */
	int			prevKeyUnCmpLen;/* Length of uncompressed portion of prevKey */
	sm_uc_ptr_t		prevKeyUnCmp;	/* pointer to beginning of uncompressed portion of prevKey */
	unsigned char		*prevKeyStart;	/* pointer to &gv_altkey->base[0] */
	unsigned char		*prevKeyTop;	/* pointer to allocated end of gv_altkey */
	unsigned char		*tmpPtr;

	/* The following load code (and code in a few other places) is coded in a "assembler" style
	 * in an attempt to encourage the compiler to get it efficient.
	 * For instance, memory and non-memory instructions are interlaced to encourage pipelining.
	 * Of course a great compiler doesn't need help, but this is portable code and ...
	 */
	DBG_CHECK_SRCH_HIST_AND_CSE_BUFFER_MATCH(pStat);
	pBlkBase = pStat->buffaddr;
	long_blk_id = IS_64_BLK_ID(pBlkBase);
	prevKeyStart = &gv_altkey->base[0];
	prevKeyTop = &gv_altkey->base[gv_altkey->top];
	/* Note: "level0" variable is guaranteed to be TRUE since gvcst_search_tail is currently invoked only for
	 * leaf blocks. We therefore do not compute it like we do in gvcst_search_blk. Assert this assumption.
	 */
	assert(0 == pStat->level);
	pTop = pBlkBase + MIN(((blk_hdr_ptr_t)pBlkBase)->bsiz, cs_data->blk_size);
	pCurrTarg = pKey->base;
	pTargKeyBase = pCurrTarg;
	pRecBase = pBlkBase + pStat->curr_rec.offset;
	pRec = pRecBase;
	nMatchCnt = pStat->prev_rec.match;
	pOldKeyBase = pOldKey->base;
	pPrevRec = pBlkBase + pStat->prev_rec.offset;
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
		/* Normally pTop points to an offset in the GDS block. But in this case, we are terminating the
		 * search without a search of the actual block and so we need to point pTop to the same location
		 * where prevKeyUnCmp points and that is prevKey.
		 */
		pTop = &prevKey->base[prevKey->end + 1];	/* + 1 needed to balance pTop-- done at function end */
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
		for (;;)
		{
			pRec = pRecBase + nRecLen;
			if (pRecBase != pBlkBase)
			{	/* nTmp points to the compression count corresponding to pPrevRec */
				if (nTmp > prevKeyCmpLen)
				{
					if (((prevKeyStart + nTmp) >= prevKeyTop) || (NULL == prevKeyUnCmp))
					{
						if (dollar_tlevel)
							TP_TRACE_HIST_MOD(pStat->blk_num, pStat->blk_target, tp_blkmod_gvcst_srch,
									  cs_data, pStat->tn, ((blk_hdr_ptr_t)pBlkBase)->tn,
									  pStat->level);
						else
							NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_srch);
						return cdb_sc_blkmod;
					}
					assert((prevKeyUnCmp > pBlkBase)
						|| ((prevKeyUnCmp == &prevKey->base[prevKeyCmpLen])
							&& (prevKeyCmpLen == (prevKey->end - 1))));
					memcpy(prevKeyStart + prevKeyCmpLen, prevKeyUnCmp, nTmp - prevKeyCmpLen);
				}
				prevKeyCmpLen = nTmp;
				prevKeyUnCmp = pRecBase + SIZEOF(rec_hdr);
			}
			if (pRec >= pTop)
			{	/* Terminated at end of block */
				if (pRec > pTop)	/* If record goes off the end, then block must be bad */
				{
					INVOKE_GVCST_SEARCH_FAIL_IF_NEEDED(pStat);
					return cdb_sc_blklenerr;
				}
				nTargLen = 0;
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
				/* Data block, make match = current compression count */
				nTargLen = nTmp;
				break;
			}
		alt_loop_entry:
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
				nTargLen = (int)(pCurrTarg - pTargKeyBase);
				break;
			}
		}
	}
	pStat->prev_rec.offset = (short)(pPrevRec - pBlkBase);
	pStat->prev_rec.match = (short)nMatchCnt;
	pStat->curr_rec.offset = (short)(pRecBase - pBlkBase);
	pStat->curr_rec.match = (short)nTargLen;
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
							  pStat->tn, ((blk_hdr_ptr_t)pBlkBase)->tn, pStat->level);
				else
					NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_srch);
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
						  ((blk_hdr_ptr_t)pBlkBase)->tn, pStat->level);
			else
				NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_srch);
			return cdb_sc_blkmod;
		}
		memcpy(prevKeyStart, prevKeyUnCmp, prevKeyUnCmpLen);
		gv_altkey->end = prevKeyCmpLen + prevKeyUnCmpLen - 1;	/* remove 2nd KEY_DELIMITER from "end" calculation */
	} else
		gv_altkey->end = 0;
	return cdb_sc_normal;
}

enum cdb_sc	gvcst_search_blk(gv_key *pKey, srch_blk_status *pStat)
{
	/* register variables named in perceived order of declining impact */
	register int		nFlg, nTargLen, nMatchCnt, nTmp;
	sm_uc_ptr_t		pBlkBase, pRecBase, pTop, pRec, pPrevRec = NULL;
	unsigned char		*pCurrTarg, *pTargKeyBase;
	boolean_t		long_blk_id;
	unsigned short		nRecLen;
	boolean_t		level0;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* The following load code (and code in a few other places) is coded in a "assembler" style
	 * in an attempt to encourage the compiler to get it efficient.
	 * For instance, memory and non-memory instructions are interlaced to encourage pipelining.
	 * Of course a great compiler doesn't need help, but this is portable code and ...
	 */
	DBG_CHECK_SRCH_HIST_AND_CSE_BUFFER_MATCH(pStat);
	pBlkBase = pStat->buffaddr;
	long_blk_id = IS_64_BLK_ID(pBlkBase);
	level0 = (0 == ((blk_hdr_ptr_t)pBlkBase)->levl);
	if (level0 && TREF(expand_prev_key))
		return gvcst_search_blk_expand_prevkey(pKey, pStat);
	pTop = pBlkBase + MIN(((blk_hdr_ptr_t)pBlkBase)->bsiz, cs_data->blk_size);
	pCurrTarg = pKey->base;
	pTargKeyBase = pCurrTarg;
	pRecBase = pBlkBase;
	nRecLen = SIZEOF(blk_hdr);
	nMatchCnt = 0;
	nTargLen = (int)pKey->end;
	nTargLen++;	/* for the terminating NUL on the key */
	for (;;)
	{
		pRec = pRecBase + nRecLen;
		if (pRec >= pTop)
		{	/* Terminated at end of block */
			if (pRec > pTop)	/* If record goes off the end, then block must be bad */
			{
				INVOKE_GVCST_SEARCH_FAIL_IF_NEEDED(pStat);
				return cdb_sc_blklenerr;
			}
			nTargLen = 0;
			if (!level0)
				nMatchCnt = 0;	/* star key */
			else
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
			if ((bstar_rec_size(long_blk_id) == nRecLen) && !level0)
			{	/* Star key has size of SIZEOF(rec_hdr) + SIZEOF(block_id), make match = 0 */
				nTargLen = 0;
			} else
				/* Data block, make match = current compression count */
				nTargLen = nTmp;
			break;
		}
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
			if ((bstar_rec_size(long_blk_id) == nRecLen) && !level0)
			{	/* Star key has size of SIZEOF(rec_hdr) + SIZEOF(block_id), make match = 0 */
				nTargLen = 0;
			} else
				nTargLen = (int)(pCurrTarg - pTargKeyBase);
			break;
		}
	}
	assert(pPrevRec);
	pStat->prev_rec.offset = (short)(pPrevRec - pBlkBase);
	pStat->prev_rec.match = (short)nMatchCnt;
	pStat->curr_rec.offset = (short)(pRecBase - pBlkBase);
	pStat->curr_rec.match = (short)nTargLen;
	return cdb_sc_normal;
}

/* gvcst_search_tail is the "start anywhere" version of gvcst_search_blk.
 * Currently this is called only for level-0 blocks. The below logic is coded with this assumption.
 * Getting started is a bit awkward, so excuse the gotos.
 */
enum cdb_sc	gvcst_search_tail(gv_key *pKey, srch_blk_status *pStat, gv_key *pOldKey)
{
	/* register variables named in perceived order of declining impact */
	register int		nFlg, nTargLen, nMatchCnt, nTmp;
	sm_uc_ptr_t		pBlkBase, pRecBase, pTop, pRec, pPrevRec;
	unsigned char		*pCurrTarg, *pTargKeyBase;
	boolean_t		long_blk_id;
	unsigned char		*pOldKeyBase, *pCurrTargPos;
	int			tmp_cmpc;
	unsigned short		nRecLen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(0 < memcmp(pKey->base, pOldKey->base, pKey->end + 1));	/* below code assumes this is ensured by caller */
	if (0 == pStat->prev_rec.offset)
		return gvcst_search_blk(pKey, pStat);	/* nice clean start at the begining of a block */
	/* The following load code (and code in a few other places) is coded in a "assembler" style
	 * in an attempt to encourage the compiler to get it efficient.
	 * For instance, memory and non-memory instructions are interlaced to encourage pipelining.
	 * Of course a great compiler doesn't need help, but this is portable code and ...
	 */
	DBG_CHECK_SRCH_HIST_AND_CSE_BUFFER_MATCH(pStat);
	pBlkBase = pStat->buffaddr;
	long_blk_id = IS_64_BLK_ID(pBlkBase);
	if (TREF(expand_prev_key))
		return gvcst_search_tail_expand_prevkey(pKey, pStat, pOldKey);
	pTop = pBlkBase + MIN(((blk_hdr_ptr_t)pBlkBase)->bsiz, cs_data->blk_size);
	pCurrTarg = pKey->base;
	pTargKeyBase = pCurrTarg;
	pRecBase = pBlkBase + pStat->curr_rec.offset;
	pRec = pRecBase;
	nMatchCnt = pStat->prev_rec.match;
	pOldKeyBase = pOldKey->base;
	pPrevRec = pBlkBase + pStat->prev_rec.offset;
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
		for (;;)
		{
			pRec = pRecBase + nRecLen;
			if (pRec >= pTop)
			{	/* Terminated at end of block */
				if (pRec > pTop)	/* If record goes off the end, then block must be bad */
				{
					INVOKE_GVCST_SEARCH_FAIL_IF_NEEDED(pStat);
					return cdb_sc_blklenerr;
				}
				nTargLen = 0;
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
				/* Data block, make match = current compression count */
				nTargLen = nTmp;
				break;
			}
			alt_loop_entry:
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
				nTargLen = (int)(pCurrTarg - pTargKeyBase);
				break;
			}
		}
	}
	pStat->prev_rec.offset = (short)(pPrevRec - pBlkBase);
	pStat->prev_rec.match = (short)nMatchCnt;
	pStat->curr_rec.offset = (short)(pRecBase - pBlkBase);
	pStat->curr_rec.match = (short)nTargLen;
	return cdb_sc_normal;
}
