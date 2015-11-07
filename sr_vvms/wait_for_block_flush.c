/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* This function is called from t_qread to wait out a "flushing" state.
   It assumes that the database is clustered. */

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "ccp.h"
#include "filestruct.h"

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF gd_region	*gv_cur_region;

void wait_for_block_flush(bt_rec *bt, block_id block)
{
	register sgmnt_addrs	*csa;
	unsigned short		cycle;

	csa = cs_addrs;
	assert(csa->hdr->clustered);

	for (;(bt->blk == block) && bt->flushing && !CCP_SEGMENT_STATE(csa->nl,CCST_MASK_HAVE_DIRTY_BUFFERS);)
	{	/* as int4 as the bt and block match, the bt shows flushing, and the ccp state indicates */
		cycle = csa->nl->ccp_cycle;
		CCP_FID_MSG(gv_cur_region, CCTR_FLUSHLK);
		ccp_userwait(gv_cur_region, CCST_MASK_HAVE_DIRTY_BUFFERS, 0, cycle);
	}
	return;
}
