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

#include "gtm_ipc.h"
#include <sys/mman.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "eintr_wrappers.h"
#include "mu_rndwn_file.h"
#include "gtm_string.h"
#include "error.h"
#include "io.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stdio.h"
#include "gtm_stat.h"
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


GBLREF boolean_t		mupip_jnl_recover;
GBLREF gd_region		*gv_cur_region;
GBLREF uint4			process_id;
GBLREF enum gtmImageTypes	image_type;
GBLREF boolean_t		sem_incremented;
GBLREF gd_region		*ftok_sem_reg;
GBLREF volatile boolean_t 	semwt2long;

static struct sembuf		ftok_sop[3];
static int			ftok_sopcnt;

#define MAX_SEM_WT      	(1000 * 30)		/* 30 second wait for DSE to acquire access control semaphore */
#define MAX_RES_TRIES  	 	620

/*
 * Description:
 * 	Using project_id this will find FTOK of FILE_INFO(reg)->fn.
 * 	Create semaphore set of id "ftok_semid" using that project_id, if it does not exist.
 * 	Then it will lock ftok_semid.
 * Parameters:
 * 	IF incr_cnt == TRUE, it will increment counter semaphore.
 * 	IF immidate == TRUE, it will use IPC_NOWAIT flag.
 * Return Value: TRUE, if succsessful
 *	         FALSE, if fails.
 */
boolean_t ftok_sem_get(gd_region *reg, boolean_t incr_cnt, int project_id, boolean_t immediate)
{
	int			sem_pid, semflag;
        int4            	status;
        uint4           	lcnt;
        unix_db_info    	*udi;

	error_def(ERR_FTOKERR);
	error_def(ERR_TEXT);
        error_def(ERR_CRITSEMFAIL);
	error_def(ERR_SEMWT2LONG);

	assert(reg);
        udi = FILE_INFO(reg);
	assert(!udi->grabbed_ftok_sem);
	assert(NULL == ftok_sem_reg);
        if (-1 == (udi->key = FTOK(udi->fn, project_id)))
	{
                gtm_putmsg(VARLSTCNT(9) ERR_FTOKERR, 2, DB_LEN_STR(reg),
			ERR_TEXT, 2, RTS_ERROR_TEXT("Error getting ftok"), errno);
		return FALSE;
	}
	semflag = SEM_UNDO | (immediate ? IPC_NOWAIT : 0);
	/*
	 * the purpose of this loop is to deal with possibility that the semaphores may
	 * be deleted as they are attached.
	 */
	for (status = -1, lcnt = 0;  -1 == status;  lcnt++)
	{
		if (-1 == (udi->ftok_semid = semget(udi->key, 2, RWDALL | IPC_CREAT)))
		{
			gtm_putmsg(VARLSTCNT(5) ERR_FTOKERR, 2, DB_LEN_STR(reg), errno);
			return FALSE;
		}
		ftok_sop[0].sem_num = 0; ftok_sop[0].sem_op = 0;	/* Wait for 0 (unlocked) */
		ftok_sop[1].sem_num = 0; ftok_sop[1].sem_op = 1;	/* Then lock it */
		ftok_sopcnt = 2;
		if (incr_cnt)
		{
			ftok_sop[2].sem_num = 1; ftok_sop[2].sem_op = 1;	/* increment counter semaphore */
			ftok_sopcnt = 3;
		}
		if (DSE_IMAGE == image_type)
		{
			assert(incr_cnt);
			/* First try non-blocking */
			ftok_sop[0].sem_flg = ftok_sop[1].sem_flg = ftok_sop[2].sem_flg = SEM_UNDO | IPC_NOWAIT;
			status = semop(udi->ftok_semid, ftok_sop, ftok_sopcnt);
			if (-1 == status)
			{
				if (EAGAIN == errno)	/* Someone else is holding it */
				{
					sem_pid = semctl(udi->ftok_semid, 0, GETPID);
					if (-1 == sem_pid)
					{
						if (EINVAL == errno)		/* the sem might have been deleted */
							continue;
						else
						{
							gtm_putmsg(VARLSTCNT(9) ERR_CRITSEMFAIL, 2,
								DB_LEN_STR(reg), ERR_TEXT, 2,
								RTS_ERROR_TEXT("semop/semctl of ftok_semid failed"), errno);
							return FALSE;
						}
					}
					util_out_print("Semaphore ftok_sop for region !AD is held by pid, !UL. "
						       "An attempt will be made in the next 30 seconds to grab it.",
						       TRUE, DB_LEN_STR(reg), sem_pid);
					semwt2long = FALSE;
					start_timer((TID)semwt2long_handler, MAX_SEM_WT, semwt2long_handler, 0, NULL);
				} else if (((EINVAL != errno) && (EIDRM != errno) && (EINTR != errno)) || (MAX_RES_TRIES < lcnt))
				{
					gtm_putmsg(VARLSTCNT(9) ERR_CRITSEMFAIL, 2,
						DB_LEN_STR(reg), ERR_TEXT, 2,
						RTS_ERROR_TEXT("db_init semop of ftok_semid"), errno);
					return FALSE;
				}
				else
					continue;
			} else
				break;
		}
		ftok_sop[0].sem_flg = ftok_sop[1].sem_flg = ftok_sop[2].sem_flg = semflag;
		status = semop(udi->ftok_semid, ftok_sop, ftok_sopcnt);
		if (-1 == status)
		{
			if (DSE_IMAGE != image_type)
			{
				if (((EINVAL != errno) && (EIDRM != errno) && (EINTR != errno)) || (MAX_RES_TRIES < lcnt))
				{
					/* issue gtm_putmsg, if it is not EINVAL, EIDRM or, EINTR error.  */
					gtm_putmsg(VARLSTCNT(9) ERR_CRITSEMFAIL, 2,
						DB_LEN_STR(reg), ERR_TEXT, 2,
						RTS_ERROR_TEXT("db_init semop of ftok_semid"), errno);
					return FALSE;
				}
				/* else continue */
			} else
			{
				if (EINTR == errno && semwt2long)
				{
					/* Timer popped. DSE will give up. */
					sem_pid = semctl(udi->ftok_semid, 0, GETPID);
					if (-1 != sem_pid)
					{
						gtm_putmsg(VARLSTCNT(5) ERR_SEMWT2LONG, 3,  DB_LEN_STR(reg), sem_pid);
						return FALSE;
					} else
					{
						gtm_putmsg(VARLSTCNT(5) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), errno);
						return FALSE;
					}
				} else if (((EINVAL != errno) && (EIDRM != errno) && (EINTR != errno)) || (MAX_RES_TRIES < lcnt))
				{
					/* issue gtm_putmsg, if it is neither EINVAL nor EIDRM nor EINTR error.  */
					cancel_timer((TID)semwt2long_handler);
					gtm_putmsg(VARLSTCNT(5) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), errno);
					return FALSE;
				}
				/* else continue */
			}
		} else if (DSE_IMAGE == image_type)
			cancel_timer((TID)semwt2long_handler);
	} /* end for loop */
	ftok_sem_reg = reg;
	udi->grabbed_ftok_sem = TRUE;
	return TRUE;
}

