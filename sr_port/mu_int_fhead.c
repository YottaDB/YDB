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
#include "gtmcrypt.h"
#include "db_snapshot.h"

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

#define SET_NATIVE_SIZE(native_size)									\
MBSTART {												\
	GBLREF util_snapshot_ptr_t	util_ss_ptr;							\
	GBLREF boolean_t		ointeg_this_reg;						\
	if (ointeg_this_reg)										\
	{												\
		assert(NULL != util_ss_ptr);								\
		native_size = util_ss_ptr->native_size;							\
		assert(0 != native_size); /* Ensure native_size is updated properly in ss_initiate */	\
	} else												\
		native_size = gds_file_size(gv_cur_region->dyn.addr->file_cntl);			\
} MBEND

boolean_t mu_int_fhead(void)
{
	unsigned char		*p1;
	unsigned int		maps, block_factor;
	gtm_uint64_t		size, native_size, delta_size;
	trans_num		temp_tn, max_tn_warn;
	sgmnt_data_ptr_t	mu_data;
	gd_segment		*seg;
	int			actual_tot_blks, should_be_tot_blks, gtmcrypt_errno;

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
	actual_tot_blks = mu_data->trans_hist.total_blks;
	if (0 == actual_tot_blks)
	{
		mu_int_err(ERR_DBTTLBLK0, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	if (mu_data->trans_hist.curr_tn != mu_data->trans_hist.early_tn)
		mu_int_err(ERR_DBTNNEQ, 0, 0, 0, 0, 0, 0, 0);
        if (0 != mu_data->kill_in_prog)
        {
                gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUKILLIP, 4, DB_LEN_STR(gv_cur_region), LEN_AND_LIT("MUPIP INTEG"));
                mu_int_errknt++;
        }
        if (0 != mu_data->abandoned_kills)
        {
                gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_KILLABANDONED, 4, DB_LEN_STR(gv_cur_region),
			LEN_AND_LIT("database could have incorrectly marked busy integrity errors"));
                mu_int_errknt++;
        }
	if (MAX_KEY_SZ < mu_data->max_key_size)
		mu_int_err(ERR_DBMAXKEYEXC, 0, 0, 0, 0, 0, 0, 0);
	gtmcrypt_errno = 0;
	seg = gv_cur_region->dyn.addr;
	if (IS_ENCRYPTED(mu_data->is_encrypted))
	{
		GTMCRYPT_HASH_CHK(cs_addrs, mu_data->encryption_hash, seg->fname_len, (char *)seg->fname, gtmcrypt_errno);
	}
	if ((0 == gtmcrypt_errno) && USES_NEW_KEY(mu_data))
	{
		GTMCRYPT_HASH_CHK(cs_addrs, mu_data->encryption_hash2, seg->fname_len, (char *)seg->fname, gtmcrypt_errno);
	}
	if (0 != gtmcrypt_errno)
	{
		GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, seg->fname_len, seg->fname);
		return FALSE;
	}
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
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUTNWARN, 4,
						DB_LEN_STR(gv_cur_region), &temp_tn, &mu_data->max_tn);
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
		case dba_bg:
		/*** WARNING: Drop thru ***/
		case dba_mm:
			mu_int_ovrhd = (int4)DIVIDE_ROUND_UP(SIZEOF_FILE_HDR(mu_data) + mu_data->free_space, DISK_BLOCK_SIZE);
	}
	assert(mu_data->blk_size == ROUND_UP(mu_data->blk_size, DISK_BLOCK_SIZE));
 	block_factor =  mu_data->blk_size / DISK_BLOCK_SIZE;
	mu_int_ovrhd += 1;
	if (mu_int_ovrhd != mu_data->start_vbn)
	{
		mu_int_err(ERR_DBHEADINV, 0, 0, 0, 0, 0, 0, 0);
		return FALSE;
	}
	size = (mu_int_ovrhd - 1) + (off_t)block_factor * (actual_tot_blks + 1);
	/* If ONLINE INTEG for this region is in progress, then native_size would have been calculated in ss_initiate. */
	SET_NATIVE_SIZE(native_size);
	ALIGN_DBFILE_SIZE_IF_NEEDED(size, native_size);
	if (native_size && (size != native_size))
	{
		if (size < native_size)
			mu_int_err(ERR_DBFGTBC, 0, 0, 0, 0, 0, 0, 0);
		else
			mu_int_err(ERR_DBFSTBC, 0, 0, 0, 0, 0, 0, 0);
		should_be_tot_blks = (native_size - (mu_data->start_vbn - 1)) / block_factor - 1;
		if ((((should_be_tot_blks + 1) * block_factor) + (mu_data->start_vbn - 1)) == native_size)
		{	/* The database file size is off but is off by N GDS blocks where N is an integer */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBTOTBLK, 2, actual_tot_blks, should_be_tot_blks);
		} else
		{	/* The database file size is off but is off by a fractional GDS block. Issue different error (alignment) */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_DBMISALIGN, 4, actual_tot_blks,
				should_be_tot_blks, should_be_tot_blks + 1,
				((should_be_tot_blks + 1 - actual_tot_blks) <= 0) ? 1 : (should_be_tot_blks + 1 - actual_tot_blks));
		}
	}
	/* make working space for all local bitmaps */
	maps = (actual_tot_blks + mu_data->bplmap - 1) / mu_data->bplmap;
	size = (gtm_uint64_t)(BM_SIZE(mu_data->bplmap) - SIZEOF(blk_hdr));
	size *= maps;
	mu_int_locals = (unsigned char *)malloc(size);
	memset(mu_int_locals, FOUR_BLKS_FREE, size);
	return TRUE;
}
