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

#include "mdef.h"

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "iosp.h"
#include "gtmio.h"

GBLDEF	bool	non_buffer_write;

int	dsk_write (gd_region *r, block_id blk, sm_uc_ptr_t buff)
{
	unix_db_info	*udi;
	int4		size, save_errno;
	sgmnt_addrs	*csa;

	udi = (unix_db_info *)(r->dyn.addr->file_cntl->file_info);
	csa = &udi->s_addrs;
#ifdef FULLBLOCKWRITES
	size = csa->hdr->blk_size;
#else
	size = ((((blk_hdr_ptr_t)buff)->bsiz + 1) & ~1);
#endif
	assert(r->read_only == FALSE);
	assert(r->dyn.addr->acc_meth == dba_bg || non_buffer_write);
	assert(!csa->acc_meth.bg.cache_state->cache_array || buff != (sm_uc_ptr_t)csa->hdr);
	assert(non_buffer_write || !csa->acc_meth.bg.cache_state->cache_array
			|| (buff >= (sm_uc_ptr_t)csa->acc_meth.bg.cache_state->cache_array
					+ (sizeof(cache_rec) * (csa->hdr->bt_buckets + csa->hdr->n_bts))));
	assert(non_buffer_write || !csa->critical || buff < (sm_uc_ptr_t)csa->critical);
		/* assumes critical follows immediately after the buffer pool */
	assert(size >= sizeof(blk_hdr));
	assert(size <= csa->hdr->blk_size);

	if (udi->raw)
		size = ROUND_UP(size, DISK_BLOCK_SIZE);	/* raw I/O must be a multiple of DISK_BLOCK_SIZE */

	LSEEKWRITE(udi->fd,
		   (DISK_BLOCK_SIZE * (csa->hdr->start_vbn - 1) + (off_t)blk * csa->hdr->blk_size),
		   buff,
		   size,
		   save_errno);

	if (0 != save_errno)		/* If it didn't work for whatever reason.. */
	{
		return -1;
	}

	return SS_NORMAL;
}