/*
 * Description:
 * 	Assumes that ftok semaphore already exists. Just lock it.
 * Parameters:
 * 	IF incr_cnt == TRUE, it will increment counter semaphore.
 * 	IF immidate == TRUE, it will use IPC_NOWAIT flag.
 * Return Value: TRUE, if succsessful
 *	         FALSE, if fails.
 */
boolean_t ftok_sem_lock(gd_region *reg, boolean_t incr_cnt, boolean_t immediate)
{
	int			semflag, save_errno, status;
	unix_db_info		*udi;

	error_def(ERR_CRITSEMFAIL);
	error_def(ERR_TEXT);

	assert(reg);
        udi = FILE_INFO(reg);
	assert(!udi->grabbed_ftok_sem);
	assert(NULL == ftok_sem_reg);
	assert(-1 != udi->ftok_semid && -1 != udi->semid && -1 != udi->shmid);
	ftok_sopcnt = 0;
	semflag = SEM_UNDO | (immediate ? IPC_NOWAIT : 0);
	if (!udi->grabbed_ftok_sem)
	{
		/*
		 * We need to gaurantee that none else access database file header
		 * when semid/shmid fields are updated in file header
		 */
		ftok_sop[0].sem_num = 0; ftok_sop[0].sem_op = 0;	/* Wait for 0 (unlocked) */
		ftok_sop[1].sem_num = 0; ftok_sop[1].sem_op = 1;	/* Then lock it */
		ftok_sop[0].sem_flg = ftok_sop[1].sem_flg = SEM_UNDO | IPC_NOWAIT;
		ftok_sopcnt = 2;
	} else if (!incr_cnt)
		return TRUE;
	if (incr_cnt)
	{
		ftok_sop[ftok_sopcnt].sem_num = 1; ftok_sop[ftok_sopcnt].sem_op = 1;
				/* increment counter for this semaphore */
		ftok_sop[ftok_sopcnt].sem_flg = SEM_UNDO | IPC_NOWAIT;
		ftok_sopcnt++;
	}
	SEMOP(udi->ftok_semid, ftok_sop, ftok_sopcnt, status);
	if (-1 == status)	/* We couldn't get it in one shot -- see if we already have it */
	{
		save_errno = errno;
		if (semctl(udi->ftok_semid, 0, GETPID) == process_id)
		{
			gtm_putmsg(VARLSTCNT(9) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, RTS_ERROR_TEXT("ftok_semid lock is owned by itself"), save_errno);
			return FALSE;
		}
		if (EAGAIN != save_errno)
		{
			gtm_putmsg(VARLSTCNT(9) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, RTS_ERROR_TEXT("SEMOP/semctl failed on ftok_semid"), save_errno);
			return FALSE;
		}
		ftok_sop[0].sem_flg = ftok_sop[1].sem_flg = semflag;
		if (incr_cnt)
			ftok_sop[2].sem_flg = semflag;
		/* Try again - blocking this time */
		SEMOP(udi->ftok_semid, ftok_sop, ftok_sopcnt, status);
		if (-1 == status)			/* We couldn't get it at all.. */
		{
			gtm_putmsg(VARLSTCNT(5) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), errno);
			return FALSE;
		}
	}
	udi->grabbed_ftok_sem = TRUE;
	ftok_sem_reg = reg;
	return TRUE;
}

