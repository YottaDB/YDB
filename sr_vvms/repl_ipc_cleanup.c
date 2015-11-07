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

#include "gtm_inet.h" /* Required for gtmsource.h */

#include <ssdef.h>
#include <secdef.h>
#include <psldef.h>
#include <descrip.h>

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
#include "repl_shutdcode.h"
#include "repl_sem.h"
#include "repl_shm.h"
#include "repl_log.h"

GBLREF jnlpool_addrs 		jnlpool;
GBLREF boolean_t	        pool_init;
GBLREF jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF int4			jnlpool_shmid;
GBLREF recvpool_addrs	        recvpool;
GBLREF int		        recvpool_shmid;
GBLREF int		        gtmsource_srv_count;
GBLREF int		        gtmrecv_srv_count;

int	gtmsource_ipc_cleanup(boolean_t auto_shutdown, int *exit_status)
{
	int		status, detach_status, remove_status;
	boolean_t	attempt_ipc_cleanup;
	int4		shm_lockid;

	attempt_ipc_cleanup = TRUE; /* attempt cleaning up the IPCs */
	/* Wait for the Source Server to detach and takeover the semaphore */
	if (!auto_shutdown && 0 > grab_sem(SOURCE, SRC_SERV_COUNT_SEM))
	{
		repl_log(stderr, FALSE, TRUE, "Error taking control of source server count semaphore : %s. Shutdown not complete\n",
														REPL_SEM_ERROR);
		*exit_status = ABNORMAL_SHUTDOWN;
		attempt_ipc_cleanup = FALSE;
	}
	/* Now we have locked out all users from the journal pool and no process can initiate any other action.
	 * Save the lock-id in a local variable because the structure holding it ("jnlpool") is memset to 0 below */
	shm_lockid = jnlpool.shm_lockid;
	if (SS$_NORMAL != (status = lastuser_of_gsec(shm_lockid)))
	{
		repl_log(stderr, FALSE, TRUE,
				"Not deleting jnlpool global section as other processes are still attached to it : %s\n",
				REPL_STR_ERROR);
		attempt_ipc_cleanup = FALSE;
		*exit_status = ABNORMAL_SHUTDOWN;
	}
	if (attempt_ipc_cleanup)
	{
		if ((0 < jnlpool_shmid) && (auto_shutdown || SS$_NORMAL == (detach_status = detach_shm(jnlpool.shm_range)))
					&& SS$_NORMAL == (remove_status = delete_shm(&jnlpool.vms_jnlpool_key.desc)))
		{
			memset((uchar_ptr_t)&jnlpool, 0, SIZEOF(jnlpool)); /* For gtmsource_exit */
			jnlpool.jnlpool_ctl = NULL;
			jnlpool_ctl = NULL;
			jnlpool_shmid = 0;
			pool_init = FALSE;
			repl_log(stdout, FALSE, FALSE, "Journal pool shared memory removed\n");
			if (0 == remove_sem_set(SOURCE))
				repl_log(stdout, FALSE, TRUE, "Journal pool semaphore removed\n");
			else
			{
				repl_log(stderr, FALSE, TRUE, "Error removing jnlpool semaphore : %s\n", REPL_SEM_ERROR);
				*exit_status = ABNORMAL_SHUTDOWN;
			}
		} else if (0 < jnlpool_shmid)
		{
			if (!auto_shutdown && SS$_NORMAL != detach_status)
				repl_log(stderr, TRUE, TRUE, "Error detaching from jnlpool : %s\n", REPL_STR_ERROR1(detach_status));
			else if (SS$_NORMAL != remove_status)
			{
				if (!auto_shutdown)
				{
					jnlpool.jnlpool_ctl = NULL; /* Detached successfully */
					jnlpool_ctl = NULL;
					pool_init = FALSE;
				}
				repl_log(stderr, FALSE, TRUE, "Error removing jnlpool shared memory : %s\n",
												REPL_STR_ERROR1(remove_status));
			}
			*exit_status = ABNORMAL_SHUTDOWN;
		}
		if (SS$_NORMAL != (status = signoff_from_gsec(shm_lockid)))
			repl_log(stderr, TRUE, TRUE, "Error dequeueing lock on jnlpool global section : %s\n", REPL_STR_ERROR);
	}
	return attempt_ipc_cleanup;
}

int	gtmrecv_ipc_cleanup(boolean_t auto_shutdown, int *exit_status)
{
	int		status, detach_status, remove_status;
	boolean_t	attempt_ipc_cleanup;
	int4		shm_lockid;

	attempt_ipc_cleanup = TRUE; /* attempt cleaning up the IPCs */
	/* Wait for the Receiver Server and Update Process to detach and takeover the semaphores.
	 * Note that the Receiver Server has already waited for the Update Process to detach.
	 * It is done here as a precaution against Receiver Server crashes.
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
			 "Error taking control of Receiver Server/Update Process count semaphore : "
			 "%s. Shutdown not complete\n", REPL_SEM_ERROR);
		*exit_status = ABNORMAL_SHUTDOWN;
		attempt_ipc_cleanup = FALSE;
	}
	/* Now we have locked out all users from the receive pool and no process can initiate any other action.
	 * Save the lock-id in a local variable because the structure holding it ("jnlpool") is memset to 0 below */
	shm_lockid = recvpool.shm_lockid;
	if ((!auto_shutdown || gtmrecv_srv_count) && SS$_NORMAL != (status = lastuser_of_gsec(shm_lockid)))
	{
		repl_log(stderr, FALSE, TRUE,
				"Not deleting recvpool global section as other processes are still attached to it : "
				"%s\n", REPL_STR_ERROR);
		attempt_ipc_cleanup = FALSE;
		*exit_status = ABNORMAL_SHUTDOWN;
	}
	if (attempt_ipc_cleanup)
	{
		if ((0 < recvpool_shmid) && (auto_shutdown || SS$_NORMAL == (detach_status = detach_shm(recvpool.shm_range)))
					&& SS$_NORMAL == (remove_status = delete_shm(&recvpool.vms_recvpool_key.desc)))
		{
			memset((uchar_ptr_t)&recvpool, 0, SIZEOF(recvpool)); /* For gtmrecv_exit */
			recvpool.recvpool_ctl = NULL;
			recvpool_shmid = 0;
			repl_log(stdout, FALSE, FALSE, "Recv pool shared memory removed\n");
			if (0 == remove_sem_set(RECV))
				repl_log(stdout, FALSE, TRUE, "Recv pool semaphore removed\n");
			else
			{
				repl_log(stderr, FALSE, TRUE, "Error removing recvpool semaphore : %s\n", REPL_SEM_ERROR);
				*exit_status = ABNORMAL_SHUTDOWN;
			}
		} else if (0 < recvpool_shmid)
		{
			if (!auto_shutdown && SS$_NORMAL != detach_status)
				repl_log(stderr, TRUE, TRUE,
						"Error detaching from recvpool : %s\n", REPL_STR_ERROR1(detach_status));
			else if (SS$_NORMAL != remove_status)
			{
				if (!auto_shutdown)
					recvpool.recvpool_ctl = NULL; /* Detached successfully */
				repl_log(stderr, FALSE, TRUE, "Error removing recvpool shared memory : %s\n",
												REPL_STR_ERROR1(remove_status));
			}
			*exit_status = ABNORMAL_SHUTDOWN;
		}
	}
	if (SS$_NORMAL != (status = signoff_from_gsec(shm_lockid)))
		repl_log(stderr, TRUE, TRUE, "Error dequeueing lock on recvpool global section : %s\n", REPL_STR_ERROR);
	return attempt_ipc_cleanup;
}
