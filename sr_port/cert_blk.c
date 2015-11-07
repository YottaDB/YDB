/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsbml.h"
#include "copy.h"
#include "subscript.h"
#include "filestruct.h"
#include "gdscc.h"
#include "gdskill.h"    /* needed for tp.h */
#include "jnl.h"        /* needed for tp.h */
#include "buddy_list.h" /* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "error.h"
#include "mmemory.h"
#include "gtm_ffs.h"
#include "cert_blk.h"
#include "gtm_ctype.h"
#ifdef GTM_TRIGGER
#include <rtnhdr.h>
#include "gv_trigger.h"
#endif

GBLREF	uint4		dollar_tlevel;
GBLREF	boolean_t	dse_running;
GBLREF	boolean_t	mu_reorg_upgrd_dwngrd_in_prog;

error_def(ERR_DBBLEVMX);
error_def(ERR_DBBLEVMN);
error_def(ERR_DBBSIZMN);
error_def(ERR_DBBSIZMX);
error_def(ERR_DBRSIZMN);
error_def(ERR_DBRSIZMX);
error_def(ERR_DBCMPNZRO);
error_def(ERR_DBSTARSIZ);
error_def(ERR_DBSTARCMP);
error_def(ERR_DBCMPMX);
error_def(ERR_DBKEYMX);
error_def(ERR_DBKEYMN);
error_def(ERR_DBCMPBAD);
error_def(ERR_DBKEYORD);
error_def(ERR_DBPTRNOTPOS);
error_def(ERR_DBPTRMX);
error_def(ERR_DBPTRMAP);
error_def(ERR_DBLVLINC);
error_def(ERR_DBBMSIZE);
error_def(ERR_DBBMBARE);
error_def(ERR_DBBMINV);
error_def(ERR_DBBMMSTR);
error_def(ERR_DBROOTBURN);
error_def(ERR_DBDIRTSUBSC);
error_def(ERR_DBMAXNRSUBS); /* same error as ERR_MAXNRSUBSCRIPTS, but has a string output as well */
error_def(ERR_DBINVGBL);
error_def(ERR_DBBDBALLOC);

#define BITS_PER_UCHAR	8
#define BLKS_PER_UINT4	((SIZEOF(uint4) / SIZEOF(unsigned char)) * BITS_PER_UCHAR) / BML_BITS_PER_BLK
#define BLOCK_WINDOW 8
#define LEVEL_WINDOW 3
#define OFFSET_WINDOW 4

#define TEXT1 ":     "
#define TEXT2 " "
#define TEXT3 ":"
#define TEXT4 ", "

#define MAX_UTIL_LEN 40
#define	RTS_ERROR_FUNC(CSA, ERR, BUFF)									\
{													\
	if (gtmassert_on_error)										\
		GTMASSERT;										\
	rts_error_csa(CSA_ARG(CSA) VARLSTCNT(4) MAKE_MSG_INFO(ERR), 2, LEN_AND_STR((char_ptr_t)BUFF));	\
}

