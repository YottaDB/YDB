/****************************************************************
 *								*
 *	Copyright 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_ipc.h"
#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_fcntl.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"
#include "iosp.h"

#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h> /* for kill(), SIGTERM, SIGQUIT */

#include "eintr_wrapper_semop.h"
#include "gtm_sem.h"
#include "gtm_c_stack_trace.h"
#include "gtm_semutils.h"

GBLREF uint4			process_id;
GBLREF volatile uint4		heartbeat_counter;

error_def(ERR_CRITSEMFAIL);
error_def(ERR_SEMWT2LONG);

#define FULLTIME(MAX_HRTBT_DELTA, START_HRTBT_CNTR)	(MAX_HRTBT_DELTA - 1 == (heartbeat_counter - START_HRTBT_CNTR))
#define HALFTIME(MAX_HRTBT_DELTA, START_HRTBT_CNTR)	(MAX_HRTBT_DELTA / 2 == (heartbeat_counter - START_HRTBT_CNTR))
#define STACKTRACE_TIME(MAX_HRTBT_DELTA, START_HRTBT_CNTR)	(HALFTIME(MAX_HRTBT_DELTA, START_HRTBT_CNTR) ||	\
									FULLTIME(MAX_HRTBT_DELTA, START_HRTBT_CNTR))
#define USER_SPECIFIED_TIME_EXPIRED(MAX_HRTBT_DELTA, START_HRTBT_CNTR)	(MAX_HRTBT_DELTA <= (heartbeat_counter - START_HRTBT_CNTR))

boolean_t do_blocking_semop(int semid, struct sembuf *sop, int sopcnt, enum gtm_semtype semtype, uint4 start_hrtbt_cntr,
				semwait_status_t *retstat)
{
	boolean_t			need_stacktrace, indefinite_wait;
	const char			*msgstr = NULL;
	int				status = SS_NORMAL, save_errno, sem_pid;
	uint4				loopcnt = 0, max_hrtbt_delta, lcl_hrtbt_cntr, stuck_cnt = 0;
	boolean_t			stacktrace_issued = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	sop[0].sem_flg = sop[1].sem_flg = sop[2].sem_flg = SEM_UNDO;
	max_hrtbt_delta = TREF(dbinit_max_hrtbt_delta);
	assert(NO_SEMWAIT_ON_EAGAIN != max_hrtbt_delta);
	if (indefinite_wait = (INDEFINITE_WAIT_ON_EAGAIN == max_hrtbt_delta))
		max_hrtbt_delta = DEFAULT_DBINIT_MAX_HRTBT_DELTA;
	need_stacktrace = (DEFAULT_DBINIT_MAX_HRTBT_DELTA <= max_hrtbt_delta);
	if (!need_stacktrace)
	{	/* Since the user specified wait time is less than the default wait, wait that time without any stack trace */
		do
		{
			status = semop(semid, sop, sopcnt);
		} while ((-1 == status) && (EINTR == errno) && !USER_SPECIFIED_TIME_EXPIRED(max_hrtbt_delta, start_hrtbt_cntr));
		if (-1 != status)
			return TRUE;
		save_errno = errno;
		if (EINTR == save_errno)
		{	/* someone else is holding it and we are done waiting */
			sem_pid = semctl(semid, 0, GETPID);
			if (-1 != sem_pid)
				RETURN_SEMWAIT_FAILURE(retstat, 0, op_invalid_sem_syscall, ERR_SEMWT2LONG, 0, sem_pid);
			save_errno = errno;
		}
	} else
	{
		lcl_hrtbt_cntr = heartbeat_counter;
		while (!USER_SPECIFIED_TIME_EXPIRED(max_hrtbt_delta, start_hrtbt_cntr))
		{
			loopcnt++;
			status = semop(semid, sop, sopcnt);
			if (-1 != status)
			{
				SENDMSG_SEMOP_SUCCESS_IF_NEEDED(stacktrace_issued, semtype);
				return TRUE;
			}
			save_errno = errno;
			if (EINTR != save_errno)
				break;
			if (lcl_hrtbt_cntr != heartbeat_counter)
			{	/* We waited for at least one heartbeat. This is to ensure that we don't prematurely conclude
				 * that we waited enough for the semop to return before taking a stack trace of the holding
				 * process.
				 */
				sem_pid = semctl(semid, 0, GETPID);
				if (-1 != sem_pid)
				{
					if (STACKTRACE_TIME(max_hrtbt_delta, start_hrtbt_cntr))
					{	/* We want to take the stack trace at half-time and after the wait. But, since
						 * the loop will continue ONLY as long as the specified wait time has not yet
						 * elapsed, get the stack trace at half-time and at the penultimate heartbeat.
						 */
						stuck_cnt++;
						if (gtm_ftok_sem == semtype)
							msgstr = (1 == stuck_cnt) ? "SEMWT2LONG_FTOK_INFO" : "SEMWT2LONG_FTOK";
						else
							msgstr = (1 == stuck_cnt) ? "SEMWT2LONG_ACCSEM_INFO" : "SEMWT2LONG_ACCSEM";
						if ((0 != sem_pid) && (sem_pid != process_id))
						{
							GET_C_STACK_FROM_SCRIPT(msgstr, process_id, sem_pid, stuck_cnt);
							if (TREF(gtm_environment_init))
								stacktrace_issued = TRUE;
						}
					}
					lcl_hrtbt_cntr = heartbeat_counter;
					continue;
				}
				save_errno = errno;	/* for the failed semctl, fall-through */
				break;
			}
		}
		if ((0 == loopcnt) || (EINTR == save_errno))
		{	/* the timer has expired */
			if (!indefinite_wait && !TREF(gtm_environment_init))
				RETURN_SEMWAIT_FAILURE(retstat, 0, op_invalid_sem_syscall, ERR_SEMWT2LONG, 0, sem_pid);
			SEMOP(semid, sop, sopcnt, status, NO_WAIT); /* ignore EINTR if asked for indefinite wait or run in-house */
			if (-1 != status)
			{
				SENDMSG_SEMOP_SUCCESS_IF_NEEDED(stacktrace_issued, semtype);
				return TRUE;
			}
			save_errno = errno;
			if (TREF(gtm_environment_init) && SEM_REMOVED(save_errno))
			{	/* If the semaphore is removed (possible by a concurrent gds_rundown), the caller will retry
				 * by doing semget once again. In most cases this will succeed and the process will get hold
				 * of the semaphore and so the SEM_REMOVED condition can be treated as if the semop succeeded
				 * So, log a SEMOP success message in the syslog.
				 */
				SENDMSG_SEMOP_SUCCESS_IF_NEEDED(stacktrace_issued, semtype);
			}
			/* else some other error occurred; fall-through */
		}
		/* else some other error occurred; fall-through */
	}
	assert(0 != save_errno);
	RETURN_SEMWAIT_FAILURE(retstat, save_errno, op_semctl_or_semop, ERR_CRITSEMFAIL, 0, 0);
}
