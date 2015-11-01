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

#include <signal.h>
#include <errno.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmsiginfo.h"
#include "mutex.h"
#include "deferred_signal_handler.h"
#include "have_crit_any_region.h"
#include "caller_id.h"

GBLREF	short 			crash_count;
GBLREF	volatile boolean_t	crit_in_flux;
GBLREF	VSIG_ATOMIC_T		forced_exit;
GBLREF	gd_region 		*gv_cur_region;
GBLREF	int			process_exiting;
GBLREF	uint4 			process_id;
GBLREF	volatile int		suspend_status;

DEBUG_ONLY(
GBLREF	sgmnt_addrs		*cs_addrs;	/* for TP_CHANGE_REG macro */
GBLREF	sgmnt_data_ptr_t	cs_data;
)

void	rel_crit(gd_region *reg)
{

	unix_db_info 		*udi;
	sgmnt_addrs  		*csa;
	int4			coidx;
	enum cdb_sc		status;
	DEBUG_ONLY(gd_region	*r_save;)

	error_def(ERR_CRITRESET);
	error_def(ERR_DBCCERR);

	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	if (csa->now_crit)
	{
		assert(FALSE == crit_in_flux);
		crit_in_flux = TRUE;	/* prevent interrupts */
		assert(csa->nl->in_crit == process_id || csa->nl->in_crit == 0);
		CRIT_TRACE(crit_ops_rw);		/* see gdsbt.h for comment on placement */
		csa->nl->in_crit = 0;
		DEBUG_ONLY(r_save = gv_cur_region; TP_CHANGE_REG(reg));	/* for LOCK_HIST macro which is used only in DEBUG */
		status = mutex_unlockw(reg, crash_count);
		DEBUG_ONLY(TP_CHANGE_REG(r_save));	/* restore gv_cur_region */
		if (status != cdb_sc_normal)
		{
			csa->now_crit = FALSE;
			crit_in_flux = FALSE;
			if (status == cdb_sc_critreset)
				rts_error(VARLSTCNT(4) ERR_CRITRESET, 2, REG_LEN_STR(reg));
			else
			{
				assert(status == cdb_sc_dbccerr);
				rts_error(VARLSTCNT(4) ERR_DBCCERR, 2, REG_LEN_STR(reg));
			}
			return;
		}
		crit_in_flux = FALSE;
	} else
	{
		CRIT_TRACE(crit_ops_nocrit);
	}

	/* Only do this if the process is not already exiting */
	if (forced_exit && !process_exiting && !have_crit_any_region(FALSE))
		deferred_signal_handler();

	if (DEFER_SUSPEND == suspend_status && !have_crit_any_region(FALSE))
		suspend();
}
