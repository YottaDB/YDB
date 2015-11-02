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
#include "shmpool.h"		/* Needed for DBG_ENSURE_PTR_WITHIN_SS_BOUNDS */
#include "db_snapshot.h"
#endif
#include "mupip_exit.h"

GBLREF sgmnt_data	mu_int_data;
GBLREF int4		mu_int_ovrhd;
GBLREF gd_region	*gv_cur_region;
GTMCRYPT_ONLY(
GBLREF gtmcrypt_key_t	mu_int_encrypt_key_handle;
)
GBLREF	boolean_t	ointeg_this_reg;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	uint4		mu_int_errknt;
GBLREF	bool		region;

error_def(ERR_DBRDERR);
error_def(ERR_DBROLLEDBACK);
error_def(ERR_DYNUPGRDFAIL);
error_def(ERR_INTEGERRS);
error_def(ERR_REGSSFAIL);

uchar_ptr_t mu_int_read(block_id blk, enum db_ver *ondsk_blkver)
{
	int4			status;
	file_control		*fc;
	unsigned char		*tmp_ptr;
#	ifdef GTM_CRYPT
	int			in_len, gtmcrypt_errno;
	char			*in;
	gd_segment		*seg;
#	endif
	boolean_t 		have_blk = FALSE;
	sgmnt_addrs		*csa;
	GTM_SNAPSHOT_ONLY(
		boolean_t	read_failed;
		shm_snapshot_t	*ss_shm_ptr;
	)

	csa = cs_addrs;
#	ifdef UNIX
	if (region && csa->nl->onln_rlbk_pid)
	{
		gtm_putmsg(VARLSTCNT(1) ERR_DBROLLEDBACK);
		mupip_exit(ERR_INTEGERRS);
	}
#	endif
	tmp_ptr = malloc(mu_int_data.blk_size);
#	ifdef GTM_SNAPSHOT
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
			gtm_putmsg(VARLSTCNT(5) ERR_REGSSFAIL, 3, ss_shm_ptr->failed_pid, DB_LEN_STR(gv_cur_region));
			mupip_exit(ERR_INTEGERRS);
		}
	}
#	endif
	if (!have_blk)
	{
		fc = gv_cur_region->dyn.addr->file_cntl;
		fc->op = FC_READ;
		fc->op_buff = tmp_ptr;
		fc->op_len = mu_int_data.blk_size;
		fc->op_pos = mu_int_ovrhd + ((gtm_int64_t)mu_int_data.blk_size / DISK_BLOCK_SIZE * blk);
		dbfilop(fc); /* No return if error */
#		ifdef GTM_SNAPSHOT
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
				gtm_putmsg(VARLSTCNT(5) ERR_REGSSFAIL, 3, ss_shm_ptr->failed_pid, DB_LEN_STR(gv_cur_region));
				mupip_exit(ERR_INTEGERRS);
			}
		}
#		endif
	}
#	ifdef GTM_CRYPT
	in_len = MIN(mu_int_data.blk_size, ((blk_hdr_ptr_t)tmp_ptr)->bsiz) - SIZEOF(blk_hdr);
	if (BLOCK_REQUIRE_ENCRYPTION(mu_int_data.is_encrypted, (((blk_hdr_ptr_t)tmp_ptr)->levl), in_len))
	{
		/* The below assert cannot be moved before BLOCK_REQUIRE_ENCRYPTION check done above as tmp_ptr could
		 * potentially point to a V4 block in which case the assert might fail when a V4 block is casted to
		 * a V5 block header.
		 */
		assert(((blk_hdr_ptr_t)tmp_ptr)->bsiz <= mu_int_data.blk_size);
		assert(((blk_hdr_ptr_t)tmp_ptr)->bsiz >= SIZEOF(blk_hdr));
		in = (char *)(tmp_ptr + SIZEOF(blk_hdr));
		GTMCRYPT_DECRYPT(csa, mu_int_encrypt_key_handle, in, in_len, NULL, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			seg = gv_cur_region->dyn.addr;
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, seg->fname_len, seg->fname);
		}
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
