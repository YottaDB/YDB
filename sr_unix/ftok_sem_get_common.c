/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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

#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <errno.h>

#include "gtm_sem.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtm_c_stack_trace.h"
#include "eintr_wrapper_semop.h"
#include "eintr_wrappers.h"
#include "mu_rndwn_file.h"
#include "error.h"
#include "io.h"
#include "gt_timer.h"
#include "iosp.h"
#include "gtmio.h"
#include "gtmimagename.h"
#include "do_semop.h"
#include "ipcrmid.h"
#include "gtmmsg.h"
#include "util.h"
#include "semwt2long_handler.h"
#include "repl_sem.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "gtm_semutils.h"
#include "ftok_sems.h"
#include "wbox_test_init.h"

GBLREF	gd_region		*ftok_sem_reg;

error_def(ERR_CRITSEMFAIL);
error_def(ERR_FTOKERR);
error_def(ERR_MAXSEMGETRETRY);
error_def(ERR_SEMWT2LONG);

#define	MAX_SEM_DSE_WT	(MILLISECS_IN_SEC * (30 / 2)) /* Actually 30 seconds before giving up - two semops with 15 second */
#define	MAX_SEM_WT	(MILLISECS_IN_SEC * (60 / 2)) /* Actually 60 seconds before giving up - two semops with 30 second */

/* If running in-house we want to debug live semop hangs. So, we will be continuing to hang until we get a successful semop with
 * stack traces taken every MAX_SEM_DSE_WT/MAX_SEM_WT seconds.
 */
#define MAX_SEMOP_TRYCNT	2	/* effective wait time - 30 seconds for DSE and 1 minute for other images */
#define MAX_SEMOP_DBG_TRYCNT	604800	/* effective wait time - 3.5 days for DSE and 1 week for other images */

#define	OLD_VERSION_SEM_PER_SET 2

#define ISSUE_CRITSEMFAIL_AND_RETURN(REG, FAILED_OP, ERRNO)									\
{																\
	gtm_putmsg_csa(CSA_ARG(REG2CSA(REG)) VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(REG));					\
	gtm_putmsg_csa(CSA_ARG(REG2CSA(REG)) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL(FAILED_OP), CALLFROM, ERRNO);	\
	return FALSE;														\
}

#define CANCEL_TIMER_AND_RETURN_SUCCESS(REG)										\
{															\
	cancel_timer((TID)semwt2long_handler);										\
	RETURN_SUCCESS(REG);												\
}

#define RETURN_SUCCESS(REG)												\
{															\
	ftok_sem_reg = REG;												\
	udi->grabbed_ftok_sem = TRUE;											\
	return TRUE;													\
}

