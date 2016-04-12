/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __LKE_CLEARTREE_H__
#define __LKE_CLEARTREE_H__

bool lke_cleartree(gd_region *region, struct CLB *lnk, mlk_ctldata_ptr_t ctl,
	mlk_shrblk_ptr_t tree, bool all, bool interactive, int4 pid, mstr one_lock, boolean_t exact);

#endif
