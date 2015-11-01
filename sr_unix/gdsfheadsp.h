/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GDSFHEADSP_H__
#define __GDSFHEADSP_H__

int dsk_write(gd_region *r, block_id blk, cache_rec_ptr_t cr);
int dsk_write_nocache(gd_region *r, block_id blk, sm_uc_ptr_t buff, enum db_ver ondsk_blkver);
void wcs_clean_dbsync_timer(sgmnt_addrs *csa);

#endif
