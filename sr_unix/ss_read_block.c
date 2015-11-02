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
#include "error.h"
#include "eintr_wrappers.h"
#include "gtmio.h"
#include "shmpool.h"
#include "db_snapshot.h"
#include "mupip_exit.h"

/*
 * This function is called to read the before image from the snapshot file
 */
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	gd_region	*gv_cur_region;
GTM_SNAPSHOT_ONLY(GBLDEF util_snapshot_ptr_t	util_ss_ptr;)

void	ss_read_block(snapshot_context_ptr_t lcl_ss_ctx, block_id blk, sm_uc_ptr_t blk_buff_ptr)
{

	int			blk_size, pread_res;
	boolean_t		issue_error;
	off_t			blk_offset;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	shm_snapshot_ptr_t	ss_shm_ptr;

	error_def(ERR_SSPREMATEOF);
	error_def(ERR_SSFILOPERR);
	error_def(ERR_INTEGERRS);

	assert(-1 != lcl_ss_ctx->shdw_fd);
	csa = cs_addrs;
	csd = csa->hdr;
	ss_shm_ptr = lcl_ss_ctx->ss_shm_ptr;
	DBG_ENSURE_PTR_WITHIN_SS_BOUNDS(csa, (sm_uc_ptr_t)ss_shm_ptr);
	blk_size = csd->blk_size;
	blk_offset = (off_t)(lcl_ss_ctx->shadow_vbn - 1) * DISK_BLOCK_SIZE + (off_t)blk * blk_size;
	LSEEKREAD(lcl_ss_ctx->shdw_fd, blk_offset, (uchar_ptr_t) blk_buff_ptr, blk_size, pread_res);
	if (0 != pread_res)
	{
		if (-1 == pread_res)
		{
			gtm_putmsg(VARLSTCNT(7) ERR_SSPREMATEOF, 5, blk, blk_size, blk_offset,
				LEN_AND_STR(ss_shm_ptr->ss_info.shadow_file));
		} else
		{
			gtm_putmsg(VARLSTCNT(7) ERR_SSFILOPERR, 4, LEN_AND_LIT("read"),
				LEN_AND_STR(ss_shm_ptr->ss_info.shadow_file), pread_res);
		}
		mupip_exit(ERR_INTEGERRS);
	}
}
