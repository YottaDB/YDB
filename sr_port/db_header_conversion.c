/****************************************************************
 *								*
 * Copyright (c) 2020-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
/* These functions will translate a v6 header to v7 and back again */

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "v6_gdsfhead.h"
#include "db_header_conversion.h"

/* Convert the header from v6 to v7 format */
void db_header_upconv(sgmnt_data_ptr_t v7)
{
	v6_sgmnt_data_ptr_t	v6 = (v6_sgmnt_data_ptr_t)v7;

	assert(sizeof(v6_sgmnt_data) == sizeof(sgmnt_data));
	v7->master_map_len = v6->master_map_len;
	v7->start_vbn = v6->start_vbn;
	v7->last_inc_bkup_last_blk = v6->last_inc_bkup_last_blk;
	v7->last_com_bkup_last_blk = v6->last_com_bkup_last_blk;
	v7->last_rec_bkup_last_blk = v6->last_rec_bkup_last_blk;
	v7->reorg_restart_block = v6->reorg_restart_block;
	v7->reorg_upgrd_dwngrd_restart_block = v6->reorg_upgrd_dwngrd_restart_block;
	v7->blks_to_upgrd = (ublock_id_32)v6->blks_to_upgrd;
	v7->blks_to_upgrd_subzero_error = v6->blks_to_upgrd_subzero_error;
	v7->before_trunc_total_blks = v6->before_trunc_total_blks;
	v7->after_trunc_total_blks = v6->after_trunc_total_blks;
	v7->before_trunc_free_blocks = v6->before_trunc_free_blocks;
	v7->encryption_hash_cutoff = v6->encryption_hash_cutoff;
	v7->trans_hist.curr_tn = v6->trans_hist.curr_tn;
	v7->trans_hist.early_tn = v6->trans_hist.early_tn;
	v7->trans_hist.last_mm_sync = v6->trans_hist.last_mm_sync;
	v7->trans_hist.mm_tn = v6->trans_hist.mm_tn;
	v7->trans_hist.lock_sequence = v6->trans_hist.lock_sequence;
	v7->trans_hist.ccp_jnl_filesize = v6->trans_hist.ccp_jnl_filesize;
	v7->trans_hist.total_blks = v6->trans_hist.total_blks;
	v7->trans_hist.free_blocks = v6->trans_hist.free_blocks;
	v7->last_start_backup = v6->last_start_backup;
	v7->problksplit = v6->problksplit;
}

#ifdef DEBUG
/* Ensure that the values stay constant when performing a downconversion */
#define assert_conversion(INPUT, TEMP)					\
MBSTART {								\
	TEMP = INPUT;							\
	assert(TEMP == (TEMP & 0x7fffffff));				\
} MBEND;
/* Ensure that the values stay constant when performing a downconversion */
#define assert_conversion_cast(INPUT, TEMP, CAST)			\
MBSTART {								\
	TEMP = INPUT;							\
	assert(TEMP == (CAST)TEMP);					\
} MBEND;
#else
#define assert_conversion(INPUT, TEMP)
#define assert_conversion_cast(INPUT, TEMP, CAST)
#endif

/* Convert the header from v7 to v6 format */
void db_header_dwnconv(sgmnt_data_ptr_t v7)
{
	v6_sgmnt_data_ptr_t	v6 = (v6_sgmnt_data_ptr_t)v7;
#ifdef DEBUG
	block_id		dbg_blkid;
	gtm_int8		dbg_int8;
#endif

	assert(sizeof(v6_sgmnt_data) == sizeof(sgmnt_data));
	assert_conversion(v7->master_map_len, dbg_int8);
	v6->master_map_len = v7->master_map_len;
	assert_conversion(v7->start_vbn, dbg_blkid);
	v6->start_vbn = v7->start_vbn;
	/* The following variables are modified by MUPIP BACKIP COMPREHENSIVE/INCREMENTAL without crit */
	assert_conversion(v7->last_inc_bkup_last_blk, dbg_blkid);
	v6->last_inc_bkup_last_blk = v7->last_inc_bkup_last_blk;
	assert_conversion(v7->last_com_bkup_last_blk, dbg_blkid);
	v6->last_com_bkup_last_blk = v7->last_com_bkup_last_blk;
	assert_conversion(v7->last_rec_bkup_last_blk, dbg_blkid);
	v6->last_rec_bkup_last_blk = v7->last_rec_bkup_last_blk;
	/* The following is modified by REORG without crit */
	assert_conversion(v7->reorg_restart_block, dbg_blkid);
	v6->reorg_restart_block = v7->reorg_restart_block;
	/* The following is not used by REORG -UPGRADE anymore and is available for repurposing */
	assert_conversion(v7->reorg_upgrd_dwngrd_restart_block, dbg_blkid);
	v6->reorg_upgrd_dwngrd_restart_block = v7->reorg_upgrd_dwngrd_restart_block;
	if ((ublock_id_32)(v7->blks_to_upgrd & 0xffffffff) == v7->blks_to_upgrd)
	{
		v6->blks_to_upgrd = v7->blks_to_upgrd & 0xffffffff;
		v6->blks_to_upgrd_subzero_error = v7->blks_to_upgrd_subzero_error;
	} else
	{	/* Blocks to upgrade should never be larger than a block_id_32, but DSE can make it happen */
		v6->blks_to_upgrd = 0;
		v6->blks_to_upgrd_subzero_error = v7->blks_to_upgrd;
	}
	/* REORG -TRUNCATE holds crit while modifying these */
	assert_conversion(v7->before_trunc_total_blks, dbg_blkid);
	v6->before_trunc_total_blks = v7->before_trunc_total_blks;
	assert_conversion(v7->after_trunc_total_blks, dbg_blkid);
	v6->after_trunc_total_blks = v7->after_trunc_total_blks;
	assert_conversion(v7->before_trunc_free_blocks, dbg_blkid);
	v6->before_trunc_free_blocks = v7->before_trunc_free_blocks;
	/* REORG -ENCRYPT holds crit for this */
	assert_conversion_cast(v7->encryption_hash_cutoff, dbg_blkid, block_id_32);
	v6->encryption_hash_cutoff = v7->encryption_hash_cutoff;
	v6->trans_hist.curr_tn = v7->trans_hist.curr_tn;
	v6->trans_hist.early_tn = v7->trans_hist.early_tn;
	v6->trans_hist.last_mm_sync = v7->trans_hist.last_mm_sync;
	v6->trans_hist.mm_tn = v7->trans_hist.mm_tn;
	v6->trans_hist.lock_sequence = v7->trans_hist.lock_sequence;
	v6->trans_hist.ccp_jnl_filesize = v7->trans_hist.ccp_jnl_filesize;
	/* DSE can change total_blks and free_blocks without crit. Cannot assert check these due to endian cvt */
	v6->trans_hist.total_blks = v7->trans_hist.total_blks;
	v6->trans_hist.free_blocks = v7->trans_hist.free_blocks;
	v6->offset = v7->offset;
	v6->max_rec = v7->max_rec;
	v6->i_reserved_bytes = v7->i_reserved_bytes;
	v6->last_start_backup = v7->last_start_backup;
	v6->problksplit = v7->problksplit;
}
