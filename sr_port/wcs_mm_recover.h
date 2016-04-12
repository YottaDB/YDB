/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __WCS_MM_RECOVER_H__
#define __WCS_MM_RECOVER_H__

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
