/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUPIP_UPGRADE_INCLUDED
#define MUPIP_UPGRADE_INCLUDED

void mupip_upgrade(void);
enum cdb_sc upgrade_idx_block(block_id *curr_blk, gd_region *reg, mname_entry *gvname, cache_rec_ptr_t *cr);
enum cdb_sc find_gvt_roots(block_id *curr_blk, gd_region *reg, cache_rec_ptr_t *cr);
enum cdb_sc find_big_sib(block_id blk, int level);

#endif /* MUPIP_UPGRADE_INCLUDED */
