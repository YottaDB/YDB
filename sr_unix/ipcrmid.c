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

/* Perform forks to priv'd gtmsecshr routine to remove ipc ids */

#include "mdef.h"

#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include "gtm_string.h"
#include "gtm_limits.h"

#include "io.h"
#include "gtmsecshr.h"
#include "secshr_client.h"
#include "gtmmsg.h"
#include "ipcrmid.h"
#include "send_msg.h"

#define REMIPC(op, ipcid) (send_mesg2gtmsecshr((unsigned int)(op), (unsigned int)(ipcid), (char *)NULL, 0))

error_def(ERR_SYSCALL);

/* General note - in the routines below (and indeed in general), if the "error" is not relevant to the current process
 * being able to continue, process should (at most) send the message to the operator log and continue. No gtm_putmsg()
 * to the console and continue. This just screws up the output console for the client.
 */

/* Remove a semaphore id : if the current process has no permissions to remove it by itself,
 * force it through gtmsecshr. If we are a debug build and $gtm_useproc is set, send it through
 * gtmsecshr anyway.
 */
int sem_rmid(int ipcid)
{
	int 		status, save_errno;
	char		buff[128];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DEBUG_ONLY(if (!TREF(gtm_usesecshr)))
	{
		if (-1 == semctl(ipcid, 0, IPC_RMID))
		{
			if (EPERM != errno)
			{
				save_errno = errno;
				SNPRINTF(buff, 128, "semctl(IPC_RMID, %d)", ipcid);
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_STR(buff), CALLFROM, save_errno);
				errno = save_errno;
				return -1;
			} else
			{
				if (0 != (status = REMIPC(REMOVE_SEM, ipcid)))
				{
					errno = status;
					return -1;
				}
			}
		}
	}
#	ifdef DEBUG
	else if (0 != (status = REMIPC(REMOVE_SEM, ipcid)))
	{
		errno = status;
		return -1;
	}
#	endif
	return 0;
}

/* Remove a shared memory id */
int shm_rmid(int ipcid)
{
	int 		status, save_errno;
	char		buff[128];
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	DEBUG_ONLY(if (!TREF(gtm_usesecshr)))
	{
		if (-1 == shmctl(ipcid, IPC_RMID, NULL))
		{
			if (EPERM != errno)
			{
				save_errno = errno;
				SNPRINTF(buff, 128, "semctl(IPC_RMID, %d)", ipcid);
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_STR(buff), CALLFROM, save_errno);
				errno = save_errno;
				return -1;
			} else
			{
				if (0 != (status = REMIPC(REMOVE_SHM, ipcid)))
				{
					errno = status;
					return -1;
				}
			}
		}
	}
#	ifdef DEBUG
	else if (0 != (status = REMIPC(REMOVE_SHM, ipcid)))
	{
		errno = status;
		return -1;
	}
#	endif
	return 0;
}
