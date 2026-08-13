/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "gvcst_blk_sidx.h"

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
 * Buffer empty			0	0	0	f
 * Hit first key in block	0	0	a	f
 * Hit star key			b	c	0	x
 * Hit last key (leaf)		b	c	a	x
 * Went past last key (leaf)	b	x	0	y
 *
 * where:
 *	a = size of target key
 *	b = number of characters which match on the previous
 *	    key (including those which have been compressed away)
 *	c = offset of previous record
 *	f = offset of first record in the block (i.e. SIZEOF(blk_hdr))
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

#define	OUT_LINE	(1024 + 1)

static	void	gvcst_search_fail(srch_blk_status *pStat)
{
	char		buff[OUT_LINE], regbuff[MAX_RN_LEN + 1];
	uint4		len;

	assert(CDB_STAGNATE <= t_tries);
	assert((NULL != pStat) && ((NULL != pStat->cr) || (dba_mm == gv_cur_region->dyn.addr->acc_meth)) && (NULL != cs_addrs));
	if (NULL != pStat)
	{
		if (NULL != pStat->cr)
		{
			SNPRINTF(buff, OUT_LINE, ": buff = 0x%lX", pStat->cr->buffaddr);
			cert_blk(gv_cur_region, pStat->cr->blk, (blk_hdr_ptr_t)GDS_ANY_REL2ABS(cs_addrs, pStat->cr->buffaddr),
				0,  SEND_MSG_ON_CERT_FAIL, NULL);
		} else
			buff[0] = '\0';
		len = (6 * SIZEOF(long unsigned int)) + gv_cur_region->rname_len + 1
			+ STRLEN("Possible data corruption in region ")
			+ STRLEN(" : blk = 0x : buff = 0x : cr = 0x%  : csa = 0x% : csalock = 0x%");
		memcpy(regbuff, gv_cur_region->rname, gv_cur_region->rname_len);
		regbuff[gv_cur_region->rname_len] = '\0';
		SNPRINTF(buff, len, "Possible data corruption in region %s : blk = 0x%lX : buff = 0x%lX : cr = 0x%lX %s : "
				"csa = 0x%lX : csalock = 0x%lX",
				regbuff, (long unsigned int)pStat->blk_num, (long unsigned int)pStat->buffaddr,
				(long unsigned int)pStat->cr, buff, (long unsigned int)cs_addrs,
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
		{	/* nTmp points to the compression count corresponding to pPrevRec.
			 * Check if we need to copy over uncompressed key bytes from the previous record into "gv_altkey".
			 */
			if (nTmp > prevKeyCmpLen)
			{
				/* Check that "gv_altkey" (destination of "memcpy" below) has space to hold
				 * the additional uncompressed key bytes from the current record. If not, it is
				 * a restartable situation.
				 */
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
				assert((prevKeyUnCmp > pBlkBase) && (prevKeyUnCmp < pTop));
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
			{	/* Check if we saw at least one record in this index block (we are guaranteed to
				 * have at least a star key). If not, this is an empty index block. Most likely,
				 * a reorg has concurrently moved the contents of this index block. It is definitely
				 * a restartable situation. So return right away. Not doing so will cause us to
				 * break out of the surrounding for loop and fail the following assert in the end.
				 *	assert(pStat->curr_rec.offset >= SIZEOF(blk_hdr));
				 */
				if (pRecBase == pBlkBase)
				{
					if (dollar_tlevel)
						TP_TRACE_HIST_MOD(pStat->blk_num, pStat->blk_target,
							tp_blkmod_gvcst_srch, cs_data,
							pStat->tn, ((blk_hdr_ptr_t)pBlkBase)->tn, pStat->level);
					else
						NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_srch);
					return cdb_sc_blkmod;
				}
				nMatchCnt = 0;	/* star key */
			} else
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
	pStat->prev_rec.offset = (unsigned short)(pPrevRec - pBlkBase);
	pStat->prev_rec.match = (unsigned short)nMatchCnt;
	/* "gvcst_search" relies on the fact that PREV_REC_UNINITIALIZED is never a valid value
	 * for prev_rec.match/prev_rec.offset. Assert that.
	 */
	assert(PREV_REC_UNINITIALIZED != pStat->prev_rec.match);
	assert(PREV_REC_UNINITIALIZED != pStat->prev_rec.offset);
	pStat->curr_rec.offset = (unsigned short)(pRecBase - pBlkBase);
	assert(pStat->curr_rec.offset >= SIZEOF(blk_hdr));
	pStat->curr_rec.match = (unsigned short)nTargLen;
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
	{
		gv_altkey->end = 0;
		DEBUG_ONLY(gv_altkey->base[0] = KEY_DELIMITER);	/* so DBG_CHECK_GVKEY_VALID passes check */
	}
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
	gv_key			*prevKey;
	enum cdb_sc		status;
	boolean_t		first_iteration;
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
	ASSERT_LEAF_BLK_PREV_REC_INITIALIZED(pStat); /* See later call to this macro for why it is called here */
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
		assert(prevKey->end < gv_altkey->top);	/* so the below memcpy is safe */
		memcpy(gv_altkey->base, prevKey->base, prevKey->end - 1);
	}
	assert(prevKey->end);
	prevKeyCmpLen = prevKey->end - 1;
	prevKeyUnCmp = &prevKey->base[prevKeyCmpLen];
	first_iteration = TRUE;
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
			{	/* nTmp points to the compression count corresponding to pPrevRec.
				 * Check if we need to copy over uncompressed key bytes from the previous record into "gv_altkey".
				 */
				if (nTmp > prevKeyCmpLen)
				{
					assert((!first_iteration && (prevKeyUnCmp > pBlkBase) && (prevKeyUnCmp < pTop))
						|| (first_iteration && (prevKeyUnCmp == &prevKey->base[prevKeyCmpLen])
							&& (prevKeyCmpLen == (prevKey->end - 1))));
					/* Check that "gv_altkey" (destination of "memcpy" below) has space to hold
					 * the additional uncompressed key bytes from the current record. If not, it is
					 * a restartable situation.
					 */
					if (((prevKeyStart + nTmp) >= prevKeyTop) || (NULL == prevKeyUnCmp)
						/* Check if "prevKeyUnCmp" (source of "memcpy" below) is pointing to prevKey.
						 * This is the case in the first iteration (as asserted above). If so, the
						 * maximum value that is allowed for "nTmp" is "prevKeyCmpLen + 1" (i.e. the
						 * first null terminator byte for the last subscript of the previous key can
						 * match with the next key). If not, this is a restartable situation as
						 * "nTmp" (the compression count of the current key relative to the previous key)
						 * cannot be greater than "prevKeyCmpLen" (the entire length of the previous key).
						 * Hence add this condition too to the "if" check that is already in progress.
						 */
						|| (first_iteration && (nTmp > (prevKeyCmpLen + 1))))
					{
						if (dollar_tlevel)
							TP_TRACE_HIST_MOD(pStat->blk_num, pStat->blk_target, tp_blkmod_gvcst_srch,
									  cs_data, pStat->tn, ((blk_hdr_ptr_t)pBlkBase)->tn,
									  pStat->level);
						else
							NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_srch);
						return cdb_sc_blkmod;
					}
					memcpy(prevKeyStart + prevKeyCmpLen, prevKeyUnCmp, nTmp - prevKeyCmpLen);
				}
				prevKeyCmpLen = nTmp;
				prevKeyUnCmp = pRecBase + SIZEOF(rec_hdr);
			}
			first_iteration = FALSE;
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
	pStat->prev_rec.offset = (unsigned short)(pPrevRec - pBlkBase);
	pStat->prev_rec.match = (unsigned short)nMatchCnt;
	/* "gvcst_search" relies on the fact that PREV_REC_UNINITIALIZED is never a valid value
	 * for prev_rec.match/prev_rec.offset. Assert that.
	 */
	assert(PREV_REC_UNINITIALIZED != pStat->prev_rec.match);
	assert(PREV_REC_UNINITIALIZED != pStat->prev_rec.offset);
	pStat->curr_rec.offset = (unsigned short)(pRecBase - pBlkBase);
	assert(pStat->curr_rec.offset >= SIZEOF(blk_hdr));
	pStat->curr_rec.match = (unsigned short)nTargLen;
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
	{
		gv_altkey->end = 0;
		DEBUG_ONLY(gv_altkey->base[0] = KEY_DELIMITER);	/* so DBG_CHECK_GVKEY_VALID passes check */
	}
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
	sgmnt_addrs		*csa;			/* a local copy of "cs_addrs" : it is read several times
							 * below and a global costs a load every time
							 */
	DEBUG_ONLY(boolean_t	used_index = FALSE;)	/* this search resumed from a search index sample */
	DEBUG_ONLY(blk_sidx_samp used_samp;)		/* and the sample it resumed from, for the shadow check */
	DEBUG_ONLY(trans_num	used_tn = 0;)		/* the block image this search STARTED from; see the */
	DEBUG_ONLY(uint4	used_bsiz = 0;)		/*  shadow check at the end of this function */
	DEBUG_ONLY(int4		used_cycle = 0;)	/* BG: the buffer generation it started from */
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
	/* Capture the image this search STARTS from, not the one it resumes from, for the DEBUG shadow
	 * check at the end of this function. That check re-walks the block from its first record and
	 * compares the answer with the one the index-assisted walk reached, and both walks use the pTop
	 * computed on the line above. A block that changed between here and the resume would leave the
	 * two comparing different images, and the check would report a disagreement the search index had
	 * no part in. Every disagreement examined while testing under a concurrent MUPIP REORG, which
	 * rewrites blocks underneath the processes searching them, proved to be exactly that.
	 */
	DEBUG_ONLY(used_tn = ((blk_hdr_ptr_t)pBlkBase)->tn;)
	DEBUG_ONLY(used_bsiz = (uint4)((blk_hdr_ptr_t)pBlkBase)->bsiz;)
	DEBUG_ONLY(used_cycle = (NULL != pStat->cr) ? pStat->cr->cycle : 0;)
	pCurrTarg = pKey->base;
	pTargKeyBase = pCurrTarg;
	pRecBase = pBlkBase;
	nRecLen = SIZEOF(blk_hdr);
	nMatchCnt = 0;
	nTargLen = (int)pKey->end;
	nTargLen++;	/* for the terminating NUL on the key */
	csa = cs_addrs;
	/* Start the walk from the sampled record nearest the target rather than from the first record
	 * (YDB#1143). The search index lives in a shared memory slot picked by block number, and is built
	 * on demand. It is never trusted: its stamp must say it describes this exact block image, and
	 * "gvcst_blk_sidx_locate" validates every byte it reads out of the slot and re-reads that stamp
	 * once everything the resume needs is out of a builder's reach. Anything inconsistent leaves the
	 * loop state below untouched, so the search skips the index and walks the block from its first
	 * record.
	 *
	 * Nothing here re-examines the block itself, and a resumed walk needs no more cover than an ordinary
	 * one : a concurrent rewrite tears a walk that started at the first record exactly as much as one
	 * that started at a sample, and the validation the database already does catches both alike.
	 */
	if (!level0 && SEARCHIDX_AVAILABLE(csa) && (CYCLE_PVT_COPY != pStat->cycle))
	{
		blk_sidx_samp	samp;		/* three ints in a pro build; see gvcst_blk_sidx.h */
		sm_uc_ptr_t	sidx;
		block_id	lcl_blk;
		blk_sidx_hdr	*lcl_hdr;
		boolean_t	lcl_build;

		/* The block number picks the slot, the same way for both access methods, and identifies the
		 * image in it together with the tn. Which buffer holds the image, or whether any does, is of
		 * no interest: that is what lets the index survive a global buffer being recycled, and what
		 * sizes the array to the working set rather than to the buffer pool. See gvcst_blk_sidx.h.
		 *
		 * The CYCLE_PVT_COPY test above is what keeps TP private block images out, for both access
		 * methods. In TP a search can run against a cw-set element's own copy of a block rather than
		 * against the committed image: t_qread hands back that buffer with a cycle of CYCLE_PVT_COPY
		 * and a NULL cache record, for BG and MM alike. Its content is the transaction's own while
		 * its tn still names the committed image, so it must neither read a slot, where a stamp
		 * could match an image it does not hold, nor populate one, where it would describe
		 * uncommitted content to every other process. Testing the cycle says that directly, and for
		 * both access methods; comparing the buffer against the cache record's would say it only for
		 * BG, MM having no cache record to compare against.
		 *
		 * The cycle can be trusted here because AT AN INDEX LEVEL a history keeps buffaddr, cr and
		 * cycle mutually consistent : all three are set together, by "t_qread", or by copying all
		 * three as "gvcst_lftsib" and "gvcst_rtsib" do. Two places do re-point a buffaddr on its own
		 * at a private buffer, "gvcst_search" with its "sync the buffers in pro, just in case" store
		 * and "t_recompute_upd_array" just before it searches - but both act on the LEVEL 0 history
		 * alone, and the !level0 test above has already excluded that. The assert below states what
		 * is left, rather than paying to re-establish it out of shared memory on every search.
		 */
		assert((NULL == pStat->cr) || (pBlkBase == GDS_ANY_REL2ABS(csa, pStat->cr->buffaddr)));
		lcl_blk = pStat->blk_num;
		sidx = GDS_ANY_SEARCHIDX(csa, lcl_blk);
		/* This BLK_SIDX_DESCRIBES does double duty and must stay AHEAD of the locate below. Besides
		 * choosing between building and locating, its read of the stamp is the acquire half of the
		 * builder's publish protocol : "gvcst_blk_sidx_locate" barriers immediately on entry and
		 * relies on this read having already happened, rather than repeating the test on the hottest
		 * path. See the CALLER CONTRACT above that function.
		 */
		if (!BLK_SIDX_DESCRIBES(sidx, pBlkBase, lcl_blk))
		{	/* Nothing here describes this image. Build one for the searches that follow: that walks
			 * the block once, which is precisely what this search is about to do anyway, and let
			 * this search proceed the ordinary way.
			 *
			 * Only one process may write a slot, and gvcst_blk_sidx_build enforces that with an
			 * atomic claim rather than leaving it to whoever gets there first. That both collapses
			 * the redundant builds every reader of a heavily searched block would otherwise do the
			 * moment it is updated, and - the part that is not an optimization - keeps two builders
			 * from interleaving their writes into one slot. See gvcst_blk_sidx.h for why a slot
			 * written by two builders at once corrupts a search rather than merely wasting work.
			 */
			lcl_hdr = (blk_sidx_hdr *)sidx;
			lcl_build = TRUE;
			if (BLK_SIDX_OWNED_BY_OTHER(sidx, lcl_blk))
			{	/* Somebody else's block already has this slot. The slots are a shared cache, so
				 * two blocks can collide on one.
				 *
				 * Both halves of BLK_SIDX_OWNED_BY_OTHER earn their place, and it is not
				 * BLK_SIDX_DESCRIBES narrowed for convenience - see the three of them together in
				 * gvcst_blk_sidx.h. The blk half distinguishes another block's entry from this
				 * block's own gone stale, which must be rebuilt at once and never damped. The tn
				 * half says the slot belongs to anybody at all : BLK_SIDX_TN_NONE is 0, so an
				 * untouched slot in a zeroed segment reads blk 0, which differs from every index
				 * block number, and without it the FIRST build of every block in the database would
				 * be turned away BLK_SIDX_SKIP_LIMIT times before being allowed.
				 *
				 * Taking the slot every time would have the two blocks evict each other on every
				 * search, both rebuilding constantly - measured at 2.3x SLOWER than no index at
				 * all. So turn this block away and count it. Refusing forever would let a cold
				 * block keep a slot that a busier one needs, hence the count: after BLK_SIDX_SKIP_LIMIT
				 * attempts the newcomer takes the slot. A block that keeps being searched resets
				 * the count each time it is used, so a busy incumbent is not displaced by an
				 * occasional rival.
				 *
				 * Plain stores, like the build claim: this decides only WHEN a rebuild happens,
				 * never whether an index is trusted, so a lost race costs nothing but timing.
				 */
				if (BLK_SIDX_SKIP_LIMIT > lcl_hdr->skips)
				{
					lcl_hdr->skips++;
					lcl_build = FALSE;
				} else
					lcl_hdr->skips = 0;
			}
			if (lcl_build)
				gvcst_blk_sidx_build(sidx, csa->search_idx_size, pBlkBase, cs_data->blk_size,
							lcl_blk, pStat->cr);
		} else if (gvcst_blk_sidx_locate(sidx, csa->search_idx_size, pBlkBase, lcl_blk,
							pTargKeyBase, nTargLen, &samp))
		{	/* This block just used the slot, so it is the busy incumbent. "skips" counts how many
			 * times some OTHER block has been turned away from this slot - see the damper further
			 * up, which is what does the counting - and clearing it here means those attempts only
			 * add up while the incumbent goes unused. Without that, a cold rival searching this
			 * slot once in a while would accumulate BLK_SIDX_SKIP_LIMIT attempts over any stretch
			 * of time and evict a slot that is being used constantly.
			 *
			 * Read before it is written, rather than written unconditionally, and that is not
			 * pedantry : "skips" is almost always already 0 on this path, and it lives in SHARED
			 * memory. An unconditional store would take the slot's cache line exclusive on every
			 * search, so the line would ping-pong between every process reading a hot block. The
			 * read leaves it shared, and the store happens only when there is something to clear.
			 */
			if (0 != ((blk_sidx_hdr *)sidx)->skips)
				((blk_sidx_hdr *)sidx)->skips = 0;
			/* Resume as though the walk had reached the sampled record: it becomes the record last
			 * examined, and the match count is what the target shares with its key. The loop then
			 * carries on at the record AFTER it. Seeding the count is not optional - entering the
			 * loop at a record with a match count of zero silently skips every following record
			 * whose compression count exceeds zero.
			 */
			nTmp = samp.match;	/* "gvcst_blk_sidx_locate" worked this out while it had the key */
			GET_USHORT(nRecLen, &((rec_hdr_ptr_t)(pBlkBase + samp.rec_off))->rsiz);
			pRecBase = pBlkBase + samp.rec_off;
			pPrevRec = pRecBase;
			nMatchCnt = nTmp;
			pCurrTarg = pTargKeyBase + nTmp;
			nTargLen -= nTmp;
			/* Record that this search resumed from a sample, and which sample it was. The block image
			 * it started from was captured at the top of this function, the shadow check needing both.
			 */
			DEBUG_ONLY(used_index = TRUE;)
			DEBUG_ONLY(used_samp = samp;)
		}
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
			if (!level0)
			{	/* Check if we saw at least one record in this index block (we are guaranteed to
				 * have at least a star key). If not, this is an empty index block. Most likely,
				 * a reorg has concurrently moved the contents of this index block. It is definitely
				 * a restartable situation. So return right away. Not doing so will cause us to
				 * break out of the surrounding for loop and fail the following assert in the end.
				 *	assert(pStat->curr_rec.offset >= SIZEOF(blk_hdr));
				 */
				if (pRecBase == pBlkBase)
				{
					if (dollar_tlevel)
						TP_TRACE_HIST_MOD(pStat->blk_num, pStat->blk_target,
							tp_blkmod_gvcst_srch, cs_data,
							pStat->tn, ((blk_hdr_ptr_t)pBlkBase)->tn, pStat->level);
					else
						NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_srch);
					return cdb_sc_blkmod;
				}
				nMatchCnt = 0;	/* star key */
			} else
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
#ifdef DEBUG
	if (!pPrevRec)
		TREF(donot_commit) |= DONOTCOMMIT_GVCST_BLK_SRCH;
#endif
	pStat->prev_rec.offset = (unsigned short)(pPrevRec - pBlkBase);
	pStat->prev_rec.match = (unsigned short)nMatchCnt;
	/* "gvcst_search" relies on the fact that PREV_REC_UNINITIALIZED is never a valid value
	 * for prev_rec.match/prev_rec.offset. Assert that.
	 */
	assert(PREV_REC_UNINITIALIZED != pStat->prev_rec.match);
	assert(PREV_REC_UNINITIALIZED != pStat->prev_rec.offset);
	pStat->curr_rec.offset = (unsigned short)(pRecBase - pBlkBase);
	assert(pStat->curr_rec.offset >= SIZEOF(blk_hdr));
	pStat->curr_rec.match = (unsigned short)nTargLen;
#	ifdef DEBUG
	if (used_index)
		gvcst_blk_sidx_shadow_check(pKey, pStat, pBlkBase, pTop, long_blk_id, level0, used_tn, used_bsiz,
						used_cycle, &used_samp);
#	endif
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
	/* Before using pStat->prev_rec.offset, we need to make sure it is initialized. i.e. it never holds the value
	 * PREV_REC_UNINITIALIZED. This is guaranteed because "gvcst_search_tail" is currently invoked only for leaf blocks.
	 * And "gvcst_search" does not currently skip "gvcst_search_blk" for leaf blocks. The below assert captures all
	 * of these so just invoke that.
	 */
	ASSERT_LEAF_BLK_PREV_REC_INITIALIZED(pStat);
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
	ASSERT_LEAF_BLK_PREV_REC_INITIALIZED(pStat); /* Purpose is in comment before previous usage of this macro just above */
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
	pStat->prev_rec.offset = (unsigned short)(pPrevRec - pBlkBase);
	pStat->prev_rec.match = (unsigned short)nMatchCnt;
	/* "gvcst_search" relies on the fact that PREV_REC_UNINITIALIZED is never a valid value
	 * for prev_rec.match/prev_rec.offset. Assert that.
	 */
	assert(PREV_REC_UNINITIALIZED != pStat->prev_rec.match);
	assert(PREV_REC_UNINITIALIZED != pStat->prev_rec.offset);
	pStat->curr_rec.offset = (unsigned short)(pRecBase - pBlkBase);
	assert(pStat->curr_rec.offset >= SIZEOF(blk_hdr));
	pStat->curr_rec.match = (unsigned short)nTargLen;
	return cdb_sc_normal;
}
