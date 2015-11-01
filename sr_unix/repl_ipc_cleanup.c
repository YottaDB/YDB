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

#include <sys/shm.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/sem.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "repl_dbg.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "repl_sp.h"
#include "repl_log.h"
#include "ipcrmid.h"


GBLREF jnlpool_addrs 	jnlpool;
GBLREF boolean_t	pool_init;
GBLREF jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF int		gtmsource_srv_count;
GBLREF int		gtmrecv_srv_count;

GBLREF recvpool_addrs	recvpool;
GBLREF int		recvpool_shmid;

int	gtmsource_ipc_cleanup(boolean_t auto_shutdown, int *exit_status)
{
	boolean_t	i_am_the_last_user, attempt_ipc_cleanup;
	int		status, detach_status, remove_status, expected_nattach;
	unix_db_info	*udi;
	struct shmid_ds	shm_buf;

	/* Attempt cleaning up the IPCs */
	attempt_ipc_cleanup = TRUE;

	/* Wait for the Source Server to detach and takeover the semaphore */
	if (!auto_shutdown && 0 > grab_sem(SOURCE, SRC_SERV_COUNT_SEM))
	{
		repl_log(stderr, FALSE, TRUE, "Error taking control of source server count semaphore : %s. Shutdown not complete\n",
														REPL_SEM_ERROR);
		*exit_status = ABNORMAL_SHUTDOWN;
		attempt_ipc_cleanup = FALSE;
	}

	udi = (unix_db_info *)FILE_INFO(jnlpool.jnlpool_dummy_reg);

	if (!auto_shutdown || !gtmsource_srv_count)
		expected_nattach = 1; /* Self, or parent */
	else
		expected_nattach = 0;  /* Source server already detached */

	i_am_the_last_user = (((status = shmctl(udi->shmid, IPC_STAT, &shm_buf)) == 0) && (shm_buf.shm_nattch == expected_nattach));
	if (!i_am_the_last_user)
	{
		if (status < 0)
			repl_log(stderr, FALSE, TRUE, "Error in jnlpool shmctl : %s\n", STRERROR(ERRNO));
		else
			repl_log(stderr, FALSE, TRUE, "Not deleting jnlpool ipcs. %d processes still attached to jnlpool\n",
				 shm_buf.shm_nattch - expected_nattach);
		attempt_ipc_cleanup = FALSE;
		*exit_status = ABNORMAL_SHUTDOWN;
	}

	if (attempt_ipc_cleanup)
	{
		if (udi->shmid > 0 && (auto_shutdown || (detach_status = SHMDT(jnlpool.jnlpool_ctl)) == 0)
				   && (remove_status = shm_rmid(udi->shmid)) == 0)
		{
			jnlpool.jnlpool_ctl = jnlpool_ctl = NULL;
			pool_init = FALSE;
			repl_log(stdout, FALSE, FALSE, "Journal pool shared memory removed\n");
			if (0 == (status = remove_sem_set(SOURCE)))
				repl_log(stdout, FALSE, TRUE, "Journal pool semaphore removed\n");
			else
			{
				repl_log(stderr, FALSE, TRUE, "Error removing jnlpool semaphore : %s\n", STRERROR(status));
				*exit_status = ABNORMAL_SHUTDOWN;
			}
		} else if (udi->shmid > 0)
		{
			if (!auto_shutdown && detach_status < 0)
				repl_log(stderr, FALSE, FALSE,
						"Error detaching from jnlpool shared memory : %s. Shared memory not removed\n",
						STRERROR(ERRNO));
			else if (remove_status != 0)
			{
				if (!auto_shutdown)
				{
					jnlpool.jnlpool_ctl = NULL; /* Detached successfully */
					jnlpool_ctl = NULL;
					pool_init = FALSE;
				}
				repl_log(stderr, FALSE, TRUE, "Error removing jnlpool shared memory : %s\n", STRERROR(ERRNO));
			}
			*exit_status = ABNORMAL_SHUTDOWN;
		}
	}
	return attempt_ipc_cleanup;
}

int	gtmrecv_ipc_cleanup(boolean_t auto_shutdown, int *exit_status)
{

	boolean_t	i_am_the_last_user, attempt_ipc_cleanup;
	int		status, detach_status, remove_status, expected_nattach;
	struct shmid_ds	shm_buf;

	/* Attempt cleaning up the IPCs */
	attempt_ipc_cleanup = TRUE;

	/*
	 * Wait for the Receiver Server and Update Process to detach and
	 * takeover the semaphores. Note that the Receiver Server has already
	 * waited for the Update Process to detach. It is done here as a
	 * precaution against Receiver Server crashes.
	 */

	if (!auto_shutdown)
		status = grab_sem(RECV, RECV_SERV_COUNT_SEM);
	else
		status = 0;
	if (0 == status && 0 > (status = grab_sem(RECV, UPD_PROC_COUNT_SEM)))
		rel_sem(RECV, RECV_SERV_COUNT_SEM);
	if (status < 0)
	{
		repl_log(stderr, FALSE, TRUE,
			"Error taking control of Receiver Server/Update Process count semaphore : %s. Shutdown not complete\n",
			REPL_SEM_ERROR);
		*exit_status = ABNORMAL_SHUTDOWN;
		attempt_ipc_cleanup = FALSE;
	}

	/* Now we have locked out all users from the receive pool */

	if (!auto_shutdown || !gtmrecv_srv_count)
		expected_nattach = 1; /* Self, or parent */
	else
		expected_nattach = 0; /* Receiver server already detached */

	i_am_the_last_user = (((status = shmctl(recvpool_shmid, IPC_STAT, &shm_buf)) == 0)
		&& (shm_buf.shm_nattch == expected_nattach));
	if (!i_am_the_last_user)
	{
		if (status < 0)
			repl_log(stderr, FALSE, TRUE, "Error in jnlpool shmctl : %s\n", STRERROR(ERRNO));
		else
			repl_log(stderr, FALSE, TRUE,
				"Not deleting receive pool ipcs. %d processes still attached to receive pool\n",
				shm_buf.shm_nattch - expected_nattach);
		attempt_ipc_cleanup = FALSE;
		*exit_status = ABNORMAL_SHUTDOWN;
	}

	if (attempt_ipc_cleanup)
	{
		if (recvpool_shmid > 0 && (auto_shutdown || (detach_status = SHMDT(recvpool.recvpool_ctl)) == 0)
				       && (remove_status = shm_rmid(recvpool_shmid)) == 0)
		{
			recvpool.recvpool_ctl = NULL;
			repl_log(stdout, FALSE, FALSE, "Receive pool shared memory removed\n");
			if (0 == (status = remove_sem_set(RECV)))
				repl_log(stdout, FALSE, TRUE, "Receive pool semaphore removed\n");
			else
			{
				repl_log(stderr, FALSE, TRUE, "Error removing receive pool semaphore : %s\n", STRERROR(status));
				*exit_status = ABNORMAL_SHUTDOWN;
			}
		} else if (recvpool_shmid > 0)
		{
			if (!auto_shutdown && detach_status < 0)
				repl_log(stderr, FALSE, FALSE,
					"Error detaching from receive pool shared memory : %s. Shared memory not removed\n",
					STRERROR(ERRNO));
			else if (remove_status != 0)
			{
				if (!auto_shutdown)
					recvpool.recvpool_ctl = NULL; /* Detached successfully */
				repl_log(stderr, FALSE, TRUE, "Error removing receive pool shared memory : %s\n", STRERROR(ERRNO));
			}
			*exit_status = ABNORMAL_SHUTDOWN;
		}
	}

	return attempt_ipc_cleanup;
}
