/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"
#include "gtm_ipc.h"
#include "gtm_stdio.h"

#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>

#include "gtm_sem.h"
#include "iosp.h"
#include "gtm_c_stack_trace.h"
#include "eintr_wrappers.h"
#include "eintr_wrapper_semop.h"
#include "ipcrmid.h"
#include "util.h"
#include "mupip_upgrade_standalone.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"

/* Lock a file using semaphore.
 * This is written so that we can use it to lock all database pre-V4.2000.
 * Parameters:
 *	fn : database file name.
 * 	semid: returns semaphore id. Caller may need to remove it.
 * Return TRUE if successful.
 * Else Return FALSE.
 */
boolean_t mupip_upgrade_standalone(char *fn, int *semid)
{
	struct sembuf	sop[4];
	int		key, semop_res, sems;

	*semid = INVALID_SEMID;
	if ((key = FTOK(fn, 1)) == -1) /* V3.2 and V4.0x had project id = 1 */
	{
		util_out_print("Error with ftok", TRUE);
		return FALSE;
	}
	/* If shared memory exists, the disk file is probably incomplete */
	if ((shmget(key, 0, 0) != -1) || errno != ENOENT)
	{
		util_out_print("File is locked by another user", TRUE);
		return FALSE;
	}
	if ((sems = semget(key, FTOK_SEM_PER_ID, RWDALL | IPC_CREAT | IPC_NOWAIT)) == -1)
	{
		util_out_print("Error with semget", TRUE);
		return FALSE;
	}
	/*
	 * Both semaphores must have value 0 and then all will be
	 * incremented to completely lockout any other potential users
	 * until we are done
	 */
	sop[0].sem_num = 0; sop[0].sem_op = 0;	/* First check all semaphore have 0 value */
	sop[1].sem_num = 1; sop[1].sem_op = 0;
	sop[2].sem_num = 0; sop[2].sem_op = 1; 	/* Increment all semaphores */
	sop[3].sem_num = 1; sop[3].sem_op = 1;
	sop[0].sem_flg = sop[1].sem_flg = sop[2].sem_flg = sop[3].sem_flg = SEM_UNDO | IPC_NOWAIT;
	SEMOP(sems, sop, 4, semop_res, NO_WAIT);
	if (-1 == semop_res)
	{
		if (errno == EAGAIN)
		{
			util_out_print("File already open by another process", TRUE);
			return FALSE;
		}
		util_out_print("Error with SEMOP", TRUE);
		return FALSE;
	}
	/* See if someone built some shared memory in the time we were
	 * building semaphores. If so, we have failed */
	if((shmget(key, 0, 0) != -1) || errno != ENOENT)
	{
		if (0 != sem_rmid(sems))
		{
			util_out_print("Error with sem_rmid", TRUE);
			return FALSE;
		}
		util_out_print("File is locked by another user", TRUE);
		return FALSE;
	}
	*semid = sems;
	return TRUE;
}
