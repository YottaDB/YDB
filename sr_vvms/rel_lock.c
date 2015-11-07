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

#include "mdef.h"

#include <signal.h>	/* for VSIG_ATOMIC_T type */

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "have_crit.h"

error_def(ERR_CRITRESET);
error_def(ERR_DBCCERR);

GBLREF	volatile int4		crit_count;
GBLREF	uint4			process_id;

void	rel_lock(gd_region *reg)
{
	enum cdb_sc 	status;
	sgmnt_addrs	*csa;

	csa = &FILE_INFO(reg)->s_addrs;

	if (csa->now_crit)
	{
		assert(0 == crit_count);
		crit_count++;
		assert(csa->nl->in_crit == process_id  ||  csa->nl->in_crit == 0); /* crit was held by this process */
		CRIT_TRACE(crit_ops_rw);		/* see gdsbt.h for comment on placement */

		csa->nl->in_crit = 0;

		if ((status = mutex_unlockw(csa->critical, 0, &csa->now_crit)) != cdb_sc_normal)
		{
			csa->now_crit = FALSE;
			crit_count = 0;
			if (status == cdb_sc_critreset)
				rts_error(ERR_CRITRESET, 2, REG_LEN_STR(reg));
			else
			{
				assert(status == cdb_sc_dbccerr);
				rts_error(ERR_DBCCERR, 2, REG_LEN_STR(reg));
			}
			return;
		}
		crit_count = 0;
	}
	/* Now that crit for THIS region is released, check if deferred signal/exit handling can be done and if so do it */
	DEFERRED_EXIT_HANDLING_CHECK;
}
