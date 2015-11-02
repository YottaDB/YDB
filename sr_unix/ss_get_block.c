/****************************************************************
 *								*
 *	Copyright 2009, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include "gtm_permissions.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_tempnam.h"
#include "gtm_time.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "eintr_wrappers.h"
#include "gtmio.h"
#include "shmpool.h"
#include "db_snapshot.h"
#include "wbox_test_init.h"

GBLREF	gd_region	*gv_cur_region;
/*
 * This function returns the before image of a block (if it exists in the snapshot file)
 */
boolean_t ss_get_block(sgmnt_addrs *csa, block_id blk, sm_uc_ptr_t blk_buff_ptr)
{
	shm_snapshot_ptr_t		ss_shm_ptr;
	snapshot_context_ptr_t		lcl_ss_ctx;

	lcl_ss_ctx = SS_CTX_CAST(csa->ss_ctx);
	assert(NULL != lcl_ss_ctx);
	ss_shm_ptr = lcl_ss_ctx->ss_shm_ptr;
	DBG_ENSURE_PTR_WITHIN_SS_BOUNDS(csa, (sm_uc_ptr_t)ss_shm_ptr);
	assert(ss_shm_ptr->in_use && SNAPSHOTS_IN_PROG(csa->nl));
	/* Ensure that we never try to request a block that is outside the snapshot range. The only exception is when GT.M
	 * failed while initializing snapshot resources and hence refrained from writing before images. Include whitebox
	 * test case to account for invalid snapshot
	 */
	assert((blk < ss_shm_ptr->ss_info.total_blks) || (WBTEST_INVALID_SNAPSHOT_EXPECTED == gtm_white_box_test_case_number));
	if (blk >= ss_shm_ptr->ss_info.total_blks)
		return FALSE;
	if (ss_chk_shdw_bitmap(csa, lcl_ss_ctx, blk))
	{
		ss_read_block(lcl_ss_ctx, blk, blk_buff_ptr); /* no return in case of failure */
		return TRUE;
	}
	return FALSE;
}
