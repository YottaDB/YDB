/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "ftok_sems.h"
#include "semwt2long_handler.h"
#include "repl_sem.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	uint4			process_id;
GBLREF	boolean_t		sem_incremented;
GBLREF	gd_region		*ftok_sem_reg;
GBLREF	volatile boolean_t	semwt2long;
GBLREF	boolean_t		gtm_environment_init;
GBLREF	gd_region		*standalone_reg;
GBLREF	int4			exi_condition;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	recvpool_addrs		recvpool;
GBLREF	jnl_gbls_t		jgbl;

static struct sembuf		ftok_sop[3];
static int			ftok_sopcnt;

#define	MAX_SEM_DSE_WT		(1000 * 30)		/* 30 second wait to acquire access control semaphore */
#define	MAX_SEM_WT		(1000 * 60)		/* 60 second wait to acquire access control semaphore */
#define	MAX_RES_TRIES		620
#define	OLD_VERSION_SEM_PER_SET 2

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
	int		sem_wait_time, sem_pid, save_errno;
	int4		status;
	uint4		lcnt;
	unix_db_info	*udi;
	union semun	semarg;

	error_def(ERR_FTOKERR);
	error_def(ERR_CRITSEMFAIL);
	error_def(ERR_SEMWT2LONG);
	error_def(ERR_SEMKEYINUSE);
	error_def(ERR_SYSCALL);

	assert(reg);
	/* The ftok semaphore should never be requested on the replication instance file while already holding the
	 * journal pool access semaphore as it can lead to deadlocks (the right order is get ftok semaphore first
	 * and then get the access semaphore). The only exception is MUPIP ROLLBACK due to an issue that is documented
	 * in C9F10-002759. Assert that below.
	 */
	assert((reg != jnlpool.jnlpool_dummy_reg) || jgbl.mur_rollback || !holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
	udi = FILE_INFO(reg);
	assert(!udi->grabbed_ftok_sem);
	assert(NULL == ftok_sem_reg);
	if (-1 == (udi->key = FTOK(udi->fn, project_id)))
	{
		gtm_putmsg(VARLSTCNT(5) ERR_FTOKERR, 2, DB_LEN_STR(reg), errno);
		return FALSE;
	}
	/*
	 * The purpose of this loop is to deal with possibility that the semaphores might
	 * be deleted by someone else after the semget call below but before semop locks it.
	 */
	for (status = -1, lcnt = 0;  -1 == status;  lcnt++)
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
			gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semget()"), CALLFROM, save_errno);
			return FALSE;
		}
		/* Following will set semaphore number 2 ( = FTOK_SEM_PER_ID - 1)  value as GTM_ID.
		 * In case we have orphaned semaphore for some reason, mupip rundown will be
		 * able to identify GTM semaphores from the value GTM_ID and will remove.
		 */
		semarg.val = GTM_ID;
		if (-1 == semctl(udi->ftok_semid, FTOK_SEM_PER_ID - 1, SETVAL, semarg))
		{
			save_errno = errno;
			/*EIDRM seen only on Linux*/
			if (((EINVAL == save_errno) || (EIDRM == errno)) && MAX_RES_TRIES >= lcnt)
					continue;
			gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semctl()"), CALLFROM, save_errno);
			GTM_SEM_CHECK_EINVAL(gtm_environment_init, save_errno, udi);
			return FALSE;
		}
		ftok_sop[0].sem_num = 0; ftok_sop[0].sem_op = 0;	/* Wait for 0 (unlocked) */
		ftok_sop[1].sem_num = 0; ftok_sop[1].sem_op = 1;	/* Then lock it */
		if (incr_cnt)
		{
			ftok_sop[2].sem_num = 1; ftok_sop[2].sem_op = 1;	/* increment counter semaphore */
			ftok_sopcnt = 3;
		} else
			ftok_sopcnt = 2;
		assert(!IS_DSE_IMAGE || incr_cnt);
		/* First try is always non-blocking */
		ftok_sop[0].sem_flg = ftok_sop[1].sem_flg = ftok_sop[2].sem_flg = SEM_UNDO | IPC_NOWAIT;
		status = semop(udi->ftok_semid, ftok_sop, ftok_sopcnt);
		if (-1 != status)
			break;
		save_errno = errno;
		if (immediate)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5,
				RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);
			return FALSE;
		}
		if (EAGAIN == save_errno)	/* Someone else is holding it */
		{	/* see note on Linux specific difference in behaviour of semctl() with GETPID in gds_rundown_ch() */
			sem_pid = semctl(udi->ftok_semid, 0, GETPID);
			if (-1 == sem_pid)
			{
				/*EIDRM seen only on Linux*/
				if ((EINVAL == errno) || (EIDRM == errno))	/* the sem might have been deleted */
					continue;
				else
				{
					gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
					gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5,
						RTS_ERROR_LITERAL("semop() and semctl()"), CALLFROM, errno);
					return FALSE;
				}
			}
			if (!IS_DSE_IMAGE)
			{
				if (!gtm_environment_init)
					sem_wait_time = MAX_SEM_WT;
				else
					sem_wait_time = 604800; /* one week, that is, infinite wait */
			} else
			{
				sem_wait_time = MAX_SEM_DSE_WT;
				util_out_print("FTOK semaphore for region !AD is held by pid, !UL. "
					       "An attempt will be made in the next !SL seconds to grab it.",
					       TRUE, DB_LEN_STR(reg), sem_pid, sem_wait_time/1000);
			}
			semwt2long = FALSE;
			start_timer((TID)semwt2long_handler, sem_wait_time, semwt2long_handler, 0, NULL);
			/*** drop thru ***/
		} else if (((EINVAL != save_errno) && (EIDRM != save_errno) && (EINTR != save_errno)) || (MAX_RES_TRIES < lcnt))
		{
			gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);
			return FALSE;
		} else
			continue;
		/* We already started a timer. Now try semop not using IPC_NOWAIT (that is, blocking semop)*/
		ftok_sop[0].sem_flg = ftok_sop[1].sem_flg = ftok_sop[2].sem_flg = SEM_UNDO;
		status = semop(udi->ftok_semid, ftok_sop, ftok_sopcnt);
		if (-1 == status)
		{
			save_errno = errno;
			if (EINTR == save_errno && semwt2long)
			{	/* Timer popped */
				/* see note on Linux specific difference in behaviour of semctl() with GETPID in gds_rundown_ch() */
				sem_pid = semctl(udi->ftok_semid, 0, GETPID);
				if (-1 != sem_pid)
				{
					gtm_putmsg(VARLSTCNT(5) ERR_SEMWT2LONG, 3,  DB_LEN_STR(reg), sem_pid);
					return FALSE;
				} else
				{
					gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
					gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5,
						RTS_ERROR_LITERAL("semop() and semctl()"), CALLFROM, save_errno);
					return FALSE;
				}
			} else if (((EINVAL != save_errno) && (EIDRM != save_errno) && (EINTR != save_errno))
				|| (MAX_RES_TRIES < lcnt))
			{
				cancel_timer((TID)semwt2long_handler);
				gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
				gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);
				return FALSE;
			}
			/* else continue */
			cancel_timer((TID)semwt2long_handler);
		} else
		{
			cancel_timer((TID)semwt2long_handler);
			break;
		}
	} /* end for loop */
	ftok_sem_reg = reg;
	udi->grabbed_ftok_sem = TRUE;
	return TRUE;
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

	error_def(ERR_CRITSEMFAIL);
	error_def(ERR_SYSCALL);

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
	/* The following two asserts are to ensure we never hold more than one FTOK semaphore at any point in time.
	 * The only exception is if we were MUPIP STOPped (or kill -3ed) while having ftok_sem lock on one region and we
	 * 	came to rundown code that invoked ftok_sem_lock() on a different region. Hence the process_exiting check below.
	 * In the pro version, we will do the right thing by returning TRUE right away if udi->grabbed_ftok_sem is TRUE.
	 * 	This is because incr_cnt is FALSE always (asserted below too).
	 */
	assert(!udi->grabbed_ftok_sem || (FALSE != process_exiting));
	assert((NULL == ftok_sem_reg) || (FALSE != process_exiting));
	assert(!incr_cnt);
	assert(INVALID_SEMID != udi->ftok_semid);
	ftok_sopcnt = 0;
	semflag = SEM_UNDO | (immediate ? IPC_NOWAIT : 0);
	if (!udi->grabbed_ftok_sem)
	{	/* We need to gaurantee that none else access database file header
		 * when semid/shmid fields are updated in file header
		 */
		ftok_sop[0].sem_num = 0; ftok_sop[0].sem_op = 0;	/* Wait for 0 (unlocked) */
		ftok_sop[1].sem_num = 0; ftok_sop[1].sem_op = 1;	/* Then lock it */
		ftok_sopcnt = 2;
	} else if (!incr_cnt)
		return TRUE;
	if (incr_cnt)
	{
		ftok_sop[ftok_sopcnt].sem_num = 1; ftok_sop[ftok_sopcnt].sem_op = 1; /* increment counter */
		ftok_sopcnt++;
	}
	ftok_sop[0].sem_flg = ftok_sop[1].sem_flg = ftok_sop[2].sem_flg = SEM_UNDO | IPC_NOWAIT;
	SEMOP(udi->ftok_semid, ftok_sop, ftok_sopcnt, status);
	if (-1 == status)	/* We couldn't get it in one shot -- see if we already have it */
	{
		save_errno = errno;
		/* see note on Linux specific difference in behaviour of semctl() with GETPID in gds_rundown_ch() */
		if (semctl(udi->ftok_semid, 0, GETPID) == process_id)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semctl()"), CALLFROM, save_errno);
			return FALSE;
		}
		if (EAGAIN != save_errno)
		{
			gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);
			GTM_SEM_CHECK_EINVAL(gtm_environment_init, save_errno, udi);
			return FALSE;
		}
		/* Try again - IPC_NOWAIT is set TRUE, if immediate is TRUE */
		ftok_sop[0].sem_flg = ftok_sop[1].sem_flg = ftok_sop[2].sem_flg = semflag;
		SEMOP(udi->ftok_semid, ftok_sop, ftok_sopcnt, status);
		if (-1 == status)			/* We couldn't get it at all.. */
		{
			save_errno = errno;
			gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);
			GTM_SEM_CHECK_EINVAL(gtm_environment_init, save_errno, udi);
			return FALSE;
		}
	}
	udi->grabbed_ftok_sem = TRUE;
	ftok_sem_reg = reg;
	return TRUE;
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
	int			semflag, save_errno, status;
	unix_db_info		*udi;
	sgmnt_addrs		*csa;

	error_def(ERR_CRITSEMFAIL);
	error_def(ERR_SYSCALL);

	assert(NULL != reg);
	assert(NULL == ftok_sem_reg);	/* assert that we never hold more than one FTOK semaphore at any point in time */
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	assert(!csa->now_crit);
	assert(INVALID_SEMID != udi->ftok_semid);
	semflag = SEM_UNDO;
	ftok_sopcnt = 0;
	ftok_sop[ftok_sopcnt].sem_num = 1;
	ftok_sop[ftok_sopcnt].sem_op = 1; /* increment counter */
	ftok_sop[ftok_sopcnt].sem_flg = SEM_UNDO;
	ftok_sopcnt++;
	SEMOP(udi->ftok_semid, ftok_sop, ftok_sopcnt, status);
	if (-1 == status)	/* We couldn't get it in one shot -- see if we already have it */
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);
		return FALSE;
	}
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

	error_def(ERR_CRITSEMFAIL);
	error_def(ERR_SYSCALL);

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
		if (-1 == (ftok_semval = semctl(udi->ftok_semid, 1, GETVAL)))
		{
			save_errno = errno;
			gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semctl()"), CALLFROM, save_errno);
			GTM_SEM_CHECK_EINVAL(gtm_environment_init, save_errno, udi);
			return FALSE;
		}
		if (1 >= ftok_semval)	/* checking against 0, in case already we decremented semaphore number 1 */
		{
			if (0 != sem_rmid(udi->ftok_semid))
			{
				save_errno = errno;
				gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
				gtm_putmsg(VARLSTCNT(7) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("sem_rmid()"), CALLFROM);
				GTM_SEM_CHECK_EINVAL(gtm_environment_init, save_errno, udi);
				return FALSE;
			}
			udi->ftok_semid = INVALID_SEMID;
			ftok_sem_reg = NULL;
			udi->grabbed_ftok_sem = FALSE;
			return TRUE;
		}
		if (0 != (save_errno = do_semop(udi->ftok_semid, 1, -1, semflag)))
		{
			gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);
			GTM_SEM_CHECK_EINVAL(gtm_environment_init, save_errno, udi);
			return FALSE;
		}
	}
	if (0 != (save_errno = do_semop(udi->ftok_semid, 0, -1, semflag)))
	{
		gtm_putmsg(VARLSTCNT(4) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg));
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semop()"), CALLFROM, save_errno);
		GTM_SEM_CHECK_EINVAL(gtm_environment_init, save_errno, udi);
		return FALSE;
	}
	udi->grabbed_ftok_sem = FALSE;
	ftok_sem_reg = NULL;
	return TRUE;
}
