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

#ifndef WCS_MM_RECOVER_H_INCLUDED
#define WCS_MM_RECOVER_H_INCLUDED

#define MM_DBFILEXT_REMAP_IF_NEEDED(csa, reg)							\
{												\
	if (csa->total_blks != csa->ti->total_blks)						\
		wcs_mm_recover(reg);								\
	assert(!csa->now_crit || (csa->total_blks == csa->ti->total_blks));			\
}

#define CHECK_MM_DBFILEXT_REMAP_IF_NEEDED(csa, reg)						\
{												\
	if (dba_mm == reg->dyn.addr->acc_meth)							\
	{											\
		MM_DBFILEXT_REMAP_IF_NEEDED(csa, reg)						\
	}											\
}

void wcs_mm_recover(gd_region *reg);

#endif
