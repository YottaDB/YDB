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

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "tp_change_reg.h"

GBLREF uint4		process_id;
GBLREF gd_region 	*gv_cur_region;

enum cdb_sc	rel_read_crit(gd_region *reg, short crash_ct)
{
#ifdef READ_CRIT_ALIVE
	sgmnt_addrs  	*csa;
	int4		coidx;
	gd_region	*r_save;
	enum cdb_sc	status;

	csa = &FILE_INFO(reg)->s_addrs;
	assert(csa->now_crit == FALSE);

	if (csa->read_lock)
	{
		CRIT_TRACE(crit_ops_rr);		/* see gdsbt.h for comment on placement */
		r_save = gv_cur_region; /* set gv_cur_region for LOCK_HIST */
		gv_cur_region = reg;
		tp_change_reg();
#if defined(UNIX)
		status = mutex_unlockr(reg, crash_ct);
#elif defined(VMS)
		status = mutex_unlockr(csa->critical, crash_ct, &csa->read_lock);
#endif
		gv_cur_region = r_save; /* restore gv_cur_region */
		tp_change_reg();

		return(status);
	} else
		GTMASSERT;
#else
	GTMASSERT;
	return(cdb_sc_dbccerr); /* keep the compiler happy with a return statement */
#endif
}
