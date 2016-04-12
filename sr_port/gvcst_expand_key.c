/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cdb_sc.h"
#include "copy.h"
#include "min_max.h"
#include "gvcst_expand_key.h"

GBLREF	gd_region	*gv_cur_region;
GBLREF unsigned int	t_tries;
GBLREF	uint4		dollar_tlevel;

/* Note: A lot of the code below is similar to that in gvcst_blk_search.h.
 * Any changes there need to be incorporated here and vice-versa.
 */
enum cdb_sc	gvcst_expand_key(srch_blk_status *pStat, int4 rec_top, gv_key *key)
{
	int		expKeyCmpLen;	/* length of compressed portion of expKey stored in key->base */
	int		expKeyUnCmpLen;/* Length of uncompressed portion of expKey */
	int		nTmp;
	int		r_offset;
	rec_hdr_ptr_t	rp, rtop;
	blk_hdr_ptr_t	bp;
	sm_uc_ptr_t	expKeyUnCmp;	/* pointer to beginning of uncompressed portion of expKey */
	sm_uc_ptr_t	pTop;
	unsigned char	*expKeyStart;	/* pointer to &key->base[0] */
	unsigned char	*expKeyTop;	/* pointer to allocated end of input "key" */
	unsigned char	*tmpPtr;
	unsigned short	temp_ushort;

	assert(SIZEOF(rec_hdr) <= SIZEOF(blk_hdr));
	bp = (blk_hdr_ptr_t)pStat->buffaddr;
	rp = (rec_hdr_ptr_t)bp;
	rtop = (rec_hdr_ptr_t)((sm_uc_ptr_t)bp + rec_top);
	expKeyCmpLen = 0;
	expKeyUnCmp = NULL;
	expKeyStart = &key->base[0];
	expKeyTop = &key->base[key->top];
	for (r_offset = SIZEOF(blk_hdr);  ; GET_USHORT(temp_ushort, &rp->rsiz), r_offset = temp_ushort)
	{
		/* WARNING:  Assumes that SIZEOF(rec_hdr) <= SIZEOF(blk_hdr)	*/
		if (r_offset < SIZEOF(rec_hdr))
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_r2small;
		}
		rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)rp + r_offset);
		if (rp > rtop)
		{
			assert(CDB_STAGNATE > t_tries);
			return cdb_sc_rmisalign;
		}
		nTmp = EVAL_CMPC(rp);
		if (nTmp > expKeyCmpLen)
		{
			if (((expKeyStart + nTmp) >= expKeyTop) || (NULL == expKeyUnCmp))
			{
				if (dollar_tlevel)
					TP_TRACE_HIST_MOD(pStat->blk_num, pStat->blk_target, tp_blkmod_gvcst_srch, cs_data,
							  pStat->tn, ((blk_hdr_ptr_t)bp)->tn, pStat->level)
				else
					NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_expand_key)
				return cdb_sc_blkmod;
			}
			assert(NULL != expKeyUnCmp);
			memcpy(expKeyStart + expKeyCmpLen, expKeyUnCmp, nTmp - expKeyCmpLen);
		}
		expKeyCmpLen = nTmp;
		expKeyUnCmp = (sm_uc_ptr_t)rp + SIZEOF(rec_hdr);
		if (rp == rtop)
			break;
	}
	assert(NULL != expKeyUnCmp);
	tmpPtr = expKeyUnCmp;
	/* gv_altkey->base[0] thru gv_altkey->base[expKeyCmpLen] already holds the compressed portion of expKey.
	 * Copy over uncompressed portion of expKey into gv_altkey->base and update gv_altkey->end before returning.
	 */
	pTop = (sm_uc_ptr_t)bp + MIN(bp->bsiz, cs_data->blk_size) - 1;	/* -1 to check for double KEY_DELIMITER byte sequence
									 * without exceeding buffer allocation bounds.
									 */
	do
	{
		if (tmpPtr >= pTop)
		{
			if (dollar_tlevel)
				TP_TRACE_HIST_MOD(pStat->blk_num, pStat->blk_target, tp_blkmod_gvcst_srch, cs_data, pStat->tn,
						  ((blk_hdr_ptr_t)bp)->tn, pStat->level)
			else
				NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_expand_key)
			return cdb_sc_blkmod;
		}
		/* It is now safe to do *tmpPtr and *++tmpPtr without worry about exceeding block bounds */
		if ((KEY_DELIMITER == *tmpPtr++) && (KEY_DELIMITER == *tmpPtr))
			break;
	} while (TRUE);
	tmpPtr++;	/* go past second KEY_DELIMITER so that gets copied over to gv_altkey too */
	expKeyUnCmpLen = tmpPtr - expKeyUnCmp;
	tmpPtr = expKeyStart + expKeyCmpLen;
	if (tmpPtr + expKeyUnCmpLen > expKeyTop)
	{
		if (dollar_tlevel)
			TP_TRACE_HIST_MOD(pStat->blk_num, pStat->blk_target, tp_blkmod_gvcst_srch, cs_data, pStat->tn,
					  ((blk_hdr_ptr_t)bp)->tn, pStat->level)
		else
			NONTP_TRACE_HIST_MOD(pStat, t_blkmod_gvcst_expand_key)
		return cdb_sc_blkmod;
	}
	memcpy(tmpPtr, expKeyUnCmp, expKeyUnCmpLen);
	if (KEY_DELIMITER == *expKeyStart)
	{	/* A valid key wouldn't start with a '\0' character. So the block must have been concurrently modified. */
		return cdb_sc_mkblk;
	}
	expKeyUnCmpLen--;	/* remove 2nd KEY_DELIMITER from "end" calculation */
	key->end = expKeyCmpLen + expKeyUnCmpLen;
	/* key->prev is not initialized. Caller should not rely on this. */
	/* Due to concurrency issues, it is possible "key" is not a well-formed key (e.g. it might have two successive
	 * KEY_DELIMITER bytes in the middle of the key). So we cannot add a DBG_CHECK_GVKEY_VALID(key) here.
	 * But we expect later validation to catch this and restart the transaction (without affecting db integrity).
	 * So we dont worry about such keys here.
	 */
	assert(2 <= key->end);
	/* Ensure the key is double-null-byte terminated even if this is a restartable situation.
	 * Callers like gvcst_put rely on this (in asserts).
	 */
	tmpPtr += expKeyUnCmpLen;
	*tmpPtr-- = KEY_DELIMITER;
	*tmpPtr-- = KEY_DELIMITER;
	if (KEY_DELIMITER == *tmpPtr)
	{	/* A valid key should have a non-null byte before the terminating 2-null-bytes.
		 * If not, the block must have been concurrently modified. So restart.
		 */
		return cdb_sc_mkblk;
	}
	return cdb_sc_normal;
}

#define GVCST_EXPAND_CURR_KEY
#include "gvcst_expand_key.h"	/* Defines the function "gvcst_expand_curr_key" */ /* BYPASSOK : intentional duplicate include. */

#undef GVCST_EXPAND_CURR_KEY

#define GVCST_EXPAND_PREV_KEY
#include "gvcst_expand_key.h"	/* Defines the function "gvcst_expand_prev_key" */ /* BYPASSOK : intentional duplicate include. */
#undef GVCST_EXPAND_PREV_KEY
