/****************************************************************
 *								*
 *	Copyright 2009, 2013 Fidelity Information Services, Inc	*
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
#include "iosp.h"
#include "error.h"
#include "cli.h"
#include "gtmio.h"
#include "send_msg.h"
#include "shmpool.h"
#include "db_snapshot.h"

GBLREF	uint4		process_id;

error_def(ERR_SSFILOPERR);

boolean_t	ss_write_block(sgmnt_addrs *csa,
				block_id blk,
				cache_rec_ptr_t cr,
				sm_uc_ptr_t mm_blk_ptr,
				snapshot_context_ptr_t lcl_ss_ctx)
{
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	sm_uc_ptr_t		blk_ptr;
	shm_snapshot_ptr_t	ss_shm_ptr;
	int			save_errno;
	uint4			size, blk_size;
	off_t			blk_offset;
	boolean_t		is_bg;
	DEBUG_ONLY(
		blk_hdr_ptr_t	save_blk_ptr;
	)

	assert(NULL != lcl_ss_ctx);
	csd = csa->hdr;
	cnl = csa->nl;
	is_bg = (dba_bg == csd->acc_meth);
	assert(is_bg || (dba_mm == csd->acc_meth));
	assert((is_bg && (NULL != cr) && (NULL == mm_blk_ptr)) || (!is_bg && (NULL == cr) && (NULL != mm_blk_ptr)));
	ss_shm_ptr = lcl_ss_ctx->ss_shm_ptr;
	DBG_ENSURE_PTR_WITHIN_SS_BOUNDS(csa, (sm_uc_ptr_t)ss_shm_ptr);
	if (ss_shm_ptr->failure_errno)
	{
		/* A prior GT.M process has encountered an error and hence the snapshot is invalid and hence there is
		 * no use in continuing with the before image writes. So, do early return.
		 */
		return FALSE;
	}
	assert(cnl->ss_shmid == lcl_ss_ctx->attach_shmid);
	assert(ss_shm_ptr->ss_info.ss_shmid == lcl_ss_ctx->attach_shmid);
	/* ss_release (function that invalidates a snapshot and announces GT.M not to write any more
	 * before images) waits for the active phase 2 commits to complete and hence the below
	 * assert is safe to be used.
	 */
	assert(ss_shm_ptr->in_use && SNAPSHOTS_IN_PROG(csa));
	assert(!is_bg || ((NULL != cr) && cr->in_cw_set)); /* ensure the buffer has been pinned (from preemption in db_csh_getn) */
	blk_size = (uint4)csd->blk_size;
	blk_ptr = is_bg ? GDS_ANY_REL2ABS(csa, cr->buffaddr) : mm_blk_ptr;
#	ifdef GTM_CRYPT
	/* If the database is encrypted, the old_block will be in the encrypted twin buffer. Logic similar to the one
	 * done in backup_block.c
	 */
	if (csd->is_encrypted)
	{
		assert(is_bg);
		DBG_ENSURE_PTR_IS_VALID_GLOBUFF(csa, csd, blk_ptr);
		DEBUG_ONLY(save_blk_ptr = (blk_hdr_ptr_t)blk_ptr;)
		blk_ptr = GDS_ANY_ENCRYPTGLOBUF(blk_ptr, csa);
		/* Ensure that the unencrypted buffer (save_blk_ptr) and the encrypted twin buffer (blk_ptr) are indeed
		 * holding the same block
		 */
		assert(save_blk_ptr->tn == ((blk_hdr_ptr_t)blk_ptr)->tn);
		assert(save_blk_ptr->bsiz == ((blk_hdr_ptr_t)blk_ptr)->bsiz);
		assert(save_blk_ptr->levl == ((blk_hdr_ptr_t)blk_ptr)->levl);
		DBG_ENSURE_PTR_IS_VALID_ENCTWINGLOBUFF(csa, csd, blk_ptr);
	}
#	endif
	assert(NULL != blk_ptr);
	size = ((blk_hdr_ptr_t)blk_ptr)->bsiz;
	if (csa->do_fullblockwrites)
		size = ROUND_UP(size, csa->fullblockwrite_len);
	/* If the block is FREE and block size is zero, we don't want to issue an empty write below. Instead write block of size
	 * equal to the database block size.
	 */
	if (!size || (size > blk_size))
		size = blk_size;
	assert(size <= ss_shm_ptr->ss_info.db_blk_size);
	blk_offset = ((off_t)(lcl_ss_ctx->shadow_vbn - 1) * DISK_BLOCK_SIZE + (off_t)blk * blk_size);
	/* Note: If a FREE block is being written here, then we could avoid the write below: if the underlying file system
	 * is guaranteed to give us all zeros for a block and if the block header is empty
	 */
	assert(-1 != lcl_ss_ctx->shdw_fd);
	LSEEKWRITE(lcl_ss_ctx->shdw_fd, blk_offset, blk_ptr, size, save_errno);
	if ((0 != save_errno) && SNAPSHOTS_IN_PROG(cnl))
	{
		assert(FALSE);
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_SSFILOPERR, 4, LEN_AND_LIT("write"),
			LEN_AND_STR(lcl_ss_ctx->shadow_file),
			save_errno);
		ss_shm_ptr->failed_pid = process_id;
		ss_shm_ptr->failure_errno = save_errno;
		return FALSE;
	}
	/* Mark the block as before imaged in the bitmap */
	ss_set_shdw_bitmap(csa, lcl_ss_ctx, blk);
	return TRUE;
}
