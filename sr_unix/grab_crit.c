/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "gdsbgtr.h"
#include "filestruct.h"
#include "send_msg.h"
#include "mutex.h"
#include "wcs_recover.h"
#include "deferred_signal_handler.h"
#include "have_crit.h"
#include "caller_id.h"
#include "is_proc_alive.h"

GBLREF	volatile int4		crit_count;
GBLREF	short			crash_count;
GBLREF	VSIG_ATOMIC_T		forced_exit;
GBLREF	boolean_t		mutex_salvaged;
GBLREF	uint4 			process_id;
GBLREF	node_local_ptr_t	locknl;

void	grab_crit(gd_region *reg)
{
	unix_db_info		*udi;
	sgmnt_addrs		*csa;
	enum cdb_sc		status;
	mutex_spin_parms_ptr_t	mutex_spin_parms;

	error_def(ERR_DBCCERR);
	error_def(ERR_CRITRESET);
	error_def(ERR_WCBLOCKED);

	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	if (!csa->now_crit)
	{
		assert(0 == crit_count);
		crit_count++;	/* prevent interrupts */
		DEBUG_ONLY(locknl = csa->nl;)	/* for DEBUG_ONLY LOCK_HIST macro */
		mutex_spin_parms = (mutex_spin_parms_ptr_t)&csa->hdr->mutex_spin_parms;
		status = mutex_lockw(reg, mutex_spin_parms, crash_count);
		DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
		if (status != cdb_sc_normal)
		{
			crit_count = 0;
			switch(status)
			{
				case cdb_sc_critreset:
					rts_error(VARLSTCNT(4) ERR_CRITRESET, 2, REG_LEN_STR(reg));
				case cdb_sc_dbccerr:
					rts_error(VARLSTCNT(4) ERR_DBCCERR, 2, REG_LEN_STR(reg));
				default:
					if (forced_exit && 0 == have_crit(CRIT_HAVE_ANY_REG))
						deferred_signal_handler();
					GTMASSERT;
			}
			return;
		}
		/* There is only one case we know of when csa->nl->in_crit can be non-zero and that is when a process holding
		 * crit gets kill -9ed and another process ends up invoking "secshr_db_clnup" which in turn clears the
		 * crit semaphore (making it available for waiters) but does not also clear csa->nl->in_crit since it does not
		 * hold crit at that point. But in that case, the pid reported in csa->nl->in_crit should be dead. Check that.
		 */
		assert((0 == csa->nl->in_crit) || (FALSE == is_proc_alive(csa->nl->in_crit, 0)));
		csa->nl->in_crit = process_id;
		CRIT_TRACE(crit_ops_gw);	/* see gdsbt.h for comment on placement */
		if (mutex_salvaged) /* Mutex crash repaired, want to do write cache recovery, just in case */
		{
			SET_TRACEABLE_VAR(csa->hdr->wc_blocked, TRUE);
			BG_TRACE_PRO_ANY(csa, wcb_grab_crit);
			send_msg(VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_grab_crit"),
				process_id, &csa->ti->curr_tn, DB_LEN_STR(reg));
		}
		crit_count = 0;
	}
	if (csa->hdr->wc_blocked)
		wcs_recover(reg);
	return;
}
