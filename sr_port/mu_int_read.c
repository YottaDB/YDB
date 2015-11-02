/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "filestruct.h"
#include "gdsbml.h"
#include "dbfilop.h"
#include "gdsdbver.h"
#include "gdsblk.h"
#include "iosp.h"
#include "mupint.h"
#include "gds_blk_upgrade.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#include "min_max.h"
#ifdef GTM_SNAPSHOT
#include "db_snapshot.h"
#endif

GBLREF sgmnt_data	mu_int_data;
GBLREF int4		mu_int_ovrhd;
GBLREF gd_region	*gv_cur_region;
GTMCRYPT_ONLY(
GBLREF gtmcrypt_key_t	mu_int_encrypt_key_handle;
)
GBLREF	boolean_t	ointeg_this_reg;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	uint4		mu_int_errknt;

uchar_ptr_t mu_int_read(block_id blk, enum db_ver *ondsk_blkver)
{
	int4		status;
	file_control	*fc;
	unsigned char	*tmp_ptr;
	GTMCRYPT_ONLY(
		int	req_dec_blk_size;
		int	crypt_status;
	)
	boolean_t 	have_blk = FALSE;
	sgmnt_addrs	*csa;
	GTM_SNAPSHOT_ONLY(boolean_t	read_failed;)

	error_def(ERR_DBRDERR);
	error_def(ERR_DYNUPGRDFAIL);

	csa = cs_addrs;
	tmp_ptr = malloc(mu_int_data.blk_size);
#	ifdef GTM_SNAPSHOT
	if (ointeg_this_reg)
	{
		if (ss_get_block(csa, csa->ss_ctx, blk, tmp_ptr, &read_failed))
			have_blk = TRUE;
		else if (read_failed)
		{
			/* ss_get_block error'ed out for some reason. */
			assert(FALSE);
			mu_int_errknt++;
			return NULL;
		}
	}
#	endif
	if (!have_blk)
	{
		fc = gv_cur_region->dyn.addr->file_cntl;
		fc->op = FC_READ;
		fc->op_buff = tmp_ptr;
		fc->op_len = mu_int_data.blk_size;
		fc->op_pos = mu_int_ovrhd + (mu_int_data.blk_size / DISK_BLOCK_SIZE * blk);
		dbfilop(fc); /* No return if error */
#		ifdef GTM_SNAPSHOT
		if (ointeg_this_reg)
		{
			/* Since we did not have locks up, ensure we didn't miss a before-image	while we where reading 	*/
			/* in this block. Note we don't have to check the return status since it will either get the 	*/
			/* before-image or not.										*/
			ss_get_block(csa, csa->ss_ctx, blk, tmp_ptr, &read_failed);
			if (read_failed)
			{
				/* ss_get_block error'ed out for some reason. */
				assert(FALSE);
				mu_int_errknt++;
				return NULL;
			}
		}
#		endif
	}
	GTM_SNAPSHOT_ONLY(
		if (ointeg_this_reg)
			(SS_CTX_CAST(csa->ss_ctx))->ss_shm_ptr->ss_read_progress++;
	)
#	ifdef GTM_CRYPT
	req_dec_blk_size = MIN(mu_int_data.blk_size, ((blk_hdr_ptr_t)tmp_ptr)->bsiz) - SIZEOF(blk_hdr);
	if (BLOCK_REQUIRE_ENCRYPTION(mu_int_data.is_encrypted, (((blk_hdr_ptr_t)tmp_ptr)->levl), req_dec_blk_size))
	{
		/* The below assert cannot be moved before BLOCK_REQUIRE_ENCRYPTION check done above as tmp_ptr could
		 * potentially point to a V4 block in which case the assert might fail when a V4 block is casted to
		 * a V5 block header.
		 */
		assert(((blk_hdr_ptr_t)tmp_ptr)->bsiz <= mu_int_data.blk_size);
		assert(((blk_hdr_ptr_t)tmp_ptr)->bsiz >= SIZEOF(blk_hdr));
		GTMCRYPT_DECODE_FAST(mu_int_encrypt_key_handle,
				    (char *)(tmp_ptr + SIZEOF(blk_hdr)),
				    req_dec_blk_size,
				    NULL,
				    crypt_status);
		if (0 != crypt_status)
			GC_RTS_ERROR(crypt_status, gv_cur_region->dyn.addr->fname);
	}
#	endif
	GDS_BLK_UPGRADE_IF_NEEDED(blk, tmp_ptr, tmp_ptr, &mu_int_data, ondsk_blkver, status, mu_int_data.fully_upgraded);
	if (SS_NORMAL != status)
		if (ERR_DYNUPGRDFAIL == status)
			rts_error(VARLSTCNT(5) status, 3, blk, DB_LEN_STR(gv_cur_region));
		else
			rts_error(VARLSTCNT(1) status);
	return tmp_ptr;
}
