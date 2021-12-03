/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
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
enum cdb_sc upgrade_gvt(block_id curr_blk, int4 blk_size, gd_region *reg, mstr *root);
enum cdb_sc upgrade_idx_block(block_id curr_blk, int4 blk_size, gd_region *reg, mstr *name);
enum cdb_sc find_big_sib(block_id blk, int level);

#endif /* MUPIP_UPGRADE_INCLUDED */
