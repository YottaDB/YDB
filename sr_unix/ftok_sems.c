/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h> /* for kill(), SIGTERM, SIGQUIT */

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

GBLREF	gd_region		*gv_cur_region;
GBLREF	uint4			process_id;
GBLREF	gd_region		*ftok_sem_reg;
GBLREF	int4			exi_condition;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	recvpool_addrs		recvpool;
GBLREF	jnl_gbls_t		jgbl;
DEBUG_ONLY(GBLREF boolean_t	mupip_jnl_recover;)

error_def(ERR_CRITSEMFAIL);
error_def(ERR_FTOKERR);
error_def(ERR_MAXSEMGETRETRY);
error_def(ERR_SEMKEYINUSE);
error_def(ERR_SEMWT2LONG);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

#define	MAX_SEM_DSE_WT	(MILLISECS_IN_SEC * (30 / 2)) /* Actually 30 seconds before giving up - two semops with 15 second */
#define	MAX_SEM_WT	(MILLISECS_IN_SEC * (60 / 2)) /* Actually 60 seconds before giving up - two semops with 30 second */

/* If running in-house we want to debug live semop hangs. So, we will be continuing to hang until we get a successful semop with
 * stack traces taken every MAX_SEM_DSE_WT/MAX_SEM_WT seconds.
 */
#define MAX_SEMOP_TRYCNT	2	/* effective wait time - 30 seconds for DSE and 1 minute for other images */
#define MAX_SEMOP_DBG_TRYCNT	604800	/* effective wait time - 3.5 days for DSE and 1 week for other images */

#define	OLD_VERSION_SEM_PER_SET 2

#define ISSUE_CRITSEMFAIL_AND_RETURN(REG, FAILED_OP, ERRNO)								\
{															\
	gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(REG));							\
	gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL(FAILED_OP), CALLFROM, ERRNO);				\
	return FALSE;													\
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

