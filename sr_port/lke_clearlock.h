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

#ifndef LKE_CLEARLOCK_H_INCLUDED
#define LKE_CLEARLOCK_H_INCLUDED

bool lke_clearlock(mlk_pvtctl_ptr_t pctl, struct CLB *lnk, mlk_shrblk_ptr_t node, mstr *name,
			bool all, bool interactive, int4 pid);

#endif