/*
 * Description:
 * 	Assumes that ftok semaphore was already locked. Now release it.
 * Parameters:
 * 	IF decr_count == TRUE, it will decrement counter semaphore.
 * 	IF immidate == TRUE, it will use IPC_NOWAIT flag.
 * Return Value: TRUE, if succsessful
 *	         FALSE, if fails.
 */
boolean_t ftok_sem_release(gd_region *reg,  boolean_t decr_count, boolean_t immediate)
{
	int		ftok_semval, semflag, save_errno;
	unix_db_info 	*udi;
	error_def (ERR_CRITSEMFAIL);
	error_def (ERR_TEXT);

	assert(NULL != reg);
	assert(reg == ftok_sem_reg);
	udi = FILE_INFO(reg);
	assert(udi->grabbed_ftok_sem);
	assert(udi && -1 != udi->ftok_semid);
	/* if we dont have the ftok semaphore, return true even if decr_cnt was requested */
	if (!udi->grabbed_ftok_sem)
		return TRUE;
	semflag = SEM_UNDO | (immediate ? IPC_NOWAIT : 0);
	if (decr_count)
	{
		if (-1 == (ftok_semval = semctl(udi->ftok_semid, 1, GETVAL)))
		{
			gtm_putmsg(VARLSTCNT(9) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
				ERR_TEXT, 2, RTS_ERROR_TEXT("ftok_sem_release semctl failed"), errno);
			return FALSE;
		}
		if (1 >= ftok_semval)	/* checking against 0, in case already we decremented semaphore number 1 */
		{
			if (0 != sem_rmid(udi->ftok_semid))
			{
				gtm_putmsg(VARLSTCNT(8) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
					ERR_TEXT, 2, RTS_ERROR_TEXT("ftok_sem_release sem_rmid"));
				return FALSE;
			}
			ftok_sem_reg = NULL;
			udi->grabbed_ftok_sem = FALSE;
			return TRUE;
		}
		if (0 != (save_errno = do_semop(udi->ftok_semid, 1, -1, semflag)))
		{
			gtm_putmsg(VARLSTCNT(9) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
			ERR_TEXT, 2, RTS_ERROR_TEXT("ftok_sem_release do_semop on count semaphore"), save_errno);
			return FALSE;
		}
	}
	if (0 != (save_errno = do_semop(udi->ftok_semid, 0, -1, semflag)))
	{
		gtm_putmsg(VARLSTCNT(9) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
			ERR_TEXT, 2, RTS_ERROR_TEXT("ftok_sem_release do_semop"), save_errno);
		return FALSE;
	}
	udi->grabbed_ftok_sem = FALSE;
	ftok_sem_reg = NULL;
	return TRUE;
}
