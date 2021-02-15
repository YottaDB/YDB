/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdsbml.h"
#include "sec_shr_map_build.h"
#include "min_max.h"
#include "bit_set.h"
#include "bit_clear.h"

int sec_shr_map_build(sgmnt_addrs *csa, block_id *array, unsigned char *base_addr, cw_set_element *cs, trans_num ctn)
{
	unsigned char		*ptr;
	block_id		blk, bitnum;
	int4			bml_full, bplmap, total_blks;	/* total_blks here represents how many blocks are covered by a
								 * local bit map and so will always be less then BLKS_PER_LMAP (512)
								 */
	node_local_ptr_t	cnl;
	uint4			setbit, prev;
	uint4			(*bml_func)();
	sgmnt_data_ptr_t	csd;
#	ifdef DEBUG
	int4			actual_cnt = 0;
	block_id		prev_bitnum = -1;
#	endif

	blk = cs->blk;
	assert(csa->now_crit && (ctn == csa->hdr->trans_hist.curr_tn));
	((blk_hdr_ptr_t)base_addr)->tn = ctn;
	base_addr += SIZEOF(blk_hdr);
	csd = csa->hdr;
	cnl = csa->nl;
	bplmap = csd->bplmap;
	DETERMINE_BML_FUNC(bml_func, cs, csa);
	for (;;)
	{
		bitnum = *array;
		assert(bitnum == (int4)bitnum);	/* Verify cast in the next assert */
		assert((int4)bitnum < bplmap);	/* check that bitnum is positive and within 0 to bplmap */
		if (0 == bitnum)
		{
			assert(actual_cnt == cs->reference_cnt);
			break;
		}
		assert(bitnum > prev_bitnum);	/* assert that blocks are sorted in the update array */
		DEBUG_ONLY(prev_bitnum = bitnum;)
		setbit = bitnum * BML_BITS_PER_BLK;
		ptr = base_addr + setbit / 8;
		setbit &= 7;
		if (bml_busy == bml_func)
		{
			*ptr &= ~(3 << setbit);	/* mark block as BUSY (00) */
			DEBUG_ONLY(actual_cnt++;)
		} else
		{
#			ifdef DEBUG
			prev = ((*ptr >> setbit) & 1);	/* prev is 0 is block WAS busy and 0 otherwise */
			if (!prev)
				actual_cnt--;
#			endif
			if (bml_recycled == bml_func)
				*ptr |= (3 << setbit);	/* mark block as RECYCLED (11) */
			else
			{	/* mark block as FREE (01) */
				*ptr &= ~(3 << setbit);	/* first mark block as BUSY (00) */
				*ptr |= (1 << setbit);	/* then  mark block as FREE (01) */
			}
		}
		++array;
	}
	/* Fix the local bitmap full/free status in the mastermap */
	/* The result of (csd->trans_hist.total_blks - blk) can be cast to int4
	 * because it should never be larger then BLKS_PER_LMAP
	 */
	assert((BLKS_PER_LMAP >= (csd->trans_hist.total_blks - blk)) ||
			((csd->trans_hist.total_blks / bplmap) * bplmap != blk));
	total_blks = ((csd->trans_hist.total_blks / bplmap) * bplmap == blk) ? (int4)(csd->trans_hist.total_blks - blk) : bplmap;
	bml_full = bml_find_free(0, base_addr, total_blks);
	if (NO_FREE_SPACE == bml_full)
	{
		bit_clear(blk / bplmap, MM_ADDR(csd));
		if (blk > cnl->highest_lbm_blk_changed)
			cnl->highest_lbm_blk_changed = blk;
	} else if (!(bit_set(blk / bplmap, MM_ADDR(csd))))
	{
		if (blk > cnl->highest_lbm_blk_changed)
			cnl->highest_lbm_blk_changed = blk;
	}
	return TRUE;
}
