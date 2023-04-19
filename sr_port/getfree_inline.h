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

#ifndef GETFREE_INLINE_INCLUDED
#define GETFREE_INLINE_INCLUDED

#include "t_qread.h"
#include "min_max.h"

#define SIMPLE_FIND_FREE_BLK(HINT, NOCRIT_PRESENT, UPGRADE)  simple_find_free(HINT, NOCRIT_PRESENT, UPGRADE)

static inline block_id simple_find_free(block_id hint, boolean_t nocrit_present, boolean_t upgrade)
{
	GBLREF gd_region	*gv_cur_region;
	GBLREF sgmnt_addrs	*cs_addrs;
	GBLREF sgmnt_data_ptr_t	cs_data;

	block_id	bml, extension_size, lmap_hint, master_bit, mmap_hint, ret, total_blks;
	boolean_t	in_last_bmap, was_crit, was_hold_onto_crit;
	cache_rec_ptr_t	dummy_cr;
	int4		bplmap, dummy_int, lmap_bit, status;
	sgmnt_addrs	*csa;
	sm_uc_ptr_t	lmap_base;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = cs_addrs;
	bplmap = csa->hdr->bplmap;
	mmap_hint = hint / bplmap;
	was_crit = csa->now_crit;
	DSE_GRAB_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, csa, gv_cur_region);
	total_blks = csa->ti->total_blks;
	master_bit = bmm_find_free(mmap_hint, csa->bmm, DIVIDE_ROUND_UP(total_blks, bplmap));
	in_last_bmap = (master_bit == (total_blks / bplmap));
	if ((NO_FREE_SPACE == master_bit) || (upgrade && in_last_bmap))
	{	/* upgrade does not risk getting free blocks lower than hint, as that can interfere with its purpose */
		if (!upgrade)
		{	/* DSE treats NO_FREE_SPACE as an error, while upgrade tries a file extension */
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, csa, gv_cur_region);
			return NO_FREE_SPACE;
		}
		extension_size = MAX(cs_data->extension_size, bplmap);
#		ifdef DEBUG
		TREF(in_bm_getfree_gdsfilext) = TRUE;
#		endif
		status = GDSFILEXT(extension_size, total_blks, TRANS_IN_PROG_FALSE);
#		ifdef DEBUG
		TREF(in_bm_getfree_gdsfilext) = FALSE;
#		endif
		if (SS_NORMAL != status)
			return status; /* GDSFILEXT returns sub-zero on error or rts_error()s out */
		if (dba_mm == cs_data->acc_meth)	/* MM: Warn the caller to remap of extensions */
			return (FILE_EXTENDED);
		total_blks = csa->total_blks;
		if (NO_FREE_SPACE == master_bit)
		{
			master_bit = bmm_find_free(mmap_hint, csa->bmm, DIVIDE_ROUND_UP(total_blks, bplmap));
			assert(NO_FREE_SPACE != master_bit); /* DB was just extended, this should not happen */
		}
		assert(master_bit < (total_blks / bplmap));
		in_last_bmap = FALSE;	/* Just extended the DB, cannot be in last local bitmap */
	}
	lmap_hint = (hint - (master_bit * bplmap));
	lmap_hint = (0 < lmap_hint) ? lmap_hint : 0; /* lmap_hint=0 implies searching the start of lmap */
	assert(bplmap > lmap_hint);
	lmap_base = t_qread(bml = (master_bit * bplmap), &dummy_int, &dummy_cr); /* WARNING assignment */
	while (TRUE)
	{	/* work the master map looking for appropriate space */
		if (NULL == lmap_base)
		{
			assert(!upgrade);
			DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, csa, gv_cur_region);
			return MAP_RD_FAIL;
		}
		lmap_bit = bml_find_free(lmap_hint, lmap_base + SIZEOF(blk_hdr), (!in_last_bmap) ?  bplmap : (total_blks - bml));
		if (lmap_bit >= lmap_hint)	/* Found free after hint */
			break;
		/* else lmap_bit < lmap_hint */
		if ((NO_FREE_SPACE == lmap_bit) || upgrade)
		{
			master_bit++;		/* Increment master_bit to move the next lmap */
			if ((total_blks / bplmap) < master_bit)
			{
				assert(!upgrade);
				DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, csa, gv_cur_region);
				return NO_FREE_SPACE;
			}
			lmap_hint = 0; /* Implies searching the start of lmap */
			lmap_base = t_qread(bml = (master_bit * bplmap), &dummy_int, &dummy_cr); /* inline assignment */
		}
		if (!upgrade) /* DSE _is_ allowed to return a block lower than the hint in the same bit map */
			break;
	}
	DSE_REL_CRIT_AS_APPROPRIATE(was_crit, was_hold_onto_crit, nocrit_present, csa, gv_cur_region);
	assert(lmap_bit);
	ret = bml + lmap_bit;
	assert(ret <= csa->total_blks);
	return ret;
}

#endif /* GETFREE_INLINE_INCLUDED */
