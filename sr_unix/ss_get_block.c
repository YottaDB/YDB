/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
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

GBLREF	gd_region	*gv_cur_region;
/*
 * This function returns the before image of a block (if it exists in the snapshot file)
 */
boolean_t ss_get_block(sgmnt_addrs *csa,
			snapshot_context_ptr_t lcl_ss_ctx,
			block_id blk,
			sm_uc_ptr_t blk_buff_ptr,
			boolean_t *read_failed)
{
	shm_snapshot_ptr_t		ss_shm_ptr;

	error_def(ERR_REGSSFAIL);

	assert(NULL != lcl_ss_ctx);
	ss_shm_ptr = lcl_ss_ctx->ss_shm_ptr;
	DBG_ENSURE_PTR_WITHIN_SS_BOUNDS(csa, (sm_uc_ptr_t)ss_shm_ptr);
	*read_failed = FALSE;
	/* Issue error if GT.M failed while writing blocks to snapshot file */
	if (ss_shm_ptr->failure_errno)
	{
		/* Detailed error will be issued in the operator log by GT.M */
		gtm_putmsg(VARLSTCNT(6) ERR_REGSSFAIL, 3, ss_shm_ptr->failed_pid, DB_LEN_STR(gv_cur_region),
			ss_shm_ptr->failure_errno);
		*read_failed = TRUE;
		return FALSE;
	}
	assert(ss_shm_ptr->in_use && SNAPSHOTS_IN_PROG(csa->nl));
	assert(blk < ss_shm_ptr->ss_info.total_blks);
	if (blk >= ss_shm_ptr->ss_info.total_blks)
		return FALSE;
	if (ss_chk_shdw_bitmap(csa, lcl_ss_ctx, blk))
	{
		if (!ss_read_block(lcl_ss_ctx, blk, blk_buff_ptr))
			*read_failed = TRUE;
		return TRUE;
	}
	return FALSE;
}
