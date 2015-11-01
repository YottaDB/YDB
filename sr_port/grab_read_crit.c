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
#include "caller_id.h"
#include "grab_read_crit.h"

GBLREF	uint4		process_id;
GBLREF	gd_region	*gv_cur_region;

enum cdb_sc 	grab_read_crit(gd_region *reg, short crash_ct)
{
	sgmnt_addrs  	*csa;
	int4		coidx;
	enum cdb_sc	status;
	gd_region	*r_save;
	mutex_spin_parms_ptr_t	mutex_spin_parms;

	csa = &FILE_INFO(reg)->s_addrs;

	if (csa->read_lock)
		return(cdb_sc_normal);

	r_save = gv_cur_region;
	gv_cur_region = reg;
	tp_change_reg();
#if defined(UNIX)
	mutex_spin_parms = (mutex_spin_parms_ptr_t)&csa->hdr->mutex_spin_parms;
	status = mutex_lockr(reg, mutex_spin_parms, crash_ct);
#elif defined(VMS)
	status = mutex_lockr(csa->critical, crash_ct, &csa->read_lock);
#endif
	gv_cur_region = r_save; /* restore gv_cur_region */
	tp_change_reg();

	if (status == cdb_sc_normal)
		CRIT_TRACE(crit_ops_gr);		/* see gdsbt.h for comment on placement */
	return(status);
}
