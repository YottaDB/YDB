/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GDSFHEADSP_H__
#define __GDSFHEADSP_H__

int dsk_write(gd_region *r, block_id blk, sm_uc_ptr_t buff);
void wcs_clean_dbsync_timer(sgmnt_addrs *csa);

#endif
