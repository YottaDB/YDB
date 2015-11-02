/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_inet.h"

#include <sys/shm.h>
#include <errno.h>
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
#include "repl_instance.h"
#include "wbox_test_init.h"
#include "have_crit.h"
#include "gtm_ipc.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	boolean_t		pool_init;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	int			gtmsource_srv_count;
GBLREF	int			gtmrecv_srv_count;

GBLREF	recvpool_addrs		recvpool;
GBLREF	int			recvpool_shmid;

int	gtmsource_ipc_cleanup(boolean_t auto_shutdown, int *exit_status, int4 *num_src_servers_running)
{
	boolean_t	i_am_the_last_user, attempt_ipc_cleanup;
	int		status, detach_status, remove_status, semval, save_errno;
	unix_db_info	*udi;
	struct shmid_ds	shm_buf;

	/* Attempt cleaning up the IPCs */
	attempt_ipc_cleanup = TRUE;
	*num_src_servers_running = 0;
	if (!auto_shutdown)
	{	/* If the value of counter semaphore is not 0 (some other source server is still up), cannot cleanup ipcs */
		semval = get_sem_info(SOURCE, SRC_SERV_COUNT_SEM, SEM_INFO_VAL);
		if (-1 == semval)
		{
			save_errno = errno;
			repl_log(stderr, TRUE, TRUE, "Error fetching source server count semaphore value : %s. "
					"Shutdown not complete\n", STRERROR(save_errno));
			attempt_ipc_cleanup = FALSE;
			*exit_status = ABNORMAL_SHUTDOWN;
		}
		if (0 != semval)
		{
			repl_log(stderr, TRUE, TRUE, "Not deleting jnlpool ipcs. %d source servers still attached to jnlpool\n",
				semval);
			*num_src_servers_running = semval;
			attempt_ipc_cleanup = FALSE;
			*exit_status = ABNORMAL_SHUTDOWN;
		}
	}
	udi = (unix_db_info *)FILE_INFO(jnlpool.jnlpool_dummy_reg);
	assert(INVALID_SHMID != udi->shmid);
	if (attempt_ipc_cleanup)
	{
		i_am_the_last_user = (((status = shmctl(udi->shmid, IPC_STAT, &shm_buf)) == 0) && (1 == shm_buf.shm_nattch));
		if (!i_am_the_last_user)
		{
			if (status < 0)
				repl_log(stderr, TRUE, TRUE, "Error in jnlpool shmctl : %s\n", STRERROR(ERRNO));
			else
				repl_log(stderr, TRUE, TRUE, "Not deleting jnlpool ipcs. %d processes still attached to jnlpool\n",
					 shm_buf.shm_nattch - 1);
			attempt_ipc_cleanup = FALSE;
			*exit_status = ABNORMAL_SHUTDOWN;
		} else if (INVALID_SHMID != udi->shmid)
		{
			if (INVALID_SEMID != jnlpool.repl_inst_filehdr->recvpool_semid DEBUG_ONLY(&&
						!(gtm_white_box_test_case_enabled &&
							(WBTEST_UPD_PROCESS_ERROR == gtm_white_box_test_case_number))))
				repl_log(stderr, TRUE, TRUE, "Receiver pool semaphore IDs were not removed\n");
			if ((INVALID_SHMID != jnlpool.repl_inst_filehdr->recvpool_shmid) DEBUG_ONLY(&&
						!(gtm_white_box_test_case_enabled &&
							 (WBTEST_UPD_PROCESS_ERROR == gtm_white_box_test_case_number))))
							 repl_log(stderr, TRUE, TRUE, "Receiver pool shared memory not removed\n");
			repl_inst_flush_jnlpool(TRUE, TRUE);
		}
	}
	/* detach from shared memory irrespective of whether we need to cleanup ipcs or not */
	JNLPOOL_SHMDT(detach_status, save_errno);
	if (0 == detach_status)
	{
		jnlpool.jnlpool_ctl = NULL; /* Detached successfully */
		jnlpool_ctl = NULL;
		jnlpool.repl_inst_filehdr = NULL;
		jnlpool.gtmsrc_lcl_array = NULL;
		jnlpool.gtmsource_local_array = NULL;
		jnlpool.jnldata_base = NULL;
		pool_init = FALSE;
	} else
	{
		repl_log(stderr, TRUE, TRUE, "Error detaching from Journal Pool : %s\n", STRERROR(save_errno));
		attempt_ipc_cleanup = FALSE;
		*num_src_servers_running = 0;
		*exit_status = ABNORMAL_SHUTDOWN;
	}
	if ((attempt_ipc_cleanup) && (INVALID_SHMID != udi->shmid))
	{
		remove_status = shm_rmid(udi->shmid);
		if (0 == remove_status)
		{
			repl_log(stdout, TRUE, FALSE, "Journal pool shared memory removed\n");
			if (0 == remove_sem_set(SOURCE))
				repl_log(stdout, TRUE, TRUE, "Journal pool semaphore removed\n");
			else
			{
				repl_log(stderr, TRUE, TRUE, "Error removing jnlpool semaphore : %s\n", STRERROR(ERRNO));
				*exit_status = ABNORMAL_SHUTDOWN;
			}
		} else
		{
			repl_log(stderr, TRUE, TRUE, "Error removing jnlpool shared memory : %s\n", STRERROR(ERRNO));
			*exit_status = ABNORMAL_SHUTDOWN;
		}
	}
	return attempt_ipc_cleanup;
}

