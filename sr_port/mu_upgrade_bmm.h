/****************************************************************
 *								*
 * Copyright (c) 2021-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef MU_REORG_EXTEND_DEFINED
#define MU_REORG_EXTEND_DEFINED

enum cdb_sc	adjust_master_map(block_id blks_in_way, gd_region *reg);
enum cdb_sc	ditch_dead_globals(block_id curr_blk, block_id offset, cache_rec_ptr_t *cr);
enum cdb_sc	key_stash_killed_gbl_names(block_id curr_blk, block_id max_blk, mident *cache, mident *key_to_find);
enum cdb_sc 	gen_hist_for_blk(srch_blk_status *blkhist, sm_uc_ptr_t blkBase2, sm_uc_ptr_t recBase,  mname_entry *gvname,
					gvnh_reg_t *gvnh_reg);
enum cdb_sc	upgrade_dir_tree(block_id curr_blk, block_id offset, gd_region *reg, block_id_32 *lost, cache_rec_ptr_t *cr);
int4		mu_upgrade_bmm(gd_region *reg, size_t blocks_needed);
int4		move_root_block(block_id new_blk_num, block_id old_blk_num, gvnh_reg_t *gvnh_reg, kill_set *kill_set_list);
int4		upgrade_extend(gtm_int8 extension, gd_region *reg);

#define	MUPIP_UPGRADE_OFF		0
#define	MUPIP_UPGRADE_IN_PROGRESS	1
#define	MUPIP_REORG_UPGRADE_IN_PROGRESS	2

#endif
