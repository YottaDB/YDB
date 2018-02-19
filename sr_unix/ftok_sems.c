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

GBLREF	uint4			process_id;
GBLREF	gd_region		*ftok_sem_reg;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	jnl_gbls_t		jgbl;

error_def(ERR_CRITSEMFAIL);
error_def(ERR_FTOKERR);
error_def(ERR_MAXSEMGETRETRY);
error_def(ERR_SEMKEYINUSE);
error_def(ERR_SEMWT2LONG);
error_def(ERR_SYSCALL);

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
MBSTART {														\
	ftok_sem_reg = REG;												\
	udi->grabbed_ftok_sem = TRUE;											\
	return TRUE;													\
} MBEND

boolean_t ftok_sem_get2(gd_region *reg, boolean_t *stacktrace_time, boolean_t *timedout, semwait_status_t *retstat,
			boolean_t *bypass, boolean_t *ftok_counter_halted, boolean_t incr_cnt)
{
	boolean_t	immediate = FALSE;
	int		project_id = GTM_ID;

	return ftok_sem_get_common(reg, incr_cnt, project_id, immediate, stacktrace_time, timedout, retstat, bypass,
					ftok_counter_halted);
}

/*
 * Description:
 * 	Using project_id this will find FTOK of FILE_INFO(reg)->fn.
 * 	Create semaphore set of id "ftok_semid" using that project_id, if it does not exist.
 * 	Then it will lock ftok_semid.
 * Parameters:
 *	reg		: Regions structure
 * 	incr_cnt	: IF incr_cnt == TRUE, it will increment counter semaphore.
 *	project_id	: Project id for ftok call.
 * 	immediate	: IF immediate == TRUE, it will use IPC_NOWAIT flag.
 * Return Value: TRUE, if succsessful
 *	         FALSE, if fails.
 */
