/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "dbfilop.h"
#include "gdsblk.h"
#include "gds_blk_downgrade.h"
#include "mupint.h"

GBLREF	sm_uc_ptr_t	reformat_buffer;
GBLREF	int		reformat_buffer_len;
GBLREF	volatile int	reformat_buffer_in_use;	/* used only in DEBUG mode */
GBLREF	unsigned char	*mu_int_locals;
GBLREF	int4		mu_int_ovrhd;
GBLREF	sgmnt_data	mu_int_data;
GBLREF	gd_region	*gv_cur_region;
GBLREF	volatile int4	fast_lock_count;

void mu_int_write(block_id blk, uchar_ptr_t ptr)
{
	file_control	*fc;

	assert(0 == fast_lock_count);
	++fast_lock_count;		/* No interrupts across this use of reformat_buffer */
	/* reformat_buffer_in_use should always be incremented only AFTER incrementing fast_lock_count
	 * as it is the latter that prevents interrupts from using the reformat buffer. Similarly
	 * the decrement of fast_lock_count should be done AFTER decrementing reformat_buffer_in_use.
	 */
	assert(0 == reformat_buffer_in_use);
	DEBUG_ONLY(reformat_buffer_in_use++;)
	if (IS_GDS_BLK_DOWNGRADE_NEEDED(mu_int_data.desired_db_format))
	{
		if (reformat_buffer_len < mu_int_data.blk_size)
		{
			if (reformat_buffer)
				free(reformat_buffer);
			reformat_buffer = malloc(mu_int_data.blk_size);
			reformat_buffer_len = mu_int_data.blk_size;
		}
		gds_blk_downgrade((v15_blk_hdr_ptr_t)reformat_buffer, (blk_hdr_ptr_t)ptr);
		ptr = reformat_buffer;
	}
	fc = gv_cur_region->dyn.addr->file_cntl;
	fc->op = FC_WRITE;
	fc->op_buff = ptr;
	/* Previously, fc->op_len was set to mu_int_data.blk_size. Although only the transaction number in the block header is
	 * going to be reset, we were writing the entire buffer (mu_int_data.blk_size). This demanded a encryption
	 * of the buffer before being written to the disk. To avoid an encryption, we are only going to write the block header
	 * in the desired offset(op_pos). Note that since mu_int_write is called just after an mu_int_read, the block previously
	 * read will be in the OS cache and hence won't cause performance issues due to unaligned writes. When the  database is
	 * not fully upgraded from V4 to V5, we will be writing the entrie block size. This is due to the block upgrades between
	 * V4 and V5 that can happen in the unencrypted versions of the database. */
	fc->op_len = UNIX_ONLY(mu_int_data.fully_upgraded ? SIZEOF(blk_hdr) : ) mu_int_data.blk_size;
	fc->op_pos = mu_int_ovrhd + ((gtm_int64_t)mu_int_data.blk_size / DISK_BLOCK_SIZE * blk);
	dbfilop(fc);
	DEBUG_ONLY(reformat_buffer_in_use--;)
	assert(0 == reformat_buffer_in_use);
	--fast_lock_count;
	assert(0 == fast_lock_count);
	return;
}