int	gtmrecv_ipc_cleanup(boolean_t auto_shutdown, int *exit_status)
{
	boolean_t	i_am_the_last_user, attempt_ipc_cleanup;
	int		status, detach_status, remove_status, expected_nattach, save_errno;
	struct shmid_ds	shm_buf;

	/* Attempt cleaning up the IPCs */
	attempt_ipc_cleanup = TRUE;
	/* Wait for the Receiver Server and Update Process to detach and takeover the semaphores.
	 * Note that the Receiver Server has already waited for the Update Process to detach.
	 * It is done here as a precaution against Receiver Server crashes.
	 */
	if (!auto_shutdown)
		status = grab_sem(RECV, RECV_SERV_COUNT_SEM);
	else
		status = 0;
	if (0 == status && 0 > (status = grab_sem(RECV, UPD_PROC_COUNT_SEM)))
	{
		save_errno = errno;
		status = rel_sem(RECV, RECV_SERV_COUNT_SEM);
		assert(0 == status);
		repl_log(stderr, TRUE, TRUE, "Error taking control of Receiver Server/Update Process count semaphore : %s. "
				"Shutdown not complete\n", STRERROR(save_errno));
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
			repl_log(stderr, TRUE, TRUE, "Error in jnlpool shmctl : %s\n", STRERROR(errno));
		else
			repl_log(stderr, TRUE, TRUE,
				"Not deleting receive pool ipcs. %d processes still attached to receive pool\n",
				shm_buf.shm_nattch - expected_nattach);
		attempt_ipc_cleanup = FALSE;
		*exit_status = ABNORMAL_SHUTDOWN;
	}
	if (attempt_ipc_cleanup)
	{
		if (INVALID_SHMID != recvpool_shmid && (auto_shutdown || (detach_status = SHMDT(recvpool.recvpool_ctl)) == 0)
				       && (remove_status = shm_rmid(recvpool_shmid)) == 0)
		{
			recvpool.recvpool_ctl = NULL;
			repl_log(stdout, TRUE, FALSE, "Receive pool shared memory removed\n");
			if (0 == (status = remove_sem_set(RECV)))
				repl_log(stdout, TRUE, TRUE, "Receive pool semaphore removed\n");
			else
			{
				repl_log(stderr, TRUE, TRUE, "Error removing receive pool semaphore : %s\n", STRERROR(status));
				*exit_status = ABNORMAL_SHUTDOWN;
			}
		} else if (INVALID_SHMID != recvpool_shmid)
		{
			if (!auto_shutdown && detach_status < 0)
				repl_log(stderr, TRUE, TRUE,
					"Error detaching from receive pool shared memory : %s. Shared memory not removed\n",
					STRERROR(ERRNO));
			else if (remove_status != 0)
			{
				if (!auto_shutdown)
					recvpool.recvpool_ctl = NULL; /* Detached successfully */
				repl_log(stderr, TRUE, TRUE, "Error removing receive pool shared memory : %s\n", STRERROR(ERRNO));
			}
			*exit_status = ABNORMAL_SHUTDOWN;
		}
	}
	return attempt_ipc_cleanup;
}
