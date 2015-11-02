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

GBLREF sgmnt_data	mu_int_data;
GBLREF int4		mu_int_ovrhd;
GBLREF unsigned char	*mu_int_locals;
GBLREF gd_region	*gv_cur_region;
GTMCRYPT_ONLY(
GBLREF gtmcrypt_key_t	mu_int_encrypt_key_handle;
)

uchar_ptr_t mu_int_read(block_id blk, enum db_ver *ondsk_blkver)
{
	int4		status;
	file_control	*fc;
	unsigned char		*tmp_ptr;
	GTMCRYPT_ONLY(
		int	req_dec_blk_size;
		int	crypt_status;
	)

	error_def(ERR_DBRDERR);
	error_def(ERR_DYNUPGRDFAIL);

	if (!bml_busy(blk,mu_int_locals))
		return 0;
	fc = gv_cur_region->dyn.addr->file_cntl;
	fc->op = FC_READ;
	fc->op_buff = (uchar_ptr_t)malloc(mu_int_data.blk_size);
	fc->op_len = mu_int_data.blk_size;
	fc->op_pos = mu_int_ovrhd + (mu_int_data.blk_size / DISK_BLOCK_SIZE * blk);
	dbfilop(fc); /* No return if error */
	tmp_ptr = fc->op_buff;
#	ifdef GTM_CRYPT
	req_dec_blk_size = (((blk_hdr_ptr_t)tmp_ptr)->bsiz) - (SIZEOF(blk_hdr));
	if (BLOCK_REQUIRE_ENCRYPTION(mu_int_data.is_encrypted, (((blk_hdr_ptr_t)tmp_ptr)->levl), req_dec_blk_size))
	{
		GTMCRYPT_DECODE_FAST(mu_int_encrypt_key_handle,
				    (char *)(tmp_ptr + sizeof(blk_hdr)),
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
