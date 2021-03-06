/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GDSFHEADSP_H_INCLUDED
#define GDSFHEADSP_H_INCLUDED

int dsk_write_nocache(gd_region *r, block_id blk, sm_uc_ptr_t buff, enum db_ver ondsk_blkver);
void wcs_clean_dbsync_timer(sgmnt_addrs *csa);

#endif
