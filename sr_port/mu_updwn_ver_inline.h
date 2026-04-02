/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_UPDWN_VER_INLINE_INCLUDED
#define MU_UPDWN_VER_INLINE_INCLUDED
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "gds_blk_upgrade_inline.h"
#include "mm_read.h"

GBLREF	uint4			mu_upgrade_in_prog;		/* non-zero if MUPIP REORG UPGRADE/DOWNGRADE is in progress */
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_data_ptr_t	cs_data;

#define INCR_BLKS_TO_UPGRD(CSA, CSD, DELTA)	incr_blks_to_upgrd((CSA), (CSD), (DELTA))
#define DECR_BLKS_TO_UPGRD(CSA, CSD, DELTA)	incr_blks_to_upgrd((CSA), (CSD), -(DELTA))

#define	UPGRD_BLK_NOT_FOUND 	(-1)
#define UPGRD_PTR_ADJUST_ERR	(-2)

static inline void incr_blks_to_upgrd(sgmnt_addrs *csa, sgmnt_data *csd, int delta)
{
	block_id	new_blks_to_upgrd;
	block_id	cur_delta;
#ifdef	DEBUG_BLKS_TO_UPGRD
	static int	corecount = 0;
#endif

	assert(0 == csd->blks_to_upgrd_subzero_error);		/* callers should ensure count has not gone negative */
	assert(CREATE_IN_PROGRESS(csd) || csa->now_crit);
	assert(csa->hdr == csd);
	cur_delta = delta;
	assert(0 != cur_delta);
	new_blks_to_upgrd = cur_delta + csd->blks_to_upgrd;
	if (0 < new_blks_to_upgrd)
		csd->blks_to_upgrd = new_blks_to_upgrd;
	else /* (0 >= new_blks_to_upgrd) */
	{
		if (0 == new_blks_to_upgrd)
			csd->tn_upgrd_blks_0 = csd->trans_hist.curr_tn;
		else
		{	/* blks_to_upgrd counter in the fileheader should never hold a negative value.
			 * Note down the negative value in a separate field for debugging and set the counter to 0.
			 */
#ifdef			DEBUG_BLKS_TO_UPGRD
			if (5 > corecount++)
				gtm_fork_n_core(); /* This should never happen */
#endif
			csd->blks_to_upgrd_subzero_error -= new_blks_to_upgrd;
		}
		csd->blks_to_upgrd = 0;
	}
}

static inline int upgrade_mm_block(block_id blk, enum db_ver desired_db_format)
{
	enum db_ver 	ondsk_blkver;
	unsigned char 	level;
	sm_uc_ptr_t	blkBase;

	assert(cs_addrs->now_crit);
	assert(dba_mm == gv_cur_region->dyn.addr->acc_meth);
	blkBase = mm_read(blk);
	if (NULL == blkBase)
		return UPGRD_BLK_NOT_FOUND;
	ondsk_blkver = ((blk_hdr_ptr_t)blkBase)->bver;
	assert(!gv_cur_region->read_only);
	level = ((blk_hdr_ptr_t)blkBase)->levl;
	assert(GDSV4 != ondsk_blkver);
	switch (level)
	{
		case LCL_MAP_LEVL:
			if ((GDSV7m > ondsk_blkver) && (GDSV7m == desired_db_format))
			{
				ondsk_blkver = ((blk_hdr_ptr_t)blkBase)->bver = GDSV7m;
				((blk_hdr_ptr_t)blkBase)->tn = cs_data->trans_hist.curr_tn;
			}
			break;
		case 0:
			break;
		default:
			assert(((signed char)level) > 0);
			if (GDSV6p > ondsk_blkver)
			{
				if (TRUE == blk_ptr_adjust(blkBase, cs_data->offset))
				{
					ondsk_blkver = ((blk_hdr_ptr_t)blkBase)->bver = GDSV6p;
					((blk_hdr_ptr_t)blkBase)->tn = cs_data->trans_hist.curr_tn;
				} else
					return UPGRD_PTR_ADJUST_ERR;
			}
	}
	return 0;
}


#endif /* MU_UPDWN_VER_INLINE_INCLUDED */
