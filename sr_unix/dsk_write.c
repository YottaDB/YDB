/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include <signal.h>
#include <errno.h>
#ifdef DEBUG
#include "gtm_stdio.h"
#endif

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
#include "gdsbml.h"
#include "jnl.h"
#include "anticipatory_freeze.h"

GBLREF	sm_uc_ptr_t	reformat_buffer;
GBLREF	int		reformat_buffer_len;
GBLREF	volatile int	reformat_buffer_in_use;	/* used only in DEBUG mode */
GBLREF	int		fast_lock_count;
GBLREF	boolean_t	dse_running;

int	dsk_write(gd_region *reg, block_id blk, cache_rec_ptr_t cr)
{
	unix_db_info		*udi;
	int4			size, save_errno;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sm_uc_ptr_t		buff;
	DEBUG_ONLY(
		blk_hdr_ptr_t	blk_hdr;
	)

	udi = (unix_db_info *)(reg->dyn.addr->file_cntl->file_info);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	assert(NULL != csd);
	assert(cr);
	assert(cr->buffaddr);
	buff = GDS_ANY_REL2ABS(csa, cr->buffaddr);
	DEBUG_ONLY(
		/* Check GDS block that is about to be written. Dont do this for DSE as it may intentionally create bad blocks */
		if (!dse_running)
		{
			blk_hdr = (blk_hdr_ptr_t)buff;
			assert((unsigned)GDSVLAST > (unsigned)blk_hdr->bver);
			assert((LCL_MAP_LEVL == blk_hdr->levl) || ((unsigned)MAX_BT_DEPTH > (unsigned)blk_hdr->levl));
			assert((unsigned)csd->blk_size >= (unsigned)blk_hdr->bsiz);
			assert(csd->trans_hist.curr_tn >= blk_hdr->tn);
		}
	)
	assert(((blk_hdr_ptr_t)buff)->bver);	/* GDSV4 (0) version uses this field as a block length so should always be > 0 */
	assert(0 == fast_lock_count); /* ensure the static reformat buffer is not being used currently */
	++fast_lock_count; 	/* Prevents interrupt from using reformat buffer while we have it */
	/* reformat_buffer_in_use should always be incremented only AFTER incrementing fast_lock_count
	 * as it is the latter that prevents interrupts from using the reformat buffer. Similarly
	 * the decrement of fast_lock_count should be done AFTER decrementing reformat_buffer_in_use.
	 */
	assert(0 == reformat_buffer_in_use);
	DEBUG_ONLY(reformat_buffer_in_use++;)
	if (IS_GDS_BLK_DOWNGRADE_NEEDED(cr->ondsk_blkver))
	{	/* Need to downgrade/reformat this block back to the previous format */
		DEBUG_DYNGRD_ONLY(PRINTF("DSK_WRITE: Block %d being dynamically downgraded on write\n", blk));
		if (csd->blk_size > reformat_buffer_len)
		{	/* Buffer not big enough (or does not exist) .. get a new one releasing old if it exists */
			if (reformat_buffer)
				free(reformat_buffer);	/* Different blksized databases in use .. keep only largest one */
			reformat_buffer = malloc(csd->blk_size);
			reformat_buffer_len = csd->blk_size;
		}
		gds_blk_downgrade((v15_blk_hdr_ptr_t)reformat_buffer, (blk_hdr_ptr_t)buff);
		buff = reformat_buffer;
		size = ((v15_blk_hdr_ptr_t)buff)->bsiz;
		assert(size <= csd->blk_size - SIZEOF(blk_hdr) + SIZEOF(v15_blk_hdr));
		size = (size + 1) & ~1;
		assert(SIZEOF(v15_blk_hdr) <= size);
	} else DEBUG_ONLY(if (GDSV6 == cr->ondsk_blkver))
	{
		size = (((blk_hdr_ptr_t)buff)->bsiz + 1) & ~1;
		assert(SIZEOF(blk_hdr) <= size);
	}
	DEBUG_ONLY(else GTMASSERT);
	if (csa->do_fullblockwrites)
		/* round size up to next full logical filesys block. */
		size = (int)ROUND_UP(size, csa->fullblockwrite_len);
	assert(size <= csd->blk_size);
	assert(FALSE == reg->read_only);
	assert(dba_bg == reg->dyn.addr->acc_meth);
	assert(!csa->acc_meth.bg.cache_state->cache_array || buff != (sm_uc_ptr_t)csd);
	assert(!csa->acc_meth.bg.cache_state->cache_array
	       || (buff >= (sm_uc_ptr_t)csa->acc_meth.bg.cache_state->cache_array
		   + (SIZEOF(cache_rec) * (csd->bt_buckets + csd->n_bts))));
	assert(buff < (sm_uc_ptr_t)csd || buff == reformat_buffer);
		/* assumes hdr follows immediately after the buffer pool in shared memory */
	assert(size <= csd->blk_size);
	if (udi->raw)
		size = ROUND_UP(size, DISK_BLOCK_SIZE);	/* raw I/O must be a multiple of DISK_BLOCK_SIZE */
	DB_LSEEKWRITE(csa, udi->fn, udi->fd,
		   (DISK_BLOCK_SIZE * (csd->start_vbn - 1) + (off_t)blk * csd->blk_size),
		   buff, size, save_errno);
	DEBUG_ONLY(reformat_buffer_in_use--;)
	assert(0 == reformat_buffer_in_use);
	--fast_lock_count;
	assert(0 == fast_lock_count);
	if (0 != save_errno)		/* If it didn't work for whatever reason.. */
		return -1;
	return SS_NORMAL;
}
