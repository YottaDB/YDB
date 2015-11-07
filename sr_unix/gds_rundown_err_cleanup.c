/****************************************************************
*								*
*	Copyright 2013 Fidelity Information Services, Inc	*
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
#include "gds_rundown.h"
#include "jnl.h"
#include "gtm_semutils.h"
#include "do_semop.h"
#include "add_inter.h"
#include "ftok_sems.h"
#include <sys/sem.h>
#include "wcs_clean_dbsync.h"
#include "interlock.h"
#include "wbox_test_init.h"
#include "gds_rundown_err_cleanup.h"

GBLREF	gd_region	*gv_cur_region;
GBLREF 	jnl_gbls_t	jgbl;
GBLREF	uint4		process_id;

error_def(ERR_TEXT);
error_def(ERR_DBRNDWN);

void gds_rundown_err_cleanup(boolean_t have_standalone_access)
{
	pid_t		sem_pid;
	int		semop_res;
	unix_db_info	*udi;
	sgmnt_addrs	*csa;
	boolean_t	cancelled_timer, cancelled_dbsync_timer;

	udi = FILE_INFO(gv_cur_region);
	csa = &udi->s_addrs;
	/* We got here on an error and are going to close the region. Cancel any pending flush timer for this region by this task*/
	CANCEL_DB_TIMERS(gv_cur_region, csa, cancelled_timer, cancelled_dbsync_timer);
	if (csa->now_crit)		/* Might hold crit if wcs_flu or other failure */
	{
		assert(!csa->hold_onto_crit || jgbl.onlnrlbk);
		if (NULL != csa->nl)
			rel_crit(gv_cur_region); /* also sets csa->now_crit to FALSE */
		else
			csa->now_crit = FALSE;
	}
	if (!have_standalone_access)
	{
		if (udi->grabbed_access_sem)
		{	/* release the access control semaphore, if you hold it */
			sem_pid = semctl(udi->semid, 0, GETPID);
			assert(sem_pid == process_id);
			if (0 != (semop_res = do_semop(udi->semid, DB_CONTROL_SEM, -1, SEM_UNDO | IPC_NOWAIT)))
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_CRITSEMFAIL, 2, DB_LEN_STR(gv_cur_region),
					     ERR_TEXT, 2, RTS_ERROR_TEXT("Error releasing access semaphore"), semop_res);
			udi->grabbed_access_sem = FALSE;
		}
		if (udi->counter_acc_incremented)
		{
			if (0 != (semop_res = do_semop(udi->semid, DB_COUNTER_SEM, -1, SEM_UNDO | IPC_NOWAIT)))
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_CRITSEMFAIL, 2, DB_LEN_STR(gv_cur_region),
					     ERR_TEXT, 2, RTS_ERROR_TEXT("Error decreasing access semaphore counter"), semop_res);
			udi->counter_acc_incremented = FALSE;
		}
	}
	if (udi->grabbed_ftok_sem)
	{	/* Decrease counter and release ftok */
		assert(!have_standalone_access);
		ftok_sem_release(gv_cur_region, !have_standalone_access, TRUE);
	} else if (udi->counter_ftok_incremented) /* Just decrease ftok counter */
	{
		if (0 != (semop_res = do_semop(udi->ftok_semid, DB_COUNTER_SEM, -1, SEM_UNDO | IPC_NOWAIT)))
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_CRITSEMFAIL, 2, DB_LEN_STR(gv_cur_region),
				     ERR_TEXT, 2, RTS_ERROR_TEXT("Error decreasing ftok semaphore counter"), semop_res);
		udi->counter_ftok_incremented = FALSE;
	}
	gv_cur_region->open = FALSE;
	csa->nl = NULL;
	REMOVE_CSA_FROM_CSADDRSLIST(csa); /* remove "csa" from list of open regions (cs_addrs_list) */
	send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_DBRNDWN, 2, REG_LEN_STR(gv_cur_region));
}
