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

#ifndef __LKE_CLEARLOCK_H__
#define __LKE_CLEARLOCK_H__

bool lke_clearlock(gd_region *region, struct CLB *lnk, mlk_ctldata_ptr_t ctl,
	mlk_shrblk_ptr_t node, mstr *name, bool all, bool interactive, int4 pid);

#endif
