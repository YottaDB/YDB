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

GBLREF	uint4			mu_upgrade_in_prog;		/* non-zero if MUPIP REORG UPGRADE/DOWNGRADE is in progress */

#define INCR_BLKS_TO_UPGRD(CSA, CSD, DELTA)	incr_blks_to_upgrd((CSA), (CSD), (DELTA))
#define DECR_BLKS_TO_UPGRD(CSA, CSD, DELTA)	incr_blks_to_upgrd((CSA), (CSD), -(DELTA))

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

#endif /* MU_UPDWN_VER_INLINE_INCLUDED */
