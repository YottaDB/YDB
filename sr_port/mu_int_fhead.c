/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "mupint.h"
#include "gtmmsg.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#ifdef GTM_SNAPSHOT
#include "db_snapshot.h"
#endif

GBLDEF	unsigned char		*mu_int_locals;
GBLDEF	int4			mu_int_ovrhd;

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_data		mu_int_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	uint4			mu_int_errknt;
GBLREF	boolean_t		tn_reset_specified;

error_def(ERR_DBNOTDB);
error_def(ERR_DBINCRVER);
error_def(ERR_DBSVBNMIN);
error_def(ERR_DBFLCORRP);
error_def(ERR_DBCREINCOMP);
error_def(ERR_DBBSIZZRO);
error_def(ERR_DBSZGT64K);
error_def(ERR_DBNOTMLTP);
error_def(ERR_DBBPLMLT512);
error_def(ERR_DBBPLMGT2K);
error_def(ERR_DBBPLNOT512);
error_def(ERR_DBTTLBLK0);
error_def(ERR_DBTNNEQ);
error_def(ERR_DBMAXKEYEXC);
error_def(ERR_DBMXRSEXCMIN);
error_def(ERR_DBUNDACCMT);
error_def(ERR_DBHEADINV);
error_def(ERR_DBFGTBC);
error_def(ERR_DBFSTBC);
error_def(ERR_DBTOTBLK);
error_def(ERR_DBMISALIGN);
error_def(ERR_KILLABANDONED);
error_def(ERR_MUKILLIP);
error_def(ERR_MUTNWARN);

