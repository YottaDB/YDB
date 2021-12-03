/****************************************************************
 *								*
 * Copyright (c) 2020-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

	assert(SIZEOF(v6_sgmnt_data) == SIZEOF(sgmnt_data));
	v7->master_map_len = v6->master_map_len;
	v7->start_vbn = v6->start_vbn;
	v7->last_inc_bkup_last_blk = v6->last_inc_bkup_last_blk;
	v7->last_com_bkup_last_blk = v6->last_com_bkup_last_blk;
	v7->last_rec_bkup_last_blk = v6->last_rec_bkup_last_blk;
	v7->reorg_restart_block = v6->reorg_restart_block;
	v7->reorg_upgrd_dwngrd_restart_block = v6->reorg_upgrd_dwngrd_restart_block;
	v7->blks_to_upgrd = v6->blks_to_upgrd;
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
}

/* Convert the header from v7 to v6 format */
void db_header_dwnconv(sgmnt_data_ptr_t v7)
{
	v6_sgmnt_data_ptr_t	v6 = (v6_sgmnt_data_ptr_t)v7;
	DEBUG_ONLY(block_id	dbg_blkid);
	DEBUG_ONLY(gtm_int8	dbg_int8);

	assert(SIZEOF(v6_sgmnt_data) == SIZEOF(sgmnt_data));
	DEBUG_ONLY(dbg_int8 = v7->master_map_len);
	assert((int4)(v7->master_map_len) == v7->master_map_len);
	v6->master_map_len = v7->master_map_len;
	DEBUG_ONLY(dbg_blkid = v7->start_vbn);
	assert((block_id_32)(v7->start_vbn) == v7->start_vbn);
	v6->start_vbn = v7->start_vbn;
	DEBUG_ONLY(dbg_blkid = v7->last_inc_bkup_last_blk);
	assert((block_id_32)(v7->last_inc_bkup_last_blk) == v7->last_inc_bkup_last_blk);
	v6->last_inc_bkup_last_blk = v7->last_inc_bkup_last_blk;
	DEBUG_ONLY(dbg_blkid = v7->last_com_bkup_last_blk);
	assert((block_id_32)(v7->last_com_bkup_last_blk) == v7->last_com_bkup_last_blk);
	v6->last_com_bkup_last_blk = v7->last_com_bkup_last_blk;
	DEBUG_ONLY(dbg_blkid = v7->last_rec_bkup_last_blk);
	assert((block_id_32)(v7->last_rec_bkup_last_blk) == v7->last_rec_bkup_last_blk);
	v6->last_rec_bkup_last_blk = v7->last_rec_bkup_last_blk;
	DEBUG_ONLY(dbg_blkid = v7->reorg_restart_block);
	assert((block_id_32)(v7->reorg_restart_block) == v7->reorg_restart_block);
	v6->reorg_restart_block = v7->reorg_restart_block;
	DEBUG_ONLY(dbg_blkid = v7->reorg_upgrd_dwngrd_restart_block);
	assert((block_id_32)(v7->reorg_upgrd_dwngrd_restart_block) == v7->reorg_upgrd_dwngrd_restart_block);
	v6->reorg_upgrd_dwngrd_restart_block = v7->reorg_upgrd_dwngrd_restart_block;
	DEBUG_ONLY(dbg_blkid = v7->blks_to_upgrd);
	assert((block_id_32)(v7->blks_to_upgrd) == v7->blks_to_upgrd);
	v6->blks_to_upgrd = v7->blks_to_upgrd;
	DEBUG_ONLY(dbg_blkid = v7->blks_to_upgrd_subzero_error);
	assert((block_id_32)(v7->blks_to_upgrd_subzero_error) == v7->blks_to_upgrd_subzero_error);
	v6->blks_to_upgrd_subzero_error = v7->blks_to_upgrd_subzero_error;
	DEBUG_ONLY(dbg_blkid = v7->before_trunc_total_blks);
	assert((block_id_32)(v7->before_trunc_total_blks) == v7->before_trunc_total_blks);
	v6->before_trunc_total_blks = v7->before_trunc_total_blks;
	DEBUG_ONLY(dbg_blkid = v7->after_trunc_total_blks);
	assert((block_id_32)(v7->after_trunc_total_blks) == v7->after_trunc_total_blks);
	v6->after_trunc_total_blks = v7->after_trunc_total_blks;
	DEBUG_ONLY(dbg_blkid = v7->before_trunc_free_blocks);
	assert((block_id_32)(v7->before_trunc_free_blocks) == v7->before_trunc_free_blocks);
	v6->before_trunc_free_blocks = v7->before_trunc_free_blocks;
	DEBUG_ONLY(dbg_blkid = v7->encryption_hash_cutoff);
	assert((block_id_32)(v7->encryption_hash_cutoff) == v7->encryption_hash_cutoff);
	v6->encryption_hash_cutoff = v7->encryption_hash_cutoff;
	v6->trans_hist.curr_tn = v7->trans_hist.curr_tn;
	v6->trans_hist.early_tn = v7->trans_hist.early_tn;
	v6->trans_hist.last_mm_sync = v7->trans_hist.last_mm_sync;
	v6->trans_hist.mm_tn = v7->trans_hist.mm_tn;
	v6->trans_hist.lock_sequence = v7->trans_hist.lock_sequence;
	v6->trans_hist.ccp_jnl_filesize = v7->trans_hist.ccp_jnl_filesize;
	DEBUG_ONLY(dbg_blkid = v7->trans_hist.total_blks);
	assert((block_id_32)(v7->trans_hist.total_blks) == v7->trans_hist.total_blks);
	v6->trans_hist.total_blks = v7->trans_hist.total_blks;
	DEBUG_ONLY(dbg_blkid = v7->trans_hist.free_blocks);
	assert((block_id_32)(v7->trans_hist.free_blocks) == v7->trans_hist.free_blocks);
	v6->trans_hist.free_blocks = v7->trans_hist.free_blocks;
	v6->offset = v7->offset;
	v6->max_rec = v7->max_rec;
	v6->i_reserved_bytes = v7->i_reserved_bytes;
	v6->last_start_backup = v7->last_start_backup;
	v6->db_got_to_V7_once = FALSE;
}
