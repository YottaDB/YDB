/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include "mutex.h"
#include "grab_read_crit.h"

GBLREF	uint4			process_id;
GBLREF	node_local_ptr_t	locknl;

enum cdb_sc 	grab_read_crit(gd_region *reg, short crash_ct)
{
	sgmnt_addrs  	*csa;
	enum cdb_sc	status;

	csa = &FILE_INFO(reg)->s_addrs;
	if (csa->read_lock)
		return(cdb_sc_normal);
	DEBUG_ONLY(locknl = csa->nl;)	/* for DEBUG_ONLY LOCK_HIST macro */
	UNIX_ONLY(status = mutex_lockr(reg, &csa->hdr->mutex_spin_parms, crash_ct);)
	VMS_ONLY(status = MUTEX_LOCKR(csa->critical, crash_ct, &csa->read_lock, &csa->hdr->mutex_spin_parms);)
	DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
	if (status == cdb_sc_normal)
		CRIT_TRACE(crit_ops_gr);		/* see gdsbt.h for comment on placement */
	return status;
}