boolean_t ftok_sem_get(gd_region *reg, boolean_t incr_cnt, int project_id, boolean_t immediate, boolean_t *ftok_counter_halted)
{
	uint4			semop_wait_time;
	unix_db_info		*udi;
	boolean_t		stacktrace_time = FALSE, sem_timeout, bypass = FALSE, result;
	semwait_status_t	retstat;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(reg);
	*ftok_counter_halted = FALSE;
	/* The ftok semaphore should never be requested on the replication instance file while already holding the
	 * journal pool access semaphore as it can lead to deadlocks (the right order is get ftok semaphore first
	 * and then get the access semaphore). The only exception is MUPIP JOURNAL -ROLLBACK -BACKWARD due to an issue
	 * that is documented in C9F10-002759. Assert that below.
	 */
	assert(((NULL == jnlpool) || (reg != jnlpool->jnlpool_dummy_reg))
		|| (jgbl.mur_rollback && !jgbl.mur_options_forward) || !holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	udi = FILE_INFO(reg);
	assert(!udi->grabbed_ftok_sem && !udi->grabbed_access_sem);
	assert(NULL == ftok_sem_reg);
	semop_wait_time = !IS_DSE_IMAGE ? MAX_SEM_WT : MAX_SEM_DSE_WT;
	TIMEOUT_INIT(sem_timeout, semop_wait_time);
	result = ftok_sem_get_common(reg, incr_cnt, project_id, immediate, &stacktrace_time, &sem_timeout, &retstat, &bypass,
						ftok_counter_halted);
	TIMEOUT_DONE(sem_timeout);
	assert(!bypass);
	if (!result)
	{
		PRINT_SEMWAIT_ERROR(&retstat, reg, udi, "ftok");
		return FALSE;
	}
	else
		RETURN_SUCCESS(reg);
}

/*
 * Description:
 * 	Assumes that ftok semaphore already exists. Just lock it.
 * Parameters:
 *	reg		: Regions structure
 * 	immediate	: IF immediate == TRUE, it will use IPC_NOWAIT flag.
 * Return Value: TRUE, if succsessful
 *	         FALSE, if fails.
 */
boolean_t ftok_sem_lock(gd_region *reg, boolean_t immediate)
{
	int			semflag, save_errno, status;
	unix_db_info		*udi;
	sgmnt_addrs		*csa;
	struct sembuf		ftok_sop[3];
	int			ftok_sopcnt;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(reg);
	/* The ftok semaphore should never be requested on the replication instance file while already holding the
	 * journal pool access semaphore as it can lead to deadlocks (the right order is get ftok semaphore first
	 * and then get the access semaphore). The only exception is MUPIP JOURNAL -ROLLBACK -BACKWARD due to an issue
	 * that is documented in C9F10-002759. Assert that below.
	 */
	assert(((NULL == jnlpool) || (reg != jnlpool->jnlpool_dummy_reg))
		|| (jgbl.mur_rollback && !jgbl.mur_options_forward) || !holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	/* Requests for the ftok lock on a db should always comes before requests for crit on the same db.
	 * This is needed to avoid deadlocks. So we should never hold crit on this db while requesting the ftok lock. Assert that.
	 */
	assert(!csa->now_crit);
	/* The following two asserts are to ensure we never hold more than one FTOK semaphore at any point in time.  The only
	 * exception is if we were MUPIP STOPped (or kill -3ed) while having ftok_sem lock on one region and we came to rundown code
	 * that invoked ftok_sem_lock() on a different region. Hence the process_exiting check below.  In the pro version, we will
	 * do the right thing by returning TRUE right away if udi->grabbed_ftok_sem is TRUE.
	 */
	assert(!udi->grabbed_ftok_sem || (FALSE != process_exiting));
	assert((NULL == ftok_sem_reg) || (FALSE != process_exiting));
	assert(INVALID_SEMID != udi->ftok_semid);
	ftok_sopcnt = 0;
	if (!udi->grabbed_ftok_sem)
	{	/* Guarantee no one else accesses database file header while we update semid/shmid fields in the file header */
		ftok_sop[0].sem_num = DB_CONTROL_SEM; ftok_sop[0].sem_op = 0;	/* Wait for 0 (unlocked) */
		ftok_sop[1].sem_num = DB_CONTROL_SEM; ftok_sop[1].sem_op = 1;	/* Then lock it */
		ftok_sopcnt = FTOK_SOPCNT_NO_INCR_COUNTER;
	} else
		return TRUE;
	ftok_sop[0].sem_flg = ftok_sop[1].sem_flg = ftok_sop[2].sem_flg = SEM_UNDO | IPC_NOWAIT;
	SEMOP(udi->ftok_semid, ftok_sop, ftok_sopcnt, status, NO_WAIT);
	if (-1 == status)	/* We couldn't get it in one shot -- see if we already have it */
	{
		save_errno = errno;
		if (EAGAIN == save_errno)
		{
			assert(process_id != semctl(udi->ftok_semid, 0, GETPID)); /* ensure that we don't hold the ftok semaphore */
			if (immediate)
			{	/* Only db_ipcs_reset passes immediate=TRUE for ftok_sem_lock. If we couldn't get the lock, return
				 * FALSE without doing a gtm_putmsg as the process that does hold the lock will release it.
				 */
				return FALSE;
			}
			ftok_sop[0].sem_flg = ftok_sop[1].sem_flg = ftok_sop[2].sem_flg = SEM_UNDO;
			SEMOP(udi->ftok_semid, ftok_sop, ftok_sopcnt, status, FORCED_WAIT)
			if (-1 == status)			/* We couldn't get it at all.. */
			{
				save_errno = errno;
				GTM_SEM_CHECK_EINVAL(TREF(gtm_environment_init), save_errno, udi);
				ISSUE_CRITSEMFAIL_AND_RETURN(reg, "semop()/semctl()", save_errno);
			}
		} else
		{
			GTM_SEM_CHECK_EINVAL(TREF(gtm_environment_init), save_errno, udi);
			ISSUE_CRITSEMFAIL_AND_RETURN(reg, "semop()", save_errno);
		}
	}
	udi->grabbed_ftok_sem = TRUE;
	RETURN_SUCCESS(reg);
}

/*
 * Description:
 * 	Assumes that ftok semaphore was already locked. Now release it.
 * Parameters:
 *	reg		: Regions structure
 * 	IF decr_cnt == DECR_CNT_TRUE (1), it will decrement counter semaphore & remove it if needed.
 * 	IF decr_cnt == DECR_CNT_SAFE (2), it will decrement counter semaphore but not remove the semaphore.
 * 	IF immediate == TRUE, it will use IPC_NOWAIT flag.
 * Return Value: TRUE, if succsessful
 *	         FALSE, if fails.
 * NOTE: The parameter "immediate" may not be necessary. Here we remove the semaphore
 * 	 or decrement the counter. We are already holding the control semaphore.
 *	 So never we need to pass IPC_NOWAIT. But we need to analyze before we change code.
 */
boolean_t ftok_sem_release(gd_region *reg,  boolean_t decr_cnt, boolean_t immediate)
{
	int		ftok_semval, semflag, save_errno;
	unix_db_info 	*udi;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(NULL != reg);
	assert(DECR_CNT_SAFE != DECR_CNT_TRUE);
	assert(DECR_CNT_SAFE != DECR_CNT_FALSE);
	/* The following assert is to ensure we never hold more than one FTOK semaphore at any point in time.  The only exception is
	 * if we were MUPIP STOPped (or kill -3ed) while having ftok_sem lock on one region and we came to rundown code that invoked
	 * ftok_sem_lock() on a different region. Hence the process_exiting check below.
	 */
	assert(reg == ftok_sem_reg || (FALSE != process_exiting));
	udi = FILE_INFO(reg);
	assert(udi->grabbed_ftok_sem);
	assert(udi && INVALID_SEMID != udi->ftok_semid);
	/* if we dont have the ftok semaphore, return true even if decr_cnt was requested */
	if (!udi->grabbed_ftok_sem)
		return TRUE;
	semflag = SEM_UNDO | (immediate ? IPC_NOWAIT : 0);
	if (decr_cnt)
	{
		assert(udi->counter_ftok_incremented);
		if (DECR_CNT_SAFE != decr_cnt)
		{
			if (-1 == (ftok_semval = semctl(udi->ftok_semid, DB_COUNTER_SEM, GETVAL)))
			{
				save_errno = errno;
				GTM_SEM_CHECK_EINVAL(TREF(gtm_environment_init), save_errno, udi);
				ISSUE_CRITSEMFAIL_AND_RETURN(reg, "semop()", save_errno);
			}
			/* Below checks against 0, in case already we decremented semaphore number 1 */
			if (DB_COUNTER_SEM_INCR >= ftok_semval)
			{
				if (0 != sem_rmid(udi->ftok_semid))
				{
					save_errno = errno;
					GTM_SEM_CHECK_EINVAL(TREF(gtm_environment_init), save_errno, udi);
					ISSUE_CRITSEMFAIL_AND_RETURN(reg, "sem_rmid()", save_errno);
				}
				udi->ftok_semid = INVALID_SEMID;
				ftok_sem_reg = NULL;
				udi->grabbed_ftok_sem = FALSE;
				udi->counter_ftok_incremented = FALSE;
				return TRUE;
			}
		}
		/* Always set IPC_NOWAIT for counter decrement. In the rare case where the counter is already zero, is it better
		 * to handle the error than it is to wait indefinitely for another process to wake us up.
		 */
		if (0 != (save_errno = do_semop(udi->ftok_semid, DB_COUNTER_SEM, -DB_COUNTER_SEM_INCR, (SEM_UNDO | IPC_NOWAIT))))
		{
			GTM_SEM_CHECK_EINVAL(TREF(gtm_environment_init), save_errno, udi);
			ISSUE_CRITSEMFAIL_AND_RETURN(reg, "semop()", save_errno);
		}
		udi->counter_ftok_incremented = FALSE;
	}
	if (0 != (save_errno = do_semop(udi->ftok_semid, DB_CONTROL_SEM, -1, semflag)))
	{
		GTM_SEM_CHECK_EINVAL(TREF(gtm_environment_init), save_errno, udi);
		ISSUE_CRITSEMFAIL_AND_RETURN(reg, "semop()", save_errno);
	}
	udi->grabbed_ftok_sem = FALSE;
	ftok_sem_reg = NULL;
	return TRUE;
}
