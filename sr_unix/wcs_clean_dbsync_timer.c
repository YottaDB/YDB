/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "gdskill.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"
#include "gt_timer.h"		/* for TID definition */
#include "timers.h"		/* for TIM_DEFER_DBSYNC #define */
#include "have_crit.h"
#include "wcs_clean_dbsync.h"

GBLREF	uint4			process_id;

void	wcs_clean_dbsync_timer(sgmnt_addrs *csa)
{
	/* Don't start a timer if we are in wcs_flu(). We could possibly have rundown the region when
	 * the timer pops. In any case wcs_flu() would take care of writing the epoch if needed.
	 * Note that in VMS the check for wcsflu_pid is !cnl->wcsflu_pid while here is process_id != cnl->wcsflu_pid
	 * This is because in VMS, if a process gets killed in the midst of a wcs_flu(), secshr_db_clnup takes care
	 * of cleaning it up while there is currently no such facility in Unix. This means that in Unix, it is
	 * possible we do two dbsyncs (in turn write 2 epochs records if before imaging) one when the wcs_wtstart of
	 * process P1 empties the queue and another by the wcs_flu() of process P2 (waiting on P1 to finish
	 * its wcs_wtstart). But this is considered infrequent enough to be better than skipping writing an
	 * epoch due to incorrect cnl->wcsflu_pid.
	 */
	if (!process_exiting && (process_id != csa->nl->wcsflu_pid) && (FALSE == csa->dbsync_timer))
		START_DBSYNC_TIMER(csa, TIM_DEFER_DBSYNC);
	return;
}

