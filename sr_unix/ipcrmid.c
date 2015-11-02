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

/* Perform forks to priv'd gtmsecshr routine to remove ipc ids */

#include "mdef.h"
#include <sys/wait.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <errno.h>
#include "io.h"
#include "gtmsecshr.h"
#include "secshr_client.h"
#include "gtmmsg.h"
#include "ipcrmid.h"

#define REMIPC(op, ipcid) (send_mesg2gtmsecshr((unsigned int)(op), (unsigned int)(ipcid), (char *)NULL, 0))

/* Remove a semaphore id : if the current process has no permissions to remove it by itself,
 * force it through gtmsecshr.
 */
int sem_rmid(int ipcid)
{
	int status;

	if (-1 == semctl(ipcid, 0, IPC_RMID))
	{
		if (EPERM != errno)
		{
			gtm_putmsg(VARLSTCNT(1) errno);
			return -1;
		} else
		{
			if (0 != (status = REMIPC(REMOVE_SEM, ipcid)))
			{
				gtm_putmsg(VARLSTCNT(1) status);
				return -1;
			}
		}
	}
	return 0;
}

/* Remove a shared memory id */
int shm_rmid(int ipcid)
{
	int status;

	if (-1 == shmctl(ipcid, IPC_RMID, NULL))
	{
		if (EPERM != errno)
		{
			gtm_putmsg(VARLSTCNT(1) errno);
			return -1;
		} else
		{
			if (0 != (status = REMIPC(REMOVE_SHMMEM, ipcid)))
			{
				gtm_putmsg(VARLSTCNT(1) status);
				return -1;
			}
		}
	}
	return 0;
}
