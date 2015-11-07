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

#include "mdef.h"

#include <signal.h>	/* for VSIG_ATOMIC_T type */

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cdb_sc.h"

error_def(ERR_CRITRESET);
error_def(ERR_DBCCERR);

GBLREF volatile int4		crit_count;
GBLREF uint4			exi_condition;
GBLREF uint4			process_id;
GBLREF sgmnt_addrs		*vms_mutex_check_csa;

boolean_t grab_lock(gd_region *reg, boolean_t dummy1, uint4 dummy2)
{
	enum cdb_sc	status;
	sgmnt_addrs	*csa;

	csa = &FILE_INFO(reg)->s_addrs;
	vms_mutex_check_csa = csa;
	assert(!lib$ast_in_prog());

	if (!csa->now_crit)
	{
		assert(0 == crit_count);
		crit_count++;
		if (cdb_sc_normal !=
			(status = MUTEX_LOCKW(csa->critical, 0, &csa->now_crit,
					      (mutex_spin_parms_ptr_t)((sm_uc_ptr_t)csa->critical + JNLPOOL_CRIT_SPACE))))
		{ /* mutex spin parms structure resides at csa->critical + JNLPOOL_CRIT_SPACE, see gtmsource.h for jnlpool layout */
			crit_count = 0;
			switch (status)
			{
			case cdb_sc_critreset:
				rts_error_csa(CSA_ARG(NULL) ERR_CRITRESET, 2, REG_LEN_STR(reg));
			case cdb_sc_dbccerr:
				rts_error_csa(CSA_ARG(NULL) ERR_DBCCERR, 2, REG_LEN_STR(reg));
			default:
				GTMASSERT;
			}
			return TRUE;
		}
		assert(csa->nl->in_crit == 0);
		csa->nl->in_crit = process_id;
		CRIT_TRACE(crit_ops_gw);		/* see gdsbt.h for comment on placement */
		crit_count = 0;
	}
	return TRUE;
}
