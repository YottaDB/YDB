/****************************************************************
 *								*
 * Copyright (c) 2011-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_ipc.h"

#include <sys/sem.h>
#include <errno.h>

#include "gtm_c_stack_trace.h"
#include "gtm_c_stack_trace_semop.h"
#include "semwt2long_handler.h"
#include "wbox_test_init.h"
#include "gt_timer.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "eintr_wrappers.h"

GBLREF uint4            process_id;
#ifdef DEBUG
GBLREF	gd_region	*gv_cur_region;
GBLREF  sgmnt_addrs     *cs_addrs;
#endif
int try_semop_get_c_stack(int semid, struct sembuf sops[], int nsops)
{
	int                     stuckcnt;
	int                     semop_pid, save_errno;
	int                     rc;
#	ifdef DEBUG
	node_local_ptr_t        cnl = NULL;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef DEBUG
	if ((NULL != gv_cur_region) && gv_cur_region->open && (NULL != cs_addrs))
		cnl = cs_addrs->nl;
#	endif
	stuckcnt = 0;
	save_errno = 0;
	TREF(semwait2long) = TRUE;
	do
	{
		/* If we are entering the loop for the first time or there was an interrupt and that 	*/
		/* was due to the timer pop (which sets semop2long), restart the timer			*/
		if (TREF(semwait2long))
		{
			TREF(semwait2long) = FALSE;
			start_timer((TID)semwt2long_handler, MAX_SEM_WAIT_TIME, semwt2long_handler, 0, NULL);
		}
		rc = semop(semid, sops, nsops);
		if (-1 != rc)
			break;
		save_errno = errno;
		if (EINTR != save_errno)
			break;
		eintr_handling_check();
		/* Timer popped, get C-stack trace */
		if (TREF(semwait2long))
		{
			int	loopcount, last_sem_trace;

			stuckcnt++;
			last_sem_trace = -1;
			for (loopcount = 0; loopcount < nsops; loopcount++)
			{
				if ((last_sem_trace != sops[loopcount].sem_num) && (0 == sops[loopcount].sem_op))
				{	/* Do not take trace of the same process again, in a point of time */
					last_sem_trace = sops[loopcount].sem_num;
					semop_pid = semctl(semid, sops[loopcount].sem_num, GETPID);
					if ((-1 != semop_pid) && (semop_pid != process_id))
					{
						GET_C_STACK_FROM_SCRIPT("SEMOP_INFO", process_id, semop_pid, stuckcnt);
						/* Got stack trace signal the first process to continue */
#							ifdef DEBUG
						if (cnl)
							GTM_WHITE_BOX_TEST(WBTEST_SEMTOOLONG_STACK_TRACE,
								cnl->wbox_test_seq_num, 3);
#							endif
					} else if (-1 == semop_pid)
					{
						save_errno = errno;
						break;
					}
				}
			}
		}
	} while (TRUE);
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
	cancel_timer((TID)semwt2long_handler);
	return (-1 == rc) ? save_errno : 0;
}