#ifdef GTM_SNAPSHOT
# define SET_NATIVE_SIZE(native_size)									\
{													\
	GBLREF util_snapshot_ptr_t	util_ss_ptr;							\
	GBLREF boolean_t		ointeg_this_reg;						\
	if (ointeg_this_reg)										\
	{												\
		assert(NULL != util_ss_ptr);								\
		native_size = util_ss_ptr->native_size;							\
		assert(0 != native_size); /* Ensure native_size is updated properly in ss_initiate */	\
	} else												\
		native_size = gds_file_size(gv_cur_region->dyn.addr->file_cntl);			\
}
#else
# define SET_NATIVE_SIZE(native_size)	native_size = gds_file_size(gv_cur_region->dyn.addr->file_cntl);
#endif
boolean_t mu_int_fhead(void)
{
	unsigned char		*p1;
	unsigned int		maps, block_factor;
	gtm_uint64_t		size, native_size;
	trans_num		temp_tn, max_tn_warn;
	sgmnt_data_ptr_t	mu_data;
#	ifdef GTM_CRYPT
	gd_segment		*seg;
	int			gtmcrypt_errno;
#	endif

	mu_data = &mu_int_data;
	if (MEMCMP_LIT(mu_data->label, GDS_LABEL))
	{
		if (memcmp(mu_data->label, GDS_LABEL, SIZEOF(GDS_LABEL) - 2))
			mu_int_err(ERR_DBNOTDB, 0, 0, 0, 0, 0, 0, 0);
		else
			mu_int_err(ERR_DBINCRVER, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	UNIX_ONLY(CHECK_DB_ENDIAN(mu_data, gv_cur_region->dyn.addr->fname_len, gv_cur_region->dyn.addr->fname)); /* BYPASSOK */
	if (mu_data->start_vbn < DIVIDE_ROUND_UP(SIZEOF_FILE_HDR(mu_data), DISK_BLOCK_SIZE))
	{
		mu_int_err(ERR_DBSVBNMIN, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	if (mu_data->file_corrupt)
		mu_int_err(ERR_DBFLCORRP, 0, 0, 0, 0, 0, 0, 0);
	if (mu_data->createinprogress)
	{
		mu_int_err(ERR_DBCREINCOMP, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	/* CHECK: 0 < blk_size <= 64K; blk_size is a multiple of DISK_BLOCK_SIZE */
	if (0 == mu_data->blk_size)
	{
		mu_int_err(ERR_DBBSIZZRO, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	if (mu_data->blk_size > 1 << 16)
	{
		mu_int_err(ERR_DBSZGT64K, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	if (mu_data->blk_size % DISK_BLOCK_SIZE)
	{	/* these messages should use rts_error and parameters */
		assert(512 == DISK_BLOCK_SIZE);		/* but in lieu of that, check that message is accurate */
		mu_int_err(ERR_DBNOTMLTP, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	/* CHECK: BLKS_PER_LMAP <= bplmap <= 2K; bplmap is a multiple of BLKS_PER_LMAP */
	if (mu_data->bplmap < BLKS_PER_LMAP)
	{
		mu_int_err(ERR_DBBPLMLT512, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	if (mu_data->bplmap > 1 << 11)
	{
		mu_int_err(ERR_DBBPLMGT2K, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	if (BLKS_PER_LMAP != mu_data->bplmap)
	{
		mu_int_err(ERR_DBBPLNOT512, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	/* CHECK: total_blks <> 0 */
	if (0 == mu_data->trans_hist.total_blks)
	{
		mu_int_err(ERR_DBTTLBLK0, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	if (mu_data->trans_hist.curr_tn != mu_data->trans_hist.early_tn)
		mu_int_err(ERR_DBTNNEQ, 0, 0, 0, 0, 0, 0, 0);
        if (0 != mu_data->kill_in_prog)
        {
                gtm_putmsg(VARLSTCNT(6) ERR_MUKILLIP, 4, DB_LEN_STR(gv_cur_region), LEN_AND_LIT("MUPIP INTEG"));
                mu_int_errknt++;
        }
        if (0 != mu_data->abandoned_kills)
        {
                gtm_putmsg(VARLSTCNT(6) ERR_KILLABANDONED, 4, DB_LEN_STR(gv_cur_region),
			LEN_AND_LIT("database could have incorrectly marked busy integrity errors"));
                mu_int_errknt++;
        }
	if (MAX_KEY_SZ < mu_data->max_key_size)
		mu_int_err(ERR_DBMAXKEYEXC, 0, 0, 0, 0, 0, 0, 0);
#	ifdef GTM_CRYPT
	if (mu_data->is_encrypted)
	{
		GTMCRYPT_HASH_CHK(cs_addrs, mu_data->encryption_hash, gtmcrypt_errno);
		if (0 != gtmcrypt_errno)
		{
			seg = gv_cur_region->dyn.addr;
			GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, seg->fname_len, seg->fname);
			return FALSE;
		}
	}
#	endif
	/* !tn_reset_this_reg should ideally be used here instead of (!tn_reset_specified || gv_cur_region->read_only).
	 * But at this point, tn_reset_this_reg has not yet been set for this region and to avoid taking a risk in
	 *   changing the code flow, we redo the computation ot tn_reset_this_reg here. This is not as much a performance concern.
	 */
	if (!tn_reset_specified || gv_cur_region->read_only)
	{
		SET_TN_WARN(mu_data, max_tn_warn);
		if (max_tn_warn == mu_data->trans_hist.curr_tn)
		{	/* implies there is not enough transactions to go before reaching MAX_TN (see SET_TN_WARN macro) */
			temp_tn = mu_data->max_tn - mu_data->trans_hist.curr_tn;
			gtm_putmsg(VARLSTCNT(6) ERR_MUTNWARN, 4, DB_LEN_STR(gv_cur_region), &temp_tn, &mu_data->max_tn);
			mu_int_errknt++;
		}
	}
	/* Note - ovrhd is incremented once in order to achieve a zero-based
	 * index of the GDS 'data' blocks (those other than the file header
	 * and the block table). */
	switch (mu_data->acc_meth)
	{
		default:
			mu_int_err(ERR_DBUNDACCMT, 0, 0, 0, 0, 0, 0, 0);
		/*** WARNING: Drop thru ***/
#		ifdef VMS
#		 ifdef GT_CX_DEF
		case dba_bg:	/* necessary to do calculation in this manner to prevent double rounding causing an error */
			if (mu_data->unbacked_cache)
				mu_int_ovrhd = DIVIDE_ROUND_UP(SIZEOF_FILE_HDR(mu_data) + mu_data->free_space +
					mu_data->lock_space_size, DISK_BLOCK_SIZE);
			else
				mu_int_ovrhd = DIVIDE_ROUND_UP(SIZEOF_FILE_HDR(mu_data) + BT_SIZE(mu_data)
					+ mu_data->free_space + mu_data->lock_space_size, DISK_BLOCK_SIZE);
			break;
		case dba_mm:
			mu_int_ovrhd = DIVIDE_ROUND_UP(SIZEOF_FILE_HDR(mu_data) + mu_data->free_space, DISK_BLOCK_SIZE);
			break;
#		 else
		case dba_bg:
		/*** WARNING: Drop thru ***/
		case dba_mm:
			mu_int_ovrhd = DIVIDE_ROUND_UP(SIZEOF_FILE_HDR(mu_data) + mu_data->free_space, DISK_BLOCK_SIZE);
		break;
#		 endif

#		elif defined(UNIX)
		case dba_bg:
		/*** WARNING: Drop thru ***/
		case dba_mm:
			mu_int_ovrhd = (int4)DIVIDE_ROUND_UP(SIZEOF_FILE_HDR(mu_data) + mu_data->free_space, DISK_BLOCK_SIZE);
#		else
#		 error unsupported platform
#		endif
	}
	assert(mu_data->blk_size == ROUND_UP(mu_data->blk_size, DISK_BLOCK_SIZE));
 	block_factor =  mu_data->blk_size / DISK_BLOCK_SIZE;
	mu_int_ovrhd += 1;
	if (mu_int_ovrhd != mu_data->start_vbn)
	{
		mu_int_err(ERR_DBHEADINV, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	size = mu_int_ovrhd + (off_t)block_factor * mu_data->trans_hist.total_blks;
	/* If ONLINE INTEG for this region is in progress, then native_size would have been calculated in ss_initiate. */
	SET_NATIVE_SIZE(native_size);
	/* In the following tests, the EOF block should always be 1 greater
	 * than the actual size of the file.  This is due to the GDS being
	 * allocated in even DISK_BLOCK_SIZE-byte blocks. */
	if (native_size && (size != native_size))
	{
		if (size < native_size)
			mu_int_err(ERR_DBFGTBC, 0, 0, 0, 0, 0, 0, 0);
		else
			mu_int_err(ERR_DBFSTBC, 0, 0, 0, 0, 0, 0, 0);
		if (native_size % 2) /* Native size should be (64K + n*1K + 512) / DISK_BLOCK_SIZE , so always an odd number. */
			gtm_putmsg(VARLSTCNT(4) ERR_DBTOTBLK, 2, (uint4)((native_size - mu_data->start_vbn) / block_factor),
				mu_data->trans_hist.total_blks);
		else
			/* Since native_size is even and the result will be rounded down, we need to add 1 before the division so we
			 * extend by enough blocks (ie. if current nb. of blocks is 100, and the file size gives 102.5 blocks, we
			 * need to extend by 3 blocks, not 2). */
			gtm_putmsg(VARLSTCNT(6) ERR_DBMISALIGN, 4, DB_LEN_STR(gv_cur_region),
				(uint4)((native_size - mu_data->start_vbn) / block_factor),
				(uint4)(((native_size + 1 - mu_data->start_vbn) / block_factor) - mu_data->trans_hist.total_blks));
	}
	/* make working space for all local bitmaps */
	maps = (mu_data->trans_hist.total_blks + mu_data->bplmap - 1) / mu_data->bplmap;
	size = (gtm_uint64_t)(BM_SIZE(mu_data->bplmap) - SIZEOF(blk_hdr));
	size *= maps;
	mu_int_locals = (unsigned char *)malloc(size);
	memset(mu_int_locals, FOUR_BLKS_FREE, size);
	return TRUE;
}
