/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtmcrypt.h"
#include "min_max.h"
#include "mupint.h"
#include "filestruct.h"

GBLREF	sm_uc_ptr_t	reformat_buffer;
GBLREF	int		reformat_buffer_len;
GBLREF	volatile int	reformat_buffer_in_use;	/* used only in DEBUG mode */
GBLREF	unsigned char	*mu_int_locals;
GBLREF	int4		mu_int_ovrhd;
GBLREF	sgmnt_data	mu_int_data;
GBLREF	gd_region	*gv_cur_region;
GBLREF	volatile int4	fast_lock_count;
GBLREF	enc_handles	mu_int_encr_handles;
GBLREF	mstr		pvt_crypt_buf;

void mu_int_write(block_id blk, uchar_ptr_t ptr)
{
	int		in_len, orig_write_len, write_len, gtmcrypt_errno;
	char		*in, *out;
	sm_uc_ptr_t	write_buff;
	gd_segment	*seg;
	file_control	*fc;
	unix_db_info	*udi;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
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
	/* In case the block whose header we touched is encrypted, we need to reencrypt its entire content (unless using null IV),
	 * since we have practically altered the IV and so the ciphertext would be different as well. If, however, the block is not
	 * encrypted, we can get away with writing only the header portion. Note that since mu_int_write is called just after
	 * mu_int_read, the block previously read will be in the OS cache and hence will not cause performance issues due to
	 * unaligned writes. When the database is not fully upgraded from V4 to V5, we will be writing the entire block size. This
	 * is due to the block upgrades between V4 and V5 that can happen in the unencrypted versions of the database. Also,
	 * we will write full GDS blocks in case asyncio=ON (i.e. O_DIRECT open) to ensure filesystem-block-size aligned writes.
	 */
	in_len = MIN(mu_int_data.blk_size, ((blk_hdr_ptr_t)ptr)->bsiz) - SIZEOF(blk_hdr);
	/* We disallow the use of TN_RESET when database (re)encryption is in progress. */
	assert(!USES_NEW_KEY(&mu_int_data));
	if (BLK_NEEDS_ENCRYPTION3(IS_ENCRYPTED(mu_int_data.is_encrypted), (((blk_hdr_ptr_t)ptr)->levl), in_len)
			&& mu_int_data.non_null_iv)
	{	/* The below assert cannot be moved before BLK_NEEDS_ENCRYPTION3 check done above as ptr could potentially point to
		 * a V4 block in which case the assert might fail when a V4 block is casted to a V5 block header.
		 */
		assert(((blk_hdr_ptr_t)ptr)->bsiz <= mu_int_data.blk_size);
		assert(((blk_hdr_ptr_t)ptr)->bsiz >= SIZEOF(blk_hdr));
		REALLOC_CRYPTBUF_IF_NEEDED(mu_int_data.blk_size);
		memcpy(pvt_crypt_buf.addr, ptr, SIZEOF(blk_hdr));
		in = (char *)(ptr + SIZEOF(blk_hdr));
		out = (char *)pvt_crypt_buf.addr + SIZEOF(blk_hdr);
		GTMCRYPT_ENCRYPT(csa, mu_int_data.non_null_iv, mu_int_encr_handles.encr_key_handle, in, in_len, out,
				ptr, SIZEOF(blk_hdr), gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			seg = gv_cur_region->dyn.addr;
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, seg->fname_len, seg->fname);
		}
		write_len = in_len + SIZEOF(blk_hdr);
		write_buff = (unsigned char *)pvt_crypt_buf.addr;
	} else
	{
		write_len = mu_int_data.fully_upgraded ? SIZEOF(blk_hdr) : mu_int_data.blk_size;
		write_buff = ptr;
	}
	fc->op_pos = mu_int_ovrhd + ((gtm_int64_t)mu_int_data.blk_size / DISK_BLOCK_SIZE * blk);
	/* Do aligned writes if opened with O_DIRECT */
	udi = FC2UDI(fc);
	if (udi->fd_opened_with_o_direct)
	{	/* We have to write the full GDS block. Get write length including block-header & content. Note "write_len"
		 * could contain just the block-header length in one case above and so do not use it. Recompute it.
		 */
		orig_write_len = in_len + SIZEOF(blk_hdr);
		write_len = ROUND_UP2(orig_write_len, DIO_ALIGNSIZE(udi));
		assert(write_len >= orig_write_len);
		DIO_BUFF_EXPAND_IF_NEEDED(udi, write_len, &(TREF(dio_buff)));
		memcpy((TREF(dio_buff)).aligned, write_buff, orig_write_len);
		memset((TREF(dio_buff)).aligned + orig_write_len, 0, write_len - orig_write_len);
		write_buff = (sm_uc_ptr_t)(TREF(dio_buff)).aligned;
	}
	fc->op_len = write_len;
	fc->op_buff = write_buff;
	dbfilop(fc);
	DEBUG_ONLY(reformat_buffer_in_use--;)
	assert(0 == reformat_buffer_in_use);
	--fast_lock_count;
	assert(0 == fast_lock_count);
	return;
}
