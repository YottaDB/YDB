/****************************************************************
 *								*
 * Copyright (c) 2011-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019-2021 YottaDB LLC and/or its subsidiaries.	*
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
#include "wbox_test_init.h"
#include "gt_timer.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "eintr_wrappers.h"

#define MAX_SEM_WAIT_TIME_IN_SECONDS	60	/* 60 seconds */

GBLREF	uint4            process_id;
GBLREF	boolean_t	exit_handler_active;
#ifdef DEBUG
GBLREF	gd_region	*gv_cur_region;
GBLREF  sgmnt_addrs     *cs_addrs;
#endif
int try_semop_get_c_stack(int semid, struct sembuf sops[], int nsops)
{
	int                     stuckcnt;
	int                     semop_pid, save_errno;
	int                     rc;
	boolean_t		new_timeout, get_stack_trace;
	ABS_TIME		start_time, end_time, remaining_time;
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
	new_timeout = TRUE;	/* Start a "semtimedop()" call with MAX_SEM_WAIT_TIME_IN_SECONDS second timeout */
	/* If YottaDB exit handler is already active (i.e. "exit_handler_active" is TRUE), then it is not safe to start
	 * timers (YDB#679). So use "semtimedop()" instead of "semop()" to achieve the effort of a timer without actually
	 * starting one. Because "semtimedop()" does not accurately maintain the remaining timeout in case of an EINTR,
	 * we maintain it ourselves outside of the "semtimedop()" call in a loop below using "remaining_time".
	 */
	do
	{
<<<<<<< HEAD
		if (new_timeout)
		{
			new_timeout = FALSE;
			sys_get_curr_time(&start_time);
			remaining_time.tv_sec = MAX_SEM_WAIT_TIME_IN_SECONDS;
			remaining_time.tv_nsec = 0;
			end_time.tv_sec = start_time.tv_sec + remaining_time.tv_sec;
			end_time.tv_nsec = start_time.tv_nsec;
		}
		rc = semtimedop(semid, sops, nsops, &remaining_time);
		if (-1 != rc)
			break;
		save_errno = errno;
		if (EAGAIN == save_errno)
			get_stack_trace = TRUE; /* "semtimedop()" returned because time limit was reached. Get stack trace */
		else if (EINTR != save_errno)
			break;
		else
		{
			ABS_TIME	cur_time;

			eintr_handling_check();
		 	/* "semtimedop()" got an EINTR due to some other signal. Determine remaining time before next stack trace */
			sys_get_curr_time(&cur_time);
			remaining_time = sub_abs_time(&end_time, &cur_time);
			get_stack_trace = ((0 > remaining_time.tv_sec)
						|| ((0 == remaining_time.tv_sec) && (0 == remaining_time.tv_nsec)));
		}
		if (get_stack_trace)
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
=======
		if (TREF(semwait2long))
		{	/* 1st time or semwait2long timer pop mean we need to (re)start timer */
			TREF(semwait2long) = FALSE;
			start_timer((TID)semwt2long_handler,(int4)MAX_SEM_WAIT_TIME, semwt2long_handler, 0, NULL);
		}
		rc = semop(semid, sops, nsops);
		if (-1 == rc)
		{	/* didn't work */
			save_errno = errno;
			if ((EINTR == save_errno) && TREF(semwait2long))
			{	/* semwait2long timer popped, get C-stack trace */
				stuckcnt++;
				last_sem_trace = -1;
				for (loopcount = 0; loopcount < nsops; loopcount++)
				{	/* for does 1 pass of each semop in set */
					if ((last_sem_trace != sops[loopcount].sem_num) && (0 == sops[loopcount].sem_op))
					{	/* Do not take trace of the same process again, in a point of time */
						last_sem_trace = sops[loopcount].sem_num;
						semop_pid = semctl(semid, sops[loopcount].sem_num, GETPID);
						if (-1 == semop_pid)
						{	/* no owner to trace leave the for loop with the errno from semctl */
							save_errno = errno;
							rc = -1;
							break;
						}
						if (semop_pid != process_id)
						{	/* not good to try tracing own process */
							GET_C_STACK_FROM_SCRIPT("SEMOP_INFO", process_id, semop_pid, stuckcnt);
							/* Got stack trace signal the first process to continue */
>>>>>>> e9a1c121 (GT.M V6.3-014)
#							ifdef DEBUG
						if (cnl)
							GTM_WHITE_BOX_TEST(WBTEST_SEMTOOLONG_STACK_TRACE,
								cnl->wbox_test_seq_num, 3);
#							endif
<<<<<<< HEAD
					} else if (-1 == semop_pid)
					{
						save_errno = errno;
						break;
					}
				}
			}
			new_timeout = TRUE; /* Start a new "semtimedop()" call for another MAX_SEM_WAIT_TIME_IN_SECONDS seconds */
		}
	} while (TRUE);
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;
=======
						}
					}
				}	/* for */
			}	/* our timer pop */
		}	/* error return from semop */
	} while ((-1 == rc) && (EINTR == save_errno));	/* keep trying as long as error is an EINTR */
	cancel_timer((TID)semwt2long_handler);
>>>>>>> e9a1c121 (GT.M V6.3-014)
	return (-1 == rc) ? save_errno : 0;
}
