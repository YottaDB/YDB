/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/types.h>
#include "gtm_unistd.h"
#include "gtm_stdio.h"
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
#include "gds_blk_downgrade.h"
#include "add_inter.h"

GBLREF	sm_uc_ptr_t	reformat_buffer;
GBLREF	int		reformat_buffer_len;
GBLREF	volatile int	reformat_buffer_in_use;	/* used only in DEBUG mode */
GBLREF	int		fast_lock_count;

/* Similiar to dsk_write but differs in two important ways:
   1) We write direct from the given buffer rather than from a cache record's buffer.
   2) This routine takes care of the maint of blks_to_upgrd in the file-header for
      these non-cached writes.
*/
int	dsk_write_nocache(gd_region *reg, block_id blk, sm_uc_ptr_t buff, enum db_ver ondsk_blkver)
{
	unix_db_info		*udi;
	int4			size, save_errno;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;

	udi = (unix_db_info *)(reg->dyn.addr->file_cntl->file_info);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	assert(NULL != csd);
	assert(((blk_hdr_ptr_t)buff)->bver);	/* GDSV4 (0) version uses this field as a block length so should always be > 0 */
	assert(0 == fast_lock_count); /* ensure the static reformat buffer is not being used currently */
	++fast_lock_count; 	/* Prevents interrupt from using reformat buffer while we have it */
	/* reformat_buffer_in_use should always be incremented only AFTER incrementing fast_lock_count
	 * as it is the latter that prevents interrupts from using the reformat buffer. Similarly
	 * the decrement of fast_lock_count should be done AFTER decrementing reformat_buffer_in_use.
	 */
	assert(0 == reformat_buffer_in_use);
	DEBUG_ONLY(reformat_buffer_in_use++;)
	if (IS_GDS_BLK_DOWNGRADE_NEEDED(ondsk_blkver))
	{	/* Need to downgrade/reformat this block back to the previous format */
		DEBUG_DYNGRD_ONLY(PRINTF("DSK_WRITE_NOCACHE: Block %d being dynamically downgraded on write\n", blk));
		if (csd->blk_size > reformat_buffer_len)
		{	/* Buffer not big enough (or does not exist) .. get a new one releasing old if it exists */
			if (reformat_buffer)
				free(reformat_buffer);	/* Different blksized databases in use .. keep only largest one */
			reformat_buffer = malloc(csd->blk_size);
			reformat_buffer_len = csd->blk_size;
		}
		gds_blk_downgrade((v15_blk_hdr_ptr_t)reformat_buffer, (blk_hdr_ptr_t)buff);
		buff = reformat_buffer;
		size = (((v15_blk_hdr_ptr_t)buff)->bsiz + 1) & ~1;
		/* Represents a block state change from V5 -> V4 */
		INCR_BLKS_TO_UPGRD(csa, csd, 1);
		assert(sizeof(v15_blk_hdr) <= size);
	} else DEBUG_ONLY(if (GDSV5 == ondsk_blkver))
	{
		size = (((blk_hdr_ptr_t)buff)->bsiz + 1) & ~1;
		assert(sizeof(blk_hdr) <= size);
		/* no adjustment to blks_to_upgrd counter is needed since the format we are going to write is GDSVCURR */
	}
	DEBUG_ONLY(else GTMASSERT);
	if (csa->do_fullblockwrites)
		size = ROUND_UP(size, csa->fullblockwrite_len); /* round size up to next full logical filesys block. */
	assert(size <= csd->blk_size);
	assert(FALSE == reg->read_only);
	assert(!csa->acc_meth.bg.cache_state->cache_array || buff != (sm_uc_ptr_t)csd);
	assert(size <= csd->blk_size);
	if (udi->raw)
		size = ROUND_UP(size, DISK_BLOCK_SIZE);	/* raw I/O must be a multiple of DISK_BLOCK_SIZE */
	LSEEKWRITE(udi->fd,
		   (DISK_BLOCK_SIZE * (csd->start_vbn - 1) + (off_t)blk * csd->blk_size),
		   buff,
		   size,
		   save_errno);
	DEBUG_ONLY(reformat_buffer_in_use--;)
	assert(0 == reformat_buffer_in_use);
	--fast_lock_count; 		/* reformat buffer is no longer necessary */
	assert(0 == fast_lock_count);
	if (0 != save_errno)		/* If it didn't work for whatever reason.. */
		return -1;
	return SS_NORMAL;
}
