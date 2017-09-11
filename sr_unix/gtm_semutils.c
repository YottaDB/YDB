/****************************************************************
 *								*
 * Copyright (c) 2011-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gtm_signal.h" /* for kill(), SIGTERM, SIGQUIT */
#include "iosp.h"

#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <errno.h>

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "eintr_wrapper_semop.h"
#include "gtm_sem.h"
#include "gtm_c_stack_trace.h"
#include "gtm_semutils.h"
#include "gtmimagename.h"
#include "do_semop.h"
#include "filestruct.h"

GBLREF uint4			process_id;
GBLREF bool			in_mupip_freeze;

error_def(ERR_CRITSEMFAIL);
error_def(ERR_SEMWT2LONG);
error_def(ERR_RESRCWAIT);
error_def(ERR_RESRCINTRLCKBYPAS);
error_def(ERR_TEXT);

#define FULLTIME(MAX_HRTBT_DELTA, START_HRTBT_CNTR)	(MAX_HRTBT_DELTA - 1 == (heartbeat_counter - START_HRTBT_CNTR))
#define HALFTIME(MAX_HRTBT_DELTA, START_HRTBT_CNTR)	(MAX_HRTBT_DELTA / 2 == (heartbeat_counter - START_HRTBT_CNTR))
#define STACKTRACE_TIME(MAX_HRTBT_DELTA, START_HRTBT_CNTR)	(HALFTIME(MAX_HRTBT_DELTA, START_HRTBT_CNTR) ||	\
									FULLTIME(MAX_HRTBT_DELTA, START_HRTBT_CNTR))
#define USER_SPECIFIED_TIME_EXPIRED(MAX_HRTBT_DELTA, START_HRTBT_CNTR)	(MAX_HRTBT_DELTA <= (heartbeat_counter - START_HRTBT_CNTR))
#define IS_ACCESS_SEM	(gtm_access_sem == semtype)
#define IS_FTOK_SEM	(gtm_ftok_sem == semtype)


