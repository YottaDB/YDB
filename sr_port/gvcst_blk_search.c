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

/*
 * NOTE:  See also GVCST_BLK_SEARCH.MAR for the VAX platform.
 *
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
#include "send_msg.h"

GBLREF unsigned int	t_tries;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;

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

static	void	gvcst_search_fail(srch_blk_status *pStat)
{
	char	buff[1024], crbuff[256], regbuff[512];

	assert(CDB_STAGNATE <= t_tries);
	assert((NULL != pStat) && ((NULL != pStat->cr) || (dba_mm == gv_cur_region->dyn.addr->acc_meth)) && (NULL != cs_addrs));
	if (NULL != pStat)
	{
		if (NULL != pStat->cr)
			SPRINTF(crbuff, ": crbuff = 0x%lX", pStat->cr->buffaddr);
		else
			crbuff[0] = '\0';
		memcpy(regbuff, gv_cur_region->rname, gv_cur_region->rname_len);
		regbuff[gv_cur_region->rname_len] = '\0';
		SPRINTF(buff, "Possible data corruption in region %s : blk = 0x%X : buff = 0x%lX : cr = 0x%lX %s : "
				"csa = 0x%lX : csalock = 0x%lX", regbuff, pStat->blk_num, (long unsigned int) pStat->buffaddr,
				(long unsigned int) pStat->cr, crbuff, (long unsigned int) cs_addrs,
				(long unsigned int) cs_addrs->lock_addrs[0]);
		send_msg(VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(buff));
	}
}

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

enum cdb_sc 	gvcst_search_blk (gv_key *pKey, srch_blk_status *pStat)
{
	/* register variables named in perceived order of declining impact */
	register int		nFlg, nTargLen, nMatchCnt, nTmp;
	sm_uc_ptr_t		pBlkBase, pRecBase, pTop, pRec, pPrevRec;
	unsigned char		*pCurrTarg, *pTargKeyBase;
	unsigned short		nRecLen;
	int			tmp_cmpc;

	/* the following load code (and code in a few other places) is coded in a "assember" style
	 * in an attempt to encourage the compiler to get it efficient;
	 * if instance, memory and non-memory instructions are interlaced to encourge pipelining.
	 * of course a great compiler doesn't need help, but this is portable code and ...
	 */
	DBG_CHECK_SRCH_HIST_AND_CSE_BUFFER_MATCH(pStat);
	pBlkBase = pStat->buffaddr;
	pRecBase = pBlkBase;
	pTop = pBlkBase + ((blk_hdr_ptr_t)pBlkBase)->bsiz;
	nRecLen = SIZEOF(blk_hdr);
	pCurrTarg = pKey->base;
	nMatchCnt = 0;
	nTargLen = (int)pKey->end;
	pTargKeyBase = pCurrTarg;
	nTargLen++;					/* for the terminating NUL on the key */

	for (;;)
	{
		pRec = pRecBase + nRecLen;

		if (pRec >= pTop)
		{	/* Terminated at end of block */
			if (pRec > pTop)		/* If record goes off the end, then block must be bad */
			{
				INVOKE_GVCST_SEARCH_FAIL_IF_NEEDED(pStat);
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_blklenerr;
			}
			nTargLen = 0;
			if (((blk_hdr_ptr_t)pBlkBase)->levl == 0)
			{	/* data block */
				pPrevRec = pRecBase;
				pRecBase = pRec;
			}
			else
				nMatchCnt = 0;	/* star key */
			break;
		}
		GET_USHORT(nRecLen, &((rec_hdr_ptr_t)pRec)->rsiz);
		if (nRecLen == 0)			/* If record length is 0, then block must be bad */
		{
			INVOKE_GVCST_SEARCH_FAIL_IF_NEEDED(pStat);
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_badoffset;
		}
		pPrevRec = pRecBase;
		pRecBase = pRec;

		/* If current compression count > last match, then this record
		   also matches on 'last match' characters; keep looping */
		EVAL_CMPC2((rec_hdr_ptr_t)pRec, nTmp)
		if (nTmp > nMatchCnt)
			continue;

		if (nTmp < nMatchCnt)
		{	/* Terminate on compression count < previous match,
			   this key is after the target */
			if (nRecLen == BSTAR_REC_SIZE  &&  ((blk_hdr_ptr_t)pBlkBase)->levl != 0)
				/* Star key has size of SIZEOF(rec_hdr) + SIZEOF(block_id), make match = 0 */
				nTargLen = 0;
			else
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
		} while ( --nTargLen);

		if (nFlg > 0)
			nMatchCnt =(int)(pCurrTarg - pTargKeyBase);
		else
		{	/* Key is after target*/
			if (nRecLen == BSTAR_REC_SIZE  &&  (((blk_hdr_ptr_t)pBlkBase)->levl != 0))
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

	return cdb_sc_normal;
}


/* search_tail is the "start anywhere" version of search_blk
   getting started is a bit awkward, so excuse the gotos */
enum cdb_sc	gvcst_search_tail (gv_key *pKey, srch_blk_status *pStat, gv_key *pOldKey)
{
	/* register variables named in perceived order of declining impact */
	register int		nFlg, nTargLen, nMatchCnt, nTmp;
	sm_uc_ptr_t		pBlkBase, pRecBase, pRec, pTop, pPrevRec;
	unsigned char		*pCurrTarg, *pTargKeyBase, *pOldKeyBase, *pCurrTargPos;
	unsigned short		nRecLen;
	int			tmp_cmpc;

	/* see comment in gvcst_search_blk above on coding style */

	if (pStat->prev_rec.offset == 0)
		return gvcst_search_blk(pKey, pStat);	/* nice clean start at the begining of a block */
	DBG_CHECK_SRCH_HIST_AND_CSE_BUFFER_MATCH(pStat);
	pBlkBase = pStat->buffaddr;
	pRecBase = pBlkBase + pStat->curr_rec.offset;
	pRec = pRecBase;
	pTop = pBlkBase + ((blk_hdr_ptr_t)pBlkBase)->bsiz;
	nMatchCnt = pStat->prev_rec.match;
	pCurrTarg = pKey->base;
	pTargKeyBase = pCurrTarg;
	pOldKeyBase = pOldKey->base;
	pPrevRec = pBlkBase + pStat->prev_rec.offset;
	nTargLen = pKey->end;
	nTargLen++;		/* for the NUL that terminates the key */
	if (pRec >= pTop)
	{	/* Terminated at end of block */
/* eob_tail: */	if (pRec > pTop)
		{
			INVOKE_GVCST_SEARCH_FAIL_IF_NEEDED(pStat);
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_blklenerr;
		}
		if ((nTargLen = nMatchCnt) != 0)
		{
			do
			{
				if (*pCurrTarg++ != *pOldKeyBase++)
					break;
			} while (--nTargLen);
		}
		if (((blk_hdr_ptr_t)pBlkBase)->levl != 0)
			nMatchCnt = 0;	/* star key */
		else
			nMatchCnt -= nTargLen;
		nTargLen = 0;
	} else
	{
		GET_USHORT(nRecLen, &((rec_hdr_ptr_t)pRec)->rsiz);
		EVAL_CMPC2((rec_hdr_ptr_t)pRec, nTmp);
		nFlg = nTmp;
		if (nFlg != 0)
		{
			do
			{
				if ((nFlg = *pCurrTarg - *pOldKeyBase++) != 0)
					break;
				pCurrTarg++;
			} while (--nTmp);
			if (nFlg > 0)
			{
				nMatchCnt = (int)(pCurrTarg - pTargKeyBase);
				nTargLen -= nMatchCnt;
			}
			if (nFlg < 0)
			{
				nTargLen += (int)(pTargKeyBase - pCurrTarg);
				goto match_term;
			}
		}
		if (nFlg == 0)
		{
			nTmp = nMatchCnt;
			nMatchCnt = (int)(pCurrTarg - pTargKeyBase);
			nTargLen -= nMatchCnt;
			nTmp -= nMatchCnt;

			if (nTmp > 0)
			{
				pCurrTargPos = pCurrTarg;

				do
				{
					if (*pCurrTargPos++ != *pOldKeyBase++)
						break;
					nMatchCnt++;
				} while (--nTmp);
			}
			goto alt_loop_entry;
		}
		for (;;)
		{
			pRec = pRecBase + nRecLen;

			if (pRec >= pTop)
			{	/* Terminated at end of block */
				if (pRec > pTop)		/* If record goes off the end, then block must be bad */
				{
					INVOKE_GVCST_SEARCH_FAIL_IF_NEEDED(pStat);
					assert(CDB_STAGNATE > t_tries);
					return cdb_sc_blklenerr;
				}
				nTargLen = 0;

				if (((blk_hdr_ptr_t)pBlkBase)->levl == 0)
				{	/* data block */
					pPrevRec = pRecBase;
					pRecBase = pRec;
				}
				else
					nMatchCnt = 0;	/* star key */
				break;
			}
			GET_USHORT(nRecLen, &((rec_hdr_ptr_t)pRec)->rsiz);
			if (nRecLen == 0)		/* If record length is 0, then block must be bad */
			{
				INVOKE_GVCST_SEARCH_FAIL_IF_NEEDED(pStat);
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_badoffset;
			}
			pPrevRec = pRecBase;
			pRecBase = pRec;
			/* If current compression count > last match, then this record
			   also matches on 'last match' characters; keep looping */
			EVAL_CMPC2((rec_hdr_ptr_t)pRec, nTmp);
			if (nTmp > nMatchCnt)
				continue;
			if (nTmp < nMatchCnt)
/* cc_term: */		{	/* Terminated on compression count < previous match,
				   this key is after the target */
				if (nRecLen == BSTAR_REC_SIZE  &&  ((blk_hdr_ptr_t)pBlkBase)->levl != 0)
					/* Star key has size of SIZEOF(rec_hdr) + SIZEOF(block_id), make match = 0 */
					nTargLen = 0;
				else
					/* Data block, make match = current compression count */
					nTargLen = nTmp;
				break;
			}
alt_loop_entry:		/* Compression count == match count;  Compare current target with current record */
			pRec += SIZEOF(rec_hdr);
			do
			{
				if ((nFlg = *pCurrTarg - *pRec++) != 0)
					break;
				pCurrTarg++;
			} while (--nTargLen);
			if (nFlg > 0)
				nMatchCnt = (int)(pCurrTarg - pTargKeyBase);
			else
match_term:		{	/* Key is after target*/
				if (nRecLen == BSTAR_REC_SIZE  &&  (((blk_hdr_ptr_t)pBlkBase)->levl != 0))
					/* Star key has size of SIZEOF(rec_hdr) + SIZEOF(block_id), make match = 0 */
					nTargLen = 0;
				else
					nTargLen = (int)(pCurrTarg - pTargKeyBase);
				break;
			}
		}
	}
/* clean_up: */
	pStat->prev_rec.offset = (short)(pPrevRec - pBlkBase);
	pStat->prev_rec.match = (short)nMatchCnt;
	pStat->curr_rec.offset = (short)(pRecBase - pBlkBase);
	pStat->curr_rec.match = (short)nTargLen;
	return cdb_sc_normal;
}