boolean_t ftok_sem_get2(gd_region *reg, uint4 start_hrtbt_cntr, semwait_status_t *retstat, boolean_t *bypass)
{
	int			status = SS_NORMAL, save_errno;
	int			ftok_sopcnt, sem_pid;
	uint4			lcnt, loopcnt;
	unix_db_info		*udi;
	key_t			ftokid;
	struct sembuf		ftok_sop[3];

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	udi = FILE_INFO(reg);
	assert(!udi->grabbed_ftok_sem && !udi->grabbed_access_sem);
	assert(NULL == ftok_sem_reg);
	if (-1 == (udi->key = FTOK(udi->fn, GTM_ID)))
		RETURN_SEMWAIT_FAILURE(retstat, errno, op_ftok, 0, ERR_FTOKERR, 0);
	/* The following loop deals with the possibility that the semaphores can be deleted by someone else AFTER a successful
	 * semget but BEFORE semop locks it, in which case we should retry.
	 */
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
				continue;
			RETURN_SEMWAIT_FAILURE(retstat, save_errno, op_semctl, 0, ERR_CRITSEMFAIL, 0);
		}
		SET_GTM_SOP_ARRAY(ftok_sop, ftok_sopcnt, TRUE, (SEM_UNDO | IPC_NOWAIT)); /* First try is always IPC_NOWAIT */
		SEMOP(ftokid, ftok_sop, ftok_sopcnt, status, NO_WAIT);
		if (-1 != status)
		{
			udi->counter_ftok_incremented = TRUE;
			RETURN_SUCCESS(reg);
		}
		assert(EINTR != errno);
		save_errno = errno;
		if (EAGAIN == save_errno)
		{	/* someone else is holding it */
			if (NO_SEMWAIT_ON_EAGAIN == TREF(dbinit_max_hrtbt_delta))
			{
				sem_pid = semctl(ftokid, 0, GETPID);
				if (-1 != sem_pid)
					RETURN_SEMWAIT_FAILURE(retstat, 0, op_invalid_sem_syscall, ERR_SEMWT2LONG, 0, sem_pid);
				save_errno = errno; /* fall-through */
			} else if (do_blocking_semop(ftokid, gtm_ftok_sem, start_hrtbt_cntr, retstat, reg, bypass))
			{
				if (*bypass)
				{
					udi->counter_ftok_incremented = TRUE;
					return TRUE;
				} else
					RETURN_SUCCESS(reg);
			} else if (!SEM_REMOVED(retstat->save_errno))
				return FALSE; /* retstat will already have the necessary error information */
			save_errno = retstat->save_errno; /* some other error. Fall-through */
		}
		if (SEM_REMOVED(save_errno))
			continue;
		assert(EINTR != save_errno);
		RETURN_SEMWAIT_FAILURE(retstat, save_errno, op_semctl_or_semop, 0, ERR_CRITSEMFAIL, 0);
	}
	assert(FALSE);
	RETURN_SEMWAIT_FAILURE(retstat, 0, op_invalid_sem_syscall, 0, ERR_MAXSEMGETRETRY, 0);
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
boolean_t ftok_sem_get(gd_region *reg, boolean_t incr_cnt, int project_id, boolean_t immediate)
{
	int			sem_pid, save_errno, ftok_sopcnt, stuck_cnt = 0;
	int4			status;
	uint4			semop_wait_time, lcnt, semop_trycnt, max_semop_trycnt, tot_wait_time;
	unix_db_info		*udi;
	union semun		semarg;
	sgmnt_addrs             *csa;
	node_local_ptr_t        cnl;
	boolean_t		shared_mem_available;
	int4			lcl_ftok_ops_index;
	struct sembuf		ftok_sop[3];
	char			*msgstr;
	boolean_t		stacktrace_issued = FALSE;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(reg);
	/* The ftok semaphore should never be requested on the replication instance file while already holding the
	 * journal pool access semaphore as it can lead to deadlocks (the right order is get ftok semaphore first
	 * and then get the access semaphore). The only exception is MUPIP ROLLBACK due to an issue that is documented
	 * in C9F10-002759. Assert that below.
	 */
	assert((reg != jnlpool.jnlpool_dummy_reg) || jgbl.mur_rollback || !holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	assert(!udi->grabbed_ftok_sem && !udi->grabbed_access_sem);
	assert(NULL == ftok_sem_reg);
	if (-1 == (udi->key = FTOK(udi->fn, project_id)))
	{
		gtm_putmsg(VARLSTCNT(5) ERR_FTOKERR, 2, DB_LEN_STR(reg), errno);
		return FALSE;
	}
	/* The following loop deals with the possibility that the semaphores can be deleted by someone else AFTER a successful
	 * semget but BEFORE semop locks it, in which case we should retry.
	 */
	for (lcnt = 0; MAX_SEMGET_RETRIES > lcnt; lcnt++)
	{
		if (-1 == (udi->ftok_semid = semget(udi->key, FTOK_SEM_PER_ID, RWDALL | IPC_CREAT)))
		{
			udi->ftok_semid = INVALID_SEMID;
			save_errno = errno;
			if (EINVAL == save_errno)
			{
				/* Possibly the key is in use by older GTM version */
				if (-1 != semget(udi->key, OLD_VERSION_SEM_PER_SET, RALL))
					gtm_putmsg(VARLSTCNT(4) ERR_SEMKEYINUSE, 1, udi->key, errno);
			}
			ISSUE_CRITSEMFAIL_AND_RETURN(reg, "semget()", save_errno);
		}
		SET_GTM_ID_SEM(udi->ftok_semid, status); /* sets 3rd semaphore's value to GTM_ID = 43 */
		if (-1 == status)
		{
			save_errno = errno;
			ISSUE_CRITSEMFAIL_AND_RETURN(reg, "semctl()", save_errno);
		}
		SET_GTM_SOP_ARRAY(ftok_sop, ftok_sopcnt, incr_cnt, (SEM_UNDO | IPC_NOWAIT));
		assert(mupip_jnl_recover || incr_cnt);
		/* First try is always non-blocking */
		SEMOP(udi->ftok_semid, ftok_sop, ftok_sopcnt, status, NO_WAIT);
		if (-1 != status)
		{
			SENDMSG_SEMOP_SUCCESS_IF_NEEDED(stacktrace_issued, gtm_ftok_sem);
			udi->counter_ftok_incremented = incr_cnt;
			RETURN_SUCCESS(reg);
		}
		save_errno = errno;
		if (immediate)
			ISSUE_CRITSEMFAIL_AND_RETURN(reg, "semop()", save_errno);
		if (EAGAIN == save_errno)
		{	/* Someone else is holding it */
			sem_pid = semctl(udi->ftok_semid, DB_CONTROL_SEM, GETPID);
			if (-1 != sem_pid)
			{
				ftok_sop[0].sem_flg = ftok_sop[1].sem_flg = ftok_sop[2].sem_flg = SEM_UNDO; /* blocking calls */
				semop_wait_time = !IS_DSE_IMAGE ? MAX_SEM_WT : MAX_SEM_DSE_WT;
				max_semop_trycnt = !(TREF(gtm_environment_init)) ? MAX_SEMOP_TRYCNT : MAX_SEMOP_DBG_TRYCNT;
				for (semop_trycnt = 0; max_semop_trycnt > semop_trycnt; ++semop_trycnt)
				{
					TREF(semwait2long) = FALSE;
					start_timer((TID)semwt2long_handler, semop_wait_time, semwt2long_handler, 0, NULL);
					do
					{
						status = semop(udi->ftok_semid, ftok_sop, ftok_sopcnt); /* blocking semop */
					} while ((-1 == status) && (EINTR == errno) && !(TREF(semwait2long)));
					if (-1 != status) /* success ? */
					{
						SENDMSG_SEMOP_SUCCESS_IF_NEEDED(stacktrace_issued, gtm_ftok_sem);
						udi->counter_ftok_incremented = incr_cnt;
						CANCEL_TIMER_AND_RETURN_SUCCESS(reg);
					}
					save_errno = errno;
					if (EINTR == save_errno)
					{	/* Timer popped. If not, we would have continued in the do..while loop */
						assert(TREF(semwait2long));
						sem_pid = semctl(udi->ftok_semid, DB_CONTROL_SEM, GETPID);
						if (-1 != sem_pid)
						{
							stuck_cnt++;
							msgstr = (1 == stuck_cnt) ? "SEMWT2LONG_FTOK_INFO" : "SEMWT2LONG_FTOK";
							if ((0 != sem_pid) && (sem_pid != process_id))
							{
								GET_C_STACK_FROM_SCRIPT(msgstr, process_id, sem_pid, stuck_cnt);
								if (TREF(gtm_environment_init))
									stacktrace_issued = TRUE;
							}
							continue;
						} else
							save_errno = errno; /* for the failed semctl */
					}
					cancel_timer((TID)semwt2long_handler);
					break; /* semop/semctl failed for some other reason (for instance, EIDRM/EINVAL) */
				}
				if (max_semop_trycnt <= semop_trycnt)
				{	/* we exhausted maximum attempts to do blocking semop. Issue SEMWT2LONG error and return */
					assert(-1 != sem_pid);
					tot_wait_time = (semop_wait_time * max_semop_trycnt) / MILLISECS_IN_SEC;
					gtm_putmsg(VARLSTCNT(9) ERR_SEMWT2LONG, 7, process_id, tot_wait_time, LEN_AND_LIT("ftok"),
							DB_LEN_STR(reg), sem_pid);
					return FALSE;
				}
				/* fall-through */
			} else
				save_errno = errno; /* for the failed semctl */
			/* fall-through */
		}
		assert(0 != save_errno);
		if (SEM_REMOVED(save_errno))
			continue;
		ISSUE_CRITSEMFAIL_AND_RETURN(reg, "semop()/semctl()", save_errno);
	} /* end for loop */
	assert(-1 == status);
	assert(MAX_SEMGET_RETRIES < lcnt);
	assert(FALSE);
	gtm_putmsg(VARLSTCNT(8) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
			ERR_TEXT, 2, RTS_ERROR_LITERAL("failed to obtain ftok semaphore after maximum retries"));
	return FALSE;
}

/*
 * Description:
 * 	Assumes that ftok semaphore already exists. Just lock it.
 * Parameters:
 *	reg		: Regions structure
 * 	incr_cnt	: IF incr_cnt == TRUE, it will increment counter semaphore.
 * 	immediate	: IF immediate == TRUE, it will use IPC_NOWAIT flag.
 * Return Value: TRUE, if succsessful
 *	         FALSE, if fails.
 */
boolean_t ftok_sem_lock(gd_region *reg, boolean_t incr_cnt, boolean_t immediate)
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
	 * and then get the access semaphore). The only exception is MUPIP ROLLBACK due to an issue that is documented
	 * in C9F10-002759. Assert that below.
	 */
	assert((reg != jnlpool.jnlpool_dummy_reg) || jgbl.mur_rollback || !holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	assert(!csa->now_crit);
	/* The following two asserts are to ensure we never hold more than one FTOK semaphore at any point in time.  The only
	 * exception is if we were MUPIP STOPped (or kill -3ed) while having ftok_sem lock on one region and we came to rundown code
	 * that invoked ftok_sem_lock() on a different region. Hence the process_exiting check below.  In the pro version, we will
	 * do the right thing by returning TRUE right away if udi->grabbed_ftok_sem is TRUE. This is
	 * because incr_cnt is FALSE always (asserted below too).
	 */
	assert(!udi->grabbed_ftok_sem || (FALSE != process_exiting));
	assert((NULL == ftok_sem_reg) || (FALSE != process_exiting));
	assert(!incr_cnt);
	assert(INVALID_SEMID != udi->ftok_semid);
	ftok_sopcnt = 0;
	if (!udi->grabbed_ftok_sem)
	{	/* Guarantee no one else accesses database file header while we update semid/shmid fields in the file header */
		ftok_sop[0].sem_num = DB_CONTROL_SEM; ftok_sop[0].sem_op = 0;	/* Wait for 0 (unlocked) */
		ftok_sop[1].sem_num = DB_CONTROL_SEM; ftok_sop[1].sem_op = 1;	/* Then lock it */
		ftok_sopcnt = 2;
	} else if (!incr_cnt)
		return TRUE;
	if (incr_cnt)
	{
		ftok_sop[ftok_sopcnt].sem_num = DB_COUNTER_SEM; ftok_sop[ftok_sopcnt].sem_op = 1; /* increment counter */
		ftok_sopcnt++;
	}
	ftok_sop[0].sem_flg = ftok_sop[1].sem_flg = ftok_sop[2].sem_flg = SEM_UNDO | IPC_NOWAIT;
	SEMOP(udi->ftok_semid, ftok_sop, ftok_sopcnt, status, NO_WAIT);
	if (-1 == status)	/* We couldn't get it in one shot -- see if we already have it */
	{
		save_errno = errno;
		if (EAGAIN == save_errno)
		{
			assert(process_id != semctl(udi->ftok_semid, 0, GETPID)); /* ensure that we don't hold the ftok semaphore */
			if(immediate)
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
	udi->counter_ftok_incremented = TRUE;
	RETURN_SUCCESS(reg);
}

/*
 * Description:
 * 	Assumes that ftok semaphore id already exists. Increment only the COUNTER SEMAPHORE in that semaphore set.
 * Parameters:
 *	reg		: Regions structure
 * Return Value: TRUE, if succsessful
 *               FALSE, if fails.
 */
boolean_t ftok_sem_incrcnt(gd_region *reg)
{
	int			save_errno, status;
	unix_db_info		*udi;
	sgmnt_addrs		*csa;
	struct sembuf		ftok_sop;

	assert(NULL != reg);
	assert(NULL == ftok_sem_reg);	/* assert that we never hold more than one FTOK semaphore at any point in time */
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	assert(!csa->now_crit);
	assert(INVALID_SEMID != udi->ftok_semid);
	ftok_sop.sem_num = DB_COUNTER_SEM;
	ftok_sop.sem_op = 1; /* increment counter */
	ftok_sop.sem_flg = SEM_UNDO;
	SEMOP(udi->ftok_semid, (&ftok_sop), 1, status, NO_WAIT);
	if (-1 == status)	/* We couldn't get it in one shot -- see if we already have it */
	{
		save_errno = errno;
		ISSUE_CRITSEMFAIL_AND_RETURN(reg, "semop()", save_errno);
	}
	udi->counter_ftok_incremented = TRUE;
	return TRUE;
}

/*
 * Description:
 * 	Assumes that ftok semaphore was already locked. Now release it.
 * Parameters:
 *	reg		: Regions structure
 * 	IF decr_cnt == TRUE, it will decrement counter semaphore.
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
	/* The following assert is to ensure we never hold more than one FTOK semaphore at any point in time.
	 * The only exception is if we were MUPIP STOPped (or kill -3ed) while having ftok_sem lock on one region and we
	 * 	came to rundown code that invoked ftok_sem_lock() on a different region. Hence the process_exiting check below.
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
		if (-1 == (ftok_semval = semctl(udi->ftok_semid, DB_COUNTER_SEM, GETVAL)))
		{
			save_errno = errno;
			GTM_SEM_CHECK_EINVAL(TREF(gtm_environment_init), save_errno, udi);
			ISSUE_CRITSEMFAIL_AND_RETURN(reg, "semop()", save_errno);
		}
		if (1 >= ftok_semval)	/* checking against 0, in case already we decremented semaphore number 1 */
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
		if (0 != (save_errno = do_semop(udi->ftok_semid, DB_COUNTER_SEM, -1, semflag)))
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