boolean_t do_blocking_semop(int semid, enum gtm_semtype semtype, boolean_t *stacktrace_time, boolean_t *timedout,
				semwait_status_t *retstat, gd_region *reg, boolean_t *bypass, boolean_t *sem_halted,
				boolean_t incr_cnt)
{
	boolean_t			need_stacktrace, indefinite_wait;
	char				*msgstr;
	int				status = SS_NORMAL, save_errno, sem_pid, semval, i, sopcnt;
	uint4				loopcnt = 0, max_hrtbt_delta, lcl_hrtbt_cntr, stuck_cnt = 0;
	boolean_t			stacktrace_issued = FALSE, ok_to_bypass;
	struct sembuf			sop[3];
	char				*sem_names[2] = {"FTOK", "access control"}; /* based on gtm_semtype enum order */

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(IS_FTOK_SEM || IS_ACCESS_SEM);
	assert(NO_SEMWAIT_ON_EAGAIN != TREF(dbinit_max_delta_secs));
	assert(!((NULL == timedout) ^ (NULL == stacktrace_time)));
	*sem_halted = FALSE;
	/* Access control semaphore should not be increased when the process is readonly */
	SET_GTM_SOP_ARRAY(sop, sopcnt, (incr_cnt && (IS_FTOK_SEM || !reg->read_only)), (SEM_UNDO | IPC_NOWAIT));
	/* If DSE or LKE or MUPIP FREEZE -ONLINE, it is okay to bypass but only if input "*bypass" is TRUE.
	 * If "*bypass" is FALSE, that overrides anything else.
	 */
	ok_to_bypass = *bypass && (IS_DSE_IMAGE || IS_LKE_IMAGE || (IS_MUPIP_IMAGE && in_mupip_freeze));
	*bypass = FALSE;	/* set default value of "*bypass" first. Can be overridden later if needed */
	indefinite_wait = (NULL == timedout);
	need_stacktrace = (DEFAULT_DBINIT_MAX_DELTA_SECS <= TREF(dbinit_max_delta_secs)) && !ok_to_bypass;
	if (!need_stacktrace)
	{	/* Since the user specified wait time is less than the default wait, wait that time without any stack trace */
		if (ok_to_bypass)
		{	/* ok_to_bypass == TRUE means we can bypass after 3 seconds of wait. IPC_NOWAIT semop every second.
			 * The semaphore value must be at least 2 to make sure the shared memeory is already created.
			 */
			if (-1 == (semval = semctl(semid, DB_COUNTER_SEM, GETVAL))) /* semval = number of process attached */
				RETURN_SEMWAIT_FAILURE(retstat, errno, op_semctl, ERR_CRITSEMFAIL, 0, 0);
			if (DB_COUNTER_SEM_INCR < semval)
			{
				if (-1 == (sem_pid = semctl(semid, 0, GETPID)))
					RETURN_SEMWAIT_FAILURE(retstat, errno, op_semctl, ERR_CRITSEMFAIL, 0, 0);
				i = 0;
				do
				{
					SEMOP(semid, sop, sopcnt, status, NO_WAIT);
					save_errno = errno;
					if ((-1 == status) && (ERANGE == save_errno))
					{
						if (!(*sem_halted))
						{
							sopcnt = 2; /* ignore the increment operation */
							*sem_halted = TRUE;
							continue; /* Try again */
						}
					} else
					{
						if (0 == i)
						{
							send_msg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_RESRCWAIT, 8,
								LEN_AND_STR(sem_names[semtype]), REG_LEN_STR(reg),
								DB_LEN_STR(reg), sem_pid, semid);
						}
						LONG_SLEEP(1);
						i++;
					}
				} while ((-1 == status) && ((EAGAIN == save_errno) || (ERANGE == save_errno))
											&& (i < MAX_BYPASS_WAIT_SEC));
				if (-1 != status)
					return TRUE;
				assert(EINTR != save_errno);
				if (EAGAIN == save_errno)
				{
					*bypass = TRUE;
					save_errno = 0;
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(12) ERR_RESRCINTRLCKBYPAS, 10,
						 LEN_AND_STR((IS_LKE_IMAGE ? "LKE" : "DSE")), process_id,
						 LEN_AND_STR(sem_names[semtype]), REG_LEN_STR(reg), DB_LEN_STR(reg), sem_pid);
					/* If this is a readonly access, we don't increment access semaphore's counter. See
					 * SET_GTM_SOP_ARRAY definition in gtm_semutils.h and how it is called from db_init().
					 */
					if (!(*sem_halted) && incr_cnt && (IS_FTOK_SEM || !reg->read_only))
					{
						/* Increase the counter semaphore. */
						save_errno = do_semop(semid, DB_COUNTER_SEM, DB_COUNTER_SEM_INCR, SEM_UNDO);
						if (save_errno == 0)
							return TRUE;
						else if (ERANGE == save_errno)
						{
							*sem_halted = TRUE;
							return TRUE;
						}
						*bypass = FALSE; /* Semaphore removed when attempting to bypass. Abort bypass. */
					} else
						return TRUE;
				}
			}
		}
		if (!ok_to_bypass || (semval <= DB_COUNTER_SEM_INCR))
		{	/* Do not bypass. (We are not LKE/DSE OR) OR (There are less than 2 processes inside) */
			sop[0].sem_flg = sop[1].sem_flg = sop[2].sem_flg = SEM_UNDO; /* Enable blocking wait. */
			do
			{
				status = semop(semid, sop, sopcnt);
				save_errno = errno;
				if ((-1 == status) && (ERANGE == save_errno))
				{
					if (!(*sem_halted))
					{
						sopcnt = 2; /* ignore the increment operation */
						*sem_halted = TRUE;
						continue; /* Try again */
					}
				}
			} while ((-1 == status) && ((EINTR == save_errno) || (ERANGE == save_errno))
				 && (indefinite_wait || !*timedout));
			if (-1 != status)
				return TRUE;
			/* someone else is holding it and we are done waiting */
			sem_pid = semctl(semid, 0, GETPID);
			if (-1 != sem_pid)
				RETURN_SEMWAIT_FAILURE(retstat, 0, op_invalid_sem_syscall, ERR_SEMWT2LONG, 0, sem_pid);
		}
	} else
	{
		sop[0].sem_flg = sop[1].sem_flg = sop[2].sem_flg = SEM_UNDO; /* Enable blocking wait. */
		do
		{
			loopcnt++;
			status = semop(semid, sop, sopcnt);
			if (-1 != status)
			{
				SENDMSG_SEMOP_SUCCESS_IF_NEEDED(stacktrace_issued, semtype);
				return TRUE;
			}
			save_errno = errno;
			if ((ERANGE == save_errno) && !(*sem_halted))
			{
				sopcnt = 2;	/* ignore the increment operation */
				*sem_halted = TRUE;
				loopcnt--;	/* do not count this attempt */
				continue;	/* retry semop */
			} else if (EINTR != save_errno)
				break;
			sem_pid = semctl(semid, 0, GETPID);
			if (-1 != sem_pid)
			{
				if (!indefinite_wait && (*stacktrace_time || *timedout))
				{
					if (*stacktrace_time)
						*stacktrace_time = FALSE;	/* Prevent re-issuing midpoint trace */
					stuck_cnt++;
					if (IS_FTOK_SEM)
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
				continue;
			}
			save_errno = errno;	/* for the failed semctl, fall-through */
			break;

		} while (indefinite_wait || !*timedout);
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
			} else if ((!*sem_halted) && (ERANGE == save_errno))
			{
				*sem_halted = TRUE;
				RETURN_SEMWAIT_FAILURE(retstat, 0, op_invalid_sem_syscall, ERR_SEMWT2LONG, 0, sem_pid);
			}
			/* else some other error occurred; fall-through */
		}
		/* else some other error occurred; fall-through */
	}
	assert(0 != save_errno);
	RETURN_SEMWAIT_FAILURE(retstat, save_errno, op_semctl_or_semop, ERR_CRITSEMFAIL, 0, 0);
}
