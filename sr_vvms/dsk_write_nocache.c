/****************************************************************
 *								*
 *	Copyright 2005, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rms.h>
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include <iodef.h>
#include "efn.h"
#include <ssdef.h>
#include "iosb_disk.h"
#include "iosp.h"
#include "filestruct.h"
#include "gtm_malloc.h"		/* for CHECK_CHANNEL_STATUS macro */
#include "gds_blk_downgrade.h"

/***********************************************************************************
 * WARNING: This routine does not manage the number of outstanding AST available.
 * The calling routine is responsible for that.
 ***********************************************************************************/

GBLREF	sm_uc_ptr_t	reformat_buffer;
GBLREF	int		reformat_buffer_len;
GBLREF	volatile int	reformat_buffer_in_use;	/* used only in DEBUG mode */
GBLREF	volatile int4	fast_lock_count;

int4	dsk_write_nocache(gd_region *reg, block_id blk, sm_uc_ptr_t buff, enum db_ver ondsk_blkver,
			  void (*ast_rtn)(), int4 ast_param, io_status_block_disk *iosb)
{
	int4			size, status;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	boolean_t		reformat;
	uint4			channel_id;

	csa = &((vms_gds_info*)(reg->dyn.addr->file_cntl->file_info))->s_addrs;
	size = (((blk_hdr_ptr_t)buff)->bsiz + 1) & ~1;
	csd = csa->hdr;
	assert(NULL != csd);
	assert(((blk_hdr_ptr_t)buff)->bver);	/* GDSV4 (0) version uses this field as a block length so should always be > 0 */
	reformat = FALSE;
	if (IS_GDS_BLK_DOWNGRADE_NEEDED(ondsk_blkver))
	{	/* Need to downgrade/reformat this block back to a previous format */
		assert(0 == fast_lock_count);	/* since this is mainline (non-interrupt) code */
		++fast_lock_count; 			/* Prevents interrupt from using reformat buffer while we have it */
		DEBUG_DYNGRD_ONLY(PRINTF("DSK_WRITE_NOCACHE: Block %d being dynamically downgraded on write\n", blk));
		reformat = TRUE;
		/* reformat_buffer_in_use should always be incremented only AFTER incrementing fast_lock_count
		 * as it is the latter that prevents interrupts from using the reformat buffer. Similarly
		 * the decrement of fast_lock_count should be done AFTER decrementing reformat_buffer_in_use.
		 */
		assert(0 == reformat_buffer_in_use);
		DEBUG_ONLY(reformat_buffer_in_use++;)
		if (csd->blk_size > reformat_buffer_len)
		{	/* Buffer not big enough (or does not exist) .. get a new one releasing old if it exists */
			if (reformat_buffer)
				free(reformat_buffer);
			reformat_buffer = malloc(csd->blk_size);
			reformat_buffer_len = csd->blk_size;
		}
		gds_blk_downgrade((v15_blk_hdr_ptr_t)reformat_buffer, (blk_hdr_ptr_t)buff);
		buff = reformat_buffer;
		size = (((v15_blk_hdr_ptr_t)buff)->bsiz + 1) & ~1;
		assert(SIZEOF(v15_blk_hdr) <= size);
		/* Represents a state change from V5 -> V4 format */
		INCR_BLKS_TO_UPGRD(csa, csd, 1);
	} else DEBUG_ONLY(if (GDSV6 == ondsk_blkver))
	{
		size = (((blk_hdr_ptr_t)buff)->bsiz + 1) & ~1;
		assert(SIZEOF(blk_hdr) <= size);
		/* no adjustment to blks_to_upgrd counter is needed since the format we are going to write is GDSVCURR */
	}
	DEBUG_ONLY(else GTMASSERT);
	if (csa->do_fullblockwrites)
		size = ROUND_UP(size, csa->fullblockwrite_len);	/* round size up to next full logical filesys block. */
	assert(size <= csd->blk_size);
	assert(FALSE == reg->read_only);
	assert((size / 2 * 2) == size);
	assert(0 == (((uint4) buff) & 7));	/* some disk controllers require quadword aligned xfers */
	assert(!csa->acc_meth.bg.cache_state->cache_array || buff != csd);
	assert(size <= csd->blk_size);
	channel_id = ((vms_gds_info*)(reg->dyn.addr->file_cntl->file_info))->fab->fab$l_stv;
	status = sys$qiow(efn_bg_qio_write, channel_id, IO$_WRITEVBLK, iosb ,ast_rtn, ast_param, buff, size,
			  (csd->blk_size / DISK_BLOCK_SIZE) * blk + csd->start_vbn, 0, 0, 0);
	if (reformat)
	{
		DEBUG_ONLY(
			reformat_buffer_in_use--;
			assert(0 == reformat_buffer_in_use);
		)
		--fast_lock_count;
		assert(0 == fast_lock_count);
	}
	CHECK_CHANNEL_STATUS(status, channel_id);
	return status;
}
