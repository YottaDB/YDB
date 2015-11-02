/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
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
	fc->op_len = mu_int_data.blk_size;
	fc->op_pos = mu_int_ovrhd + (mu_int_data.blk_size / DISK_BLOCK_SIZE * blk);
	dbfilop(fc);
	DEBUG_ONLY(reformat_buffer_in_use--;)
	assert(0 == reformat_buffer_in_use);
	--fast_lock_count;
	assert(0 == fast_lock_count);
	return;
}
