/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GVCST_EXPAND_KEY_INCLUDED
#define GVCST_EXPAND_KEY_INCLUDED

enum cdb_sc gvcst_expand_key(srch_blk_status *pStat, int4 rec_top, gv_key *key);
enum cdb_sc gvcst_expand_curr_key(srch_blk_status *pStat, gv_key *srch_key, gv_key *exp_key);
enum cdb_sc gvcst_expand_prev_key(srch_blk_status *pStat, gv_key *srch_key, gv_key *exp_key);

GBLREF	sgmnt_data_ptr_t	cs_data;

#endif /* GVCST_EXPAND_KEY_INCLUDED */

#if (defined(GVCST_EXPAND_CURR_KEY) || defined(GVCST_EXPAND_PREV_KEY))
#	ifdef GVCST_EXPAND_CURR_KEY
	/* Determine the uncompressed key at pStat->curr_rec.offset after a block search using srch_key has filled in
	 * pStat->curr_rec. The uncompressed key is stored in "exp_key".
	 */
	enum cdb_sc	gvcst_expand_curr_key(srch_blk_status *pStat, gv_key *srch_key, gv_key *exp_key)
#	endif
#	ifdef GVCST_EXPAND_PREV_KEY
	/* Determine the uncompressed key at pStat->prev_rec.offset after a block search using srch_key has filled in
	 * pStat->prev_rec. The uncompressed key is stored in "exp_key".
	 */
	enum cdb_sc	gvcst_expand_prev_key(srch_blk_status *pStat, gv_key *srch_key, gv_key *exp_key)
#	endif
{
	int		tmpCmpc, match, offset, keyend;
	rec_hdr_ptr_t	rp;
	sm_uc_ptr_t	buffaddr;
	unsigned char	*dstBase, *dstEnd, *dstTop;	/* exp_key  related variables */
	unsigned char	*src;				/* srch_key related variables */
	unsigned char	ch;
#	ifdef GVCST_EXPAND_CURR_KEY
	boolean_t	fullmatch;
#	endif
#	ifdef DEBUG
	boolean_t	match_adjusted = FALSE;
#	endif

	/* Since searching for "srch_key" landed us in between pStat->prev_rec and pStat->curr_rec, we are guaranteed that
	 * pStat->curr_rec.match >= record-compression-count-at-pStat->curr_rec.offset (or else the search would not have
	 * terminated in between prev_rec and curr_rec). This means we can get all the compressed bytes of the key at
	 * curr_rec from srch_key and get the uncompressed bytes from the actual record.
	 */
	buffaddr = pStat->buffaddr;
#	ifdef GVCST_EXPAND_PREV_KEY
	offset = pStat->prev_rec.offset;
	if (SIZEOF(blk_hdr) > offset)
	{
		assert(0 == offset);
		return cdb_sc_badoffset; /* prev_key not in current block but in left sibling block. Return */
	}
	match = pStat->prev_rec.match;
#	endif
#	ifdef GVCST_EXPAND_CURR_KEY
	offset = pStat->curr_rec.offset;
	match = pStat->curr_rec.match;
#	endif
	assert(SIZEOF(blk_hdr) <= offset);
	rp = (rec_hdr_ptr_t)(buffaddr + offset);
	EVAL_CMPC2(rp, tmpCmpc);
	if (tmpCmpc > match)
	{
#		ifdef GVCST_EXPAND_PREV_KEY
		/* We cannot determine the uncompressed prev_key based only on prev_rec.match and srch_key.
		 * Need to go the full-blown route.
		 */
		return gvcst_expand_key(pStat, offset, exp_key);
#		endif
#		ifdef GVCST_EXPAND_CURR_KEY
		/* This means the block changed since we did the search. Return abnormal status so retry occurs. */
		return cdb_sc_blkmod;
#		endif
	}
	/* Get all compressed bytes of exp_key from srch_key and get the uncompressed bytes from actual record */
	dstBase = dstEnd = exp_key->base;
	dstTop = &dstBase[exp_key->top];
	keyend = srch_key->end;
	assert(2 <= keyend);	/* Need at least one non-zero byte to start and a zero byte to end key */
#	ifdef GVCST_EXPAND_PREV_KEY
	assert(match != (keyend + 1));	/* Can have a full-match only on curr_key, not on prev_key */
#	endif
	src = srch_key->base;
#	ifdef GVCST_EXPAND_CURR_KEY
	if (match == (keyend + 1))
	{	/* Full match. Return srch_key */
		fullmatch = TRUE;
		match = keyend + 1;
	} else
	{
		fullmatch = FALSE;
#	endif
		assert(match <= keyend);
		/* If last matching byte in key is \0, back off one byte while copying from srch_key. Otherwise, the
		 * logic to check for double KEY_DELIMITER sequence below will get confused.
		 */
		if ((match > tmpCmpc) && (KEY_DELIMITER == src[match - 1]))
		{
			match--;
			DEBUG_ONLY(match_adjusted = TRUE;)
		}
#	ifdef GVCST_EXPAND_CURR_KEY
	}
#	endif
	if (dstEnd + match >= dstTop)
	{
		assert(CDB_STAGNATE > t_tries);
		return cdb_sc_keyoflow;
	}
	memcpy(dstEnd, src, match);
#	ifdef GVCST_EXPAND_CURR_KEY
	if (fullmatch)
		match--;
#	endif
	dstEnd += match;
#	ifdef GVCST_EXPAND_CURR_KEY
	if (!fullmatch)
	{
#	endif
		src = ((sm_uc_ptr_t)(rp + 1)) + (match - tmpCmpc);
		dstTop--;	/* to check for double KEY_DELIMITER byte sequence without exceeding buffer allocation bounds */
		for ( ; ; )
		{
			if (dstEnd >= dstTop)
			{
				assert(CDB_STAGNATE > t_tries);
				return cdb_sc_keyoflow;
			}
			*dstEnd++ = ch = *src++;
			if ((KEY_DELIMITER == ch) && (KEY_DELIMITER == (ch = *src)))
			{
				*dstEnd = ch;
				break;
			}
		}
#	ifdef GVCST_EXPAND_CURR_KEY
	}
#	endif
	if (KEY_DELIMITER == *dstBase)
	{	/* A valid key wouldn't start with a '\0' character. So the block must have been concurrently modified. */
		return cdb_sc_mkblk;
	}
	exp_key->end = dstEnd - dstBase;
	assert(2 <= exp_key->end);
	/* Ensure the key is double-null-byte terminated even if this is a restartable situation.
	 * Callers like gvcst_put rely on this (in asserts).
	 */
	*dstEnd-- = KEY_DELIMITER;
	*dstEnd-- = KEY_DELIMITER;
	if (KEY_DELIMITER == *dstEnd)
	{	/* A valid key should have a non-null byte before the terminating 2-null-bytes.
		 * If not, the block must have been concurrently modified. So restart.
		 */
		return cdb_sc_mkblk;
	}
	/* exp_key->prev is not initialized. Caller should not rely on this. */
	/* Due to concurrency issues, it is possible "exp_key" is not a well-formed key (e.g. it might have two successive
	 * KEY_DELIMITER bytes in the middle of the key). So we cannot add a DBG_CHECK_GVKEY_VALID(exp_key) here.
	 * But we expect later validation to catch this and restart the transaction (without affecting db integrity).
	 * So we dont worry about such keys here.
	 */
	return cdb_sc_normal;
}
#endif /* (defined(GVCST_EXPAND_CURR_KEY) || defined(GVCST_EXPAND_PREV_KEY)) */