boolean_t ftok_sem_get_common(gd_region *reg, boolean_t incr_cnt, int project_id, boolean_t immediate, boolean_t *stacktrace_time,
				boolean_t *timedout, semwait_status_t *retstat, boolean_t *bypass, boolean_t *ftok_counter_halted)
{
	int			status = SS_NORMAL, save_errno;
	int			ftok_sopcnt, sem_pid;
	uint4			lcnt, loopcnt;
	unix_db_info		*udi;
	union semun		semarg;
	sgmnt_addrs             *csa;
	node_local_ptr_t        cnl;
	boolean_t		shared_mem_available;
	int4			lcl_ftok_ops_index;
	key_t			ftokid;
	struct sembuf		ftok_sop[3];
	char			*msgstr;

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	udi = FILE_INFO(reg);
	assert(!udi->grabbed_ftok_sem && !udi->grabbed_access_sem);
	assert(NULL == ftok_sem_reg);
	if (-1 == (udi->key = FTOK(udi->fn, project_id)))
		RETURN_SEMWAIT_FAILURE(retstat, errno, op_ftok, 0, ERR_FTOKERR, 0);
	/* First try is always IPC_NOWAIT */
	SET_GTM_SOP_ARRAY(ftok_sop, ftok_sopcnt, incr_cnt, (SEM_UNDO | IPC_NOWAIT));
	/* The following loop deals with the possibility that the semaphores can be deleted by someone else AFTER a successful
	 * semget but BEFORE semop locks it, in which case we should retry.
	 */
	*ftok_counter_halted = FALSE;
	for (lcnt = 0; MAX_SEMGET_RETRIES > lcnt; lcnt++)
	{
		if (INVALID_SEMID == (udi->ftok_semid = semget(udi->key, FTOK_SEM_PER_ID, RWDALL | IPC_CREAT)))
		{
			save_errno = errno;
			RETURN_SEMWAIT_FAILURE(retstat, save_errno, op_semget, 0, ERR_CRITSEMFAIL, 0);
		}
		ftokid = udi->ftok_semid;
		SET_GTM_ID_SEM(ftokid, status); /* Set 3rd semaphore's value to GTM_ID = 43 */
		if (-1 == status)
		{
			save_errno = errno;
			if (SEM_REMOVED(save_errno))
			{	/* start afresh for next iteration of for loop with new semid and initial operations */
				*ftok_counter_halted = FALSE;
				SET_GTM_SOP_ARRAY(ftok_sop, ftok_sopcnt, incr_cnt, (SEM_UNDO | IPC_NOWAIT));
				continue;
			}
			RETURN_SEMWAIT_FAILURE(retstat, save_errno, op_semctl, 0, ERR_CRITSEMFAIL, 0);
		}
		/* First try is always non-blocking */
		SEMOP(ftokid, ftok_sop, ftok_sopcnt, status, NO_WAIT);
		if (-1 != status)
		{
			udi->counter_ftok_incremented = (FTOK_SOPCNT_NO_INCR_COUNTER != ftok_sopcnt);
			/* Input parameter *bypass could be OK_TO_BYPASS_FALSE or OK_TO_BYPASS_TRUE (for "do_blocking_semop" call).
			 * But if we are returning without going that path, reset "*bypass" to reflect no bypass happened.
			 */
			*bypass = FALSE;
			RETURN_SUCCESS(reg);
		}
		save_errno = errno;
		assert(EINTR != save_errno);
		if (ERANGE == save_errno)
		{	/* We have no access to file header to check so just assume qdbrundown is set in the file header.
			 * If it turns out to be FALSE, after we read the file header, we will issue an error
			 */
			assert(!*ftok_counter_halted);
			*ftok_counter_halted = TRUE;
			ftok_sopcnt = FTOK_SOPCNT_NO_INCR_COUNTER; /* Ignore increment operation */
			lcnt--; /* Do not count this attempt */
			continue;
		}
		if (immediate)
			RETURN_SEMWAIT_FAILURE(retstat, save_errno, op_semop, 0, ERR_CRITSEMFAIL, 0);
		if (EAGAIN == save_errno)
		{	/* someone else is holding it */
			if (NO_SEMWAIT_ON_EAGAIN == TREF(dbinit_max_delta_secs))
			{
				sem_pid = semctl(ftokid, DB_CONTROL_SEM, GETPID);
				if (-1 != sem_pid)
					RETURN_SEMWAIT_FAILURE(retstat, 0, op_invalid_sem_syscall, ERR_SEMWT2LONG, 0, sem_pid);
				save_errno = errno; /* fall-through */
			} else if (do_blocking_semop(ftokid, gtm_ftok_sem, stacktrace_time, timedout, retstat, reg, bypass,
						     ftok_counter_halted, incr_cnt))
			{	/* ftok_counter_halted and bypass set by "do_blocking_semop" */
				udi->counter_ftok_incremented = incr_cnt && !(*ftok_counter_halted);
				if (*bypass)
					return TRUE;
				else
					RETURN_SUCCESS(reg);
			} else if (!SEM_REMOVED(retstat->save_errno))
				return FALSE; /* retstat will already have the necessary error information */
			save_errno = retstat->save_errno; /* some other error. Fall-through */
		}
		if (SEM_REMOVED(save_errno))
		{	/* start afresh for next iteration of for loop with new semid and ftok_sopcnt */
			*ftok_counter_halted = FALSE;
			SET_GTM_SOP_ARRAY(ftok_sop, ftok_sopcnt, incr_cnt, (SEM_UNDO | IPC_NOWAIT));
			continue;
		}
		assert(EINTR != save_errno);
		RETURN_SEMWAIT_FAILURE(retstat, save_errno, op_semctl_or_semop, 0, ERR_CRITSEMFAIL, 0);
	}
	assert(FALSE);
	RETURN_SEMWAIT_FAILURE(retstat, 0, op_invalid_sem_syscall, 0, ERR_MAXSEMGETRETRY, 0);
}
