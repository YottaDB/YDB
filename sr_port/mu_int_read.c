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
#include "filestruct.h"
#include "gdsbml.h"
#include "dbfilop.h"
#include "gdsdbver.h"
#include "gdsblk.h"
#include "iosp.h"
#include "mupint.h"
#include "gds_blk_upgrade.h"
#include "gtmcrypt.h"
#include "min_max.h"
#include "shmpool.h"		/* Needed for DBG_ENSURE_PTR_WITHIN_SS_BOUNDS */
#include "db_snapshot.h"
#include "mupip_exit.h"

GBLREF sgmnt_data		mu_int_data;
GBLREF int4			mu_int_ovrhd;
GBLREF gd_region		*gv_cur_region;
GBLREF enc_handles		mu_int_encr_handles;
GBLREF boolean_t		ointeg_this_reg;
GBLREF sgmnt_addrs		*cs_addrs;
GBLREF sgmnt_data_ptr_t		cs_data;
GBLREF uint4			mu_int_errknt;
GBLREF bool			region;

error_def(ERR_DBRDERR);
error_def(ERR_DBROLLEDBACK);
error_def(ERR_DYNUPGRDFAIL);
error_def(ERR_INTEGERRS);
error_def(ERR_REGSSFAIL);

/* Returns buffer containing the GDS block. "free_buff" is set to point to the start of the malloced buffer
 * so the caller needs to use "free_buff" when doing the "free". This is necessary particularly in case of
 * asyncio=TRUE when the file is opened with O_DIRECT and the malloced buffer is not necessarily aligned
 * for DIO. And so we allocate with padding to get alignment. In this case the aligned buffer is returned
 * as the GDS block but the unaligned buffer is set in "free_buff" and is the one that needs to be freed.
 * In case asyncio=FALSE, the return value is the same as "free_buff". So in all cases, the caller is safe to
 * do a free of "free_buff".
 */
uchar_ptr_t mu_int_read(block_id blk, enum db_ver *ondsk_blkver, uchar_ptr_t *free_buff)
{
	int4			status;
	file_control		*fc;
	unsigned char		*tmp_ptr;
	int			in_len, gtmcrypt_errno;
	char			*in;
	gd_segment		*seg;
	boolean_t		db_is_encrypted, use_new_key;
	sgmnt_data_ptr_t	csd;
	boolean_t 		have_blk;
	sgmnt_addrs		*csa;
	unix_db_info		*udi;
	boolean_t		read_failed;
	shm_snapshot_t		*ss_shm_ptr;

	have_blk = FALSE;
	csa = cs_addrs;
	if (region && csa->nl->onln_rlbk_pid)
	{
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(1) ERR_DBROLLEDBACK);
		mupip_exit(ERR_INTEGERRS);
	}
	fc = gv_cur_region->dyn.addr->file_cntl;
	udi = FC2UDI(fc);
	if (udi->fd_opened_with_o_direct)
	{	/* We need aligned buffers */
		tmp_ptr = (unsigned char *)malloc(ROUND_UP2(mu_int_data.blk_size, DIO_ALIGNSIZE(udi)) + OS_PAGE_SIZE);
		*free_buff = tmp_ptr;
		tmp_ptr = (unsigned char *)ROUND_UP2((UINTPTR_T)tmp_ptr, OS_PAGE_SIZE);
	} else
	{
		tmp_ptr = malloc(mu_int_data.blk_size);
		*free_buff = tmp_ptr;
	}
	if (ointeg_this_reg)
	{
		assert(NULL != csa->ss_ctx);
		ss_shm_ptr = (SS_CTX_CAST(csa->ss_ctx))->ss_shm_ptr;
		DBG_ENSURE_PTR_WITHIN_SS_BOUNDS(csa, (sm_uc_ptr_t)ss_shm_ptr);
		if (!ss_shm_ptr->failure_errno)
			have_blk = ss_get_block(csa, blk, tmp_ptr);
		else
		{	/* GT.M has failed initiating snapshot resources. Report error and exit so that we don't continue with
			 * an INTEG which is unusable. Note that, it's okay to check failure_errno (in the shared memory) outside
			 * crit as GT.M is the only one that sets it and INTEG (while doing snapshot cleanup) is the only one
			 * that resets it
			 */
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_REGSSFAIL, 3, ss_shm_ptr->failed_pid,
						DB_LEN_STR(gv_cur_region));
			mupip_exit(ERR_INTEGERRS);
		}
	}
	if (!have_blk)
	{
		fc->op = FC_READ;
		fc->op_buff = tmp_ptr;
		fc->op_len = mu_int_data.blk_size;
		fc->op_pos = mu_int_ovrhd + ((gtm_int64_t)mu_int_data.blk_size / DISK_BLOCK_SIZE * blk);
		dbfilop(fc); /* No return if error */
		if (ointeg_this_reg)
		{
			assert(NULL != ss_shm_ptr); /* should have been initialized above */
			/* Since we did not have locks up, ensure we didn't miss a before-image	while we were reading
			 * in this block. Note we don't have to check the return status since it will either get the
			 * before-image or not.
			 */
			if (!ss_shm_ptr->failure_errno)
				ss_get_block(csa, blk, tmp_ptr);
			else
			{
				gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_REGSSFAIL, 3, ss_shm_ptr->failed_pid,
							DB_LEN_STR(gv_cur_region));
				mupip_exit(ERR_INTEGERRS);
			}
		}
	}
	csd = &mu_int_data;
	if (USES_ENCRYPTION(csd->is_encrypted))
	{
		in_len = MIN(mu_int_data.blk_size, ((blk_hdr_ptr_t)tmp_ptr)->bsiz) - SIZEOF(blk_hdr);
		db_is_encrypted = IS_ENCRYPTED(csd->is_encrypted);
		use_new_key = NEEDS_NEW_KEY(csd, ((blk_hdr_ptr_t)tmp_ptr)->tn);
		if ((use_new_key || db_is_encrypted) && IS_BLK_ENCRYPTED((((blk_hdr_ptr_t)tmp_ptr)->levl), in_len))
		{	/* The below assert cannot be moved before (use_new_key || db_is_encrypted) check done above as tmp_ptr
			 * could potentially point to a V4 block in which case the assert might fail when a V4 block is cast to
			 * a V5 block header.
			 */
			assert(((blk_hdr_ptr_t)tmp_ptr)->bsiz <= csd->blk_size);
			assert(((blk_hdr_ptr_t)tmp_ptr)->bsiz >= SIZEOF(blk_hdr));
			in = (char *)(tmp_ptr + SIZEOF(blk_hdr));
			GTMCRYPT_DECRYPT(csa, (use_new_key ? TRUE : csd->non_null_iv),
					(use_new_key ? mu_int_encr_handles.encr_key_handle2 : mu_int_encr_handles.encr_key_handle),
					in, in_len, NULL, tmp_ptr, SIZEOF(blk_hdr), gtmcrypt_errno);
			if (0 != gtmcrypt_errno)
			{
				seg = gv_cur_region->dyn.addr;
				GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, seg->fname_len, seg->fname);
			}
		}
	}
	GDS_BLK_UPGRADE_IF_NEEDED(blk, tmp_ptr, tmp_ptr, &mu_int_data, ondsk_blkver, status, mu_int_data.fully_upgraded);
	if (SS_NORMAL != status)
		if (ERR_DYNUPGRDFAIL == status)
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(5) status, 3, blk, DB_LEN_STR(gv_cur_region));
		else
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(1) status);
	return tmp_ptr;
}