int cert_blk (gd_region *reg, block_id blk, blk_hdr_ptr_t bp, block_id root, boolean_t gtmassert_on_error)
{
	block_id		child, prev_child;
	rec_hdr_ptr_t		rp, r_top;
	int			num_subscripts;
	uint4			bplmap, mask1, offset, rec_offset, rec_size;
	sm_uint_ptr_t		chunk_p;			/* Value is unaligned so will be assigned to chunk */
	uint4			chunk, blk_size;
	sm_uc_ptr_t		blk_top, blk_id_ptr, next_tp_child_ptr, key_base, mp, b_ptr;
	unsigned short		rec_cmpc, min_cmpc;	/* the minimum cmpc expected in any record (except star-key) in a gvt */
	int			tmp_cmpc;
	unsigned char		ch, prior_expkey[MAX_KEY_SZ + 1];
	unsigned int		prior_expkeylen;
	unsigned short		temp_ushort;
	int			blk_levl;
	int			comp_length, key_size;
	unsigned char		util_buff[MAX_UTIL_LEN];
	int			util_len;
	off_chain		chain;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	boolean_t		is_gvt, is_directory, first_key, full, prev_char_is_delimiter;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	bplmap = csd->bplmap;
	assert(bplmap == BLKS_PER_LMAP);
	blk_levl = bp->levl;
	blk_size = bp->bsiz;
	offset = (uint4)blk / bplmap;

	util_len=0;
	i2hex_blkfill(blk,&util_buff[util_len], BLOCK_WINDOW);
	util_len += BLOCK_WINDOW;
	MEMCPY_LIT(&util_buff[util_len], TEXT1); /* OFFSET_WINDOW + 1 spaces */
	util_len += SIZEOF(TEXT3) - 1;
	util_len += OFFSET_WINDOW +1;
	i2hex_blkfill(blk_levl, &util_buff[util_len], LEVEL_WINDOW);
	util_len += LEVEL_WINDOW;
	MEMCPY_LIT(&util_buff[util_len], TEXT2);
	util_len += SIZEOF(TEXT2) - 1;
	util_buff[util_len] = 0;

	chain = *(off_chain *)&blk;
	/* Assert that if at all chain.flag is non-zero (i.e. a created block), we are in TP and not yet in the commit logic.
	 * The only exception to this rule is if we are in TP and inside phase1 of the commit logic and trying to certify a
	 * block that was killed inside of the transaction (possible if cert_blk is called directly from tp_tend). In this case,
	 * the block number passed is a special value GDS_CREATE_BLK_MAX so check that.
	 */
	assert(!chain.flag || dollar_tlevel && (!csa->t_commit_crit
						|| ((T_COMMIT_CRIT_PHASE1 == csa->t_commit_crit) && (GDS_CREATE_BLK_MAX == blk))));
	if (!chain.flag && ((offset * bplmap) == (uint4)blk))					/* it's a bitmap */
	{
		if ((unsigned char)blk_levl != LCL_MAP_LEVL)
		{
			RTS_ERROR_FUNC(csa, MAKE_MSG_INFO(ERR_DBLVLINC), util_buff);
			return FALSE;
		}
		if (blk_size != BM_SIZE(bplmap))
		{
			RTS_ERROR_FUNC(csa, ERR_DBBMSIZE, util_buff);
			return FALSE;
		}
		mp = (sm_uc_ptr_t)bp + SIZEOF(blk_hdr);
		if ((*mp & 1) != 0)
		{	/* bitmap doesn't protect itself */
			RTS_ERROR_FUNC(csa, ERR_DBBMBARE, util_buff);
			return FALSE;
		}
		full = TRUE;
		offset = ((csa->ti->total_blks - blk) >= bplmap) ? bplmap : (csa->ti->total_blks - blk);
		blk_top = (sm_uc_ptr_t)bp + BM_SIZE(offset + (BITS_PER_UCHAR / BML_BITS_PER_BLK) - 1);
		for (chunk_p = (sm_uint_ptr_t)mp ;  (sm_uc_ptr_t)chunk_p - blk_top < 0 ;  chunk_p++)
		{
			GET_LONG(chunk, chunk_p);		/* Obtain unalinged unit4 value */
			/* The following code is NOT independent of the bitmap layout: */
			mask1 = chunk & SIXTEEN_BLKS_FREE;	/* mask 'recycled' blocks to 'free' blocks */
			if ((mask1 != 0) && full)		/* check for free blocks */
			{	/* if (full bitmap || full chunk || regular scan of a "short" bitmap) */
				if ((offset == bplmap) || ((blk_top - (sm_uc_ptr_t)chunk_p) > SIZEOF(chunk))
					|| (NO_FREE_SPACE != bml_find_free((int4)((sm_uc_ptr_t)chunk_p - mp), mp, offset)))
				{
					full = FALSE;
				}
			}
			mask1 ^= SIXTEEN_BLKS_FREE;		/* complement to busy */
			mask1 <<= 1;				/* shift to reused position */
			mask1 &= chunk;				/* check against the original contents */
			if (mask1 != 0)				/* busy and reused should never appear together */
			{
				RTS_ERROR_FUNC(csa, ERR_DBBMINV, util_buff);
				return FALSE;
			}

		}
		if (full == (NO_FREE_SPACE != gtm_ffs(blk / bplmap, MM_ADDR(csd), MASTER_MAP_BITS_PER_LMAP)))
		{
			RTS_ERROR_FUNC(csa, ERR_DBBMMSTR, util_buff);
			/* DSE CACHE -VERIFY used to fail occasionally with the DBBMMSTR error because of passing
			 * an older twin global buffer that contained stale bitmap information. That is now fixed.
			 * So we dont expect any more such failures. Assert accordingly.
			 */
			assert(!dse_running);
			return FALSE;
		}
		return TRUE;
	}
	if (blk_levl > MAX_BT_DEPTH)
	{
		RTS_ERROR_FUNC(csa, ERR_DBBLEVMX, util_buff);
		return FALSE;
	}
	if (blk_levl < 0)
	{
		RTS_ERROR_FUNC(csa, ERR_DBBLEVMN, util_buff);
		return FALSE;
	}
	if (blk_levl == 0)
	{	/* data block */
		if ((DIR_ROOT == blk) || (blk == root))
		{	/* headed for where an index block should be */
			RTS_ERROR_FUNC(csa, ERR_DBROOTBURN, util_buff);
			return FALSE;
		}
		if (blk_size < (uint4)SIZEOF(blk_hdr))
		{
			RTS_ERROR_FUNC(csa, ERR_DBBSIZMN, util_buff);
			return FALSE;
		}
	} else
	{	/* index block */
		if (blk_size < (uint4)(SIZEOF(blk_hdr) + SIZEOF(rec_hdr) + SIZEOF(block_id)))
		{	/* must have at least one record */
			RTS_ERROR_FUNC(csa, ERR_DBBSIZMN, util_buff);
			return FALSE;
		}
	}
	if (blk_size > (uint4)csd->blk_size)
	{
		RTS_ERROR_FUNC(csa, ERR_DBBSIZMX, util_buff);
		return FALSE;
	}
	is_directory = FALSE;
	is_gvt = FALSE;
	/* if both "is_directory" and "is_gvt" are FALSE, then we dont know YET if the given block is a directory or gvt */
	if (DIR_ROOT == root)
		is_directory = TRUE;
	if ((0 != root) && (DIR_ROOT != root))
		is_gvt = TRUE;
	/* MUPIP REORG -TRUNCATE has some special cases */
	if (MUSWP_INCR_ROOT_CYCLE == TREF(in_mu_swap_root_state))
	{	/* We could be updating either a gvt root block or a directory leaf block. Don't know yet. */
		is_directory = FALSE;
		is_gvt = FALSE;
	} else if (MUSWP_DIRECTORY_SWAP == TREF(in_mu_swap_root_state))
	{	/* We know we're updating a directory block, even though root is not DIR_ROOT. root and gv_target correspond
		 * to the gvt being REORG'ed.
		 */
		is_directory = TRUE;
		is_gvt = FALSE;
	}
	blk_top = (sm_uc_ptr_t)bp + blk_size;
	first_key = TRUE;
	min_cmpc = 0;
	prior_expkeylen = 0;
	comp_length = 2 * SIZEOF(char);		/* for double NUL to indicate no prior key */
	prior_expkey[0] = prior_expkey[1] = 0;	/* double NUL also works for memvcmp test for key order */
	next_tp_child_ptr = NULL;
	prev_child = 0;

	for (rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)bp + SIZEOF(blk_hdr)) ;  rp < (rec_hdr_ptr_t)blk_top ;  rp = r_top)
	{
		GET_RSIZ(rec_size, rp);
		rec_offset = (uint4)((sm_ulong_t)rp - (sm_ulong_t)bp);
		/*add util_buff here*/

		util_len=0;
		i2hex_blkfill(blk,&util_buff[util_len], BLOCK_WINDOW);
		util_len += BLOCK_WINDOW;
		MEMCPY_LIT(&util_buff[util_len], TEXT1); /* OFFSET_WINDOW + 1 spaces */
		util_len += SIZEOF(TEXT3) - 1;
		i2hex_nofill(rec_offset, &util_buff[util_len], OFFSET_WINDOW);
		util_len += OFFSET_WINDOW + 1;
		i2hex_blkfill(blk_levl, &util_buff[util_len], LEVEL_WINDOW);
		util_len += LEVEL_WINDOW;
		MEMCPY_LIT(&util_buff[util_len], TEXT2);
		util_len += SIZEOF(TEXT2) - 1;
		util_buff[util_len] = 0;

		if (rec_size <= (uint4)SIZEOF(rec_hdr))
		{
			RTS_ERROR_FUNC(csa, ERR_DBRSIZMN, util_buff);
			return FALSE;
		}
		if (rec_size > (uint4)((sm_ulong_t)blk_top - (sm_ulong_t)rp))
		{
			RTS_ERROR_FUNC(csa, ERR_DBRSIZMX, util_buff);
			return FALSE;
		}
		r_top = (rec_hdr_ptr_t)((sm_ulong_t)rp + rec_size);
		rec_cmpc = EVAL_CMPC(rp);
		if (first_key)
		{
			if (rec_cmpc)
			{
				RTS_ERROR_FUNC(csa, ERR_DBCMPNZRO, util_buff);
				return FALSE;
			}
			if (0 == blk_levl)
			{
				ch = *((sm_uc_ptr_t)rp + SIZEOF(rec_hdr));
				if (!(VALFIRSTCHAR_WITH_TRIG(ch)))
					GTMASSERT;
			}
		}
		if (r_top == (rec_hdr_ptr_t)blk_top && blk_levl)
		{	/* star key */
			if (rec_size != SIZEOF(rec_hdr) + SIZEOF(block_id))
			{
				RTS_ERROR_FUNC(csa, ERR_DBSTARSIZ, util_buff);
				return FALSE;
			}
			if (rec_cmpc)
			{
				RTS_ERROR_FUNC(csa, ERR_DBSTARCMP, util_buff);
				return FALSE;
			}
			blk_id_ptr = (sm_uc_ptr_t)rp + SIZEOF(rec_hdr);
		} else
		{	/* non-star key */
			key_base = (sm_uc_ptr_t)rp + SIZEOF(rec_hdr);
			/* num_subscripts = number of full subscripts found in the key (including the compressed part) */
			num_subscripts = 0;
			prev_char_is_delimiter = FALSE;
			if (rec_cmpc)
			{
				if (rec_cmpc >= prior_expkeylen)
				{
					RTS_ERROR_FUNC(csa, ERR_DBCMPMX, util_buff);
					return FALSE;
				}
				for (b_ptr = prior_expkey; b_ptr < (prior_expkey + rec_cmpc); b_ptr++)
				{
					if (KEY_DELIMITER == *b_ptr)
						num_subscripts++;
				}
				prev_char_is_delimiter = (KEY_DELIMITER == prior_expkey[rec_cmpc - 1]);
			}
			assert(key_base < (sm_uc_ptr_t)r_top);	/* otherwise we would have signalled ERR_DBRSIZMN error */
			for (blk_id_ptr = key_base ;  ; )
			{
				if (KEY_DELIMITER == *blk_id_ptr++)
				{
					if (!min_cmpc)
					{	/* note down the length of the global-variable (without subscripts) from the
						 * first key in the block. every other record in this block (except the star-key
						 * in case of an index block) should have a rec_cmpc that is atleast this much
						 * (of course this is TRUE only if we are in a GVT).
						 */
						min_cmpc = blk_id_ptr - key_base;
					}
					if (prev_char_is_delimiter)
						break;	/* found key terminator */
					prev_char_is_delimiter = TRUE;
					num_subscripts++;
				} else
					prev_char_is_delimiter = FALSE;
				if (blk_id_ptr >= (sm_uc_ptr_t)r_top)
				{
					RTS_ERROR_FUNC(csa, ERR_DBKEYMX, util_buff);
					return FALSE;
				}
			}
			num_subscripts--;	/* the global variable name was counted above as a subscript. adjust that */
			key_size = (int4)(blk_id_ptr - key_base);
			if (!first_key && (rec_cmpc < min_cmpc))
			{	/* name-level change between consecutive records in the block, this should be a directory block */
				if (is_gvt)
				{	/* this is a contradiction. a block cannot be a directory and gvt at the same time.
					 * gvt should contain all keys with the same global name */
					RTS_ERROR_FUNC(csa, ERR_DBINVGBL, util_buff);
					return FALSE;
				}
				is_directory = TRUE;	/* no need to do this if it was already TRUE but we save an if check */
			}
			if (num_subscripts)
			{	/* key has subscripts, should therefore be a GVT block */
				if (is_directory)
				{	/* this is a contradiction. a block cannot be a directory and gvt at the same time.
					 * the directory tree should contain only name-level (i.e. unsubscripted) globals */
					RTS_ERROR_FUNC(csa, ERR_DBDIRTSUBSC, util_buff);
					return FALSE;
				}
				is_gvt = TRUE;	/* no need to do this if it was already TRUE but we save an if check */
			}
			if (MAX_GVSUBSCRIPTS <= num_subscripts)
			{
				RTS_ERROR_FUNC(csa, ERR_DBMAXNRSUBS, util_buff);
				return FALSE;
			}
			if (blk_levl && (key_size != (rec_size - SIZEOF(block_id) - SIZEOF(rec_hdr))))
			{
				RTS_ERROR_FUNC(csa, ERR_DBKEYMN, util_buff);
				return FALSE;
			}
			assert(first_key || (rec_cmpc < prior_expkeylen));
			if (!first_key)
			{
				if (prior_expkey[rec_cmpc] == key_base[0])
				{
					RTS_ERROR_FUNC(csa, ERR_DBCMPBAD, util_buff);
					return FALSE;
				}
				if (((unsigned int)prior_expkey[rec_cmpc] >= (unsigned int)key_base[0]))
				{
					RTS_ERROR_FUNC(csa, ERR_DBKEYORD, util_buff);
					return FALSE;
				}
			}
			memcpy(prior_expkey + rec_cmpc, key_base, key_size);
			prior_expkeylen = rec_cmpc + key_size;
		}
		/* Check for proper child block numbers */
		if ((0 != blk_levl) || (0 != is_directory))
		{
			GET_LONG(child, blk_id_ptr);
			chain = *(off_chain *)&child;
			/* In TP, child block number can be greater than the total_blks for blocks created within TP.
			 * Dont do any checks on such blocks.
			 */
			if (!dollar_tlevel || !chain.flag)
			{
				if (child <= 0)
				{
					RTS_ERROR_FUNC(csa, ERR_DBPTRNOTPOS, util_buff);
					return FALSE;
				}
				if ((child > csa->ti->total_blks) && !mu_reorg_upgrd_dwngrd_in_prog)
				{	/* REORG -UPGRADE/DOWNGRADE can update recycled blocks, which may contain children beyond
					 * the total_blks if a truncate happened sometime after the block was killed.
					 */
					RTS_ERROR_FUNC(csa, ERR_DBPTRMX, util_buff);
					return FALSE;
				}
				if (!(child % bplmap))
				{
					RTS_ERROR_FUNC(csa, ERR_DBPTRMAP, util_buff);
					return FALSE;
				}
				if (child == prev_child)
				{
					RTS_ERROR_FUNC(csa, ERR_DBBDBALLOC, util_buff);
					return FALSE;
				}
				prev_child = child;
			} else
			{
				if ((blk_id_ptr != next_tp_child_ptr) && (NULL != next_tp_child_ptr))
				{
					RTS_ERROR_FUNC(csa, ERR_DBPTRNOTPOS, util_buff);
					return FALSE;
				}
				next_tp_child_ptr = blk_id_ptr + chain.next_off;
			}
		}
		first_key = FALSE;
		comp_length = prior_expkeylen;
	}
	assert(!is_directory || !is_gvt);	/* the block cannot be a directory AND gvt at the same time */
	return TRUE;
}
