/****************************************************************
 *								*
 * Copyright (c) 2005-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GDS_BLK_UPGRADE_INCLUDED
#define GDS_BLK_UPGRADE_INCLUDED

void gds_blk_downgrade(v15_blk_hdr_ptr_t gds_blk_trg, blk_hdr_ptr_t gds_blk_src);

#define	IS_GDS_BLK_DOWNGRADE_NEEDED(ondskblkver)	FALSE /* (GDSV7 <= (ondskblkver)) if v7 -> V6 downgrade were to exist */

#endif
