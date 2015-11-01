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
#include <sys/shm.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include "gtm_unistd.h"
#include <arpa/inet.h>
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include <sys/sem.h>
#include "gtm_sem.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "gtmio.h"
#include "repl_instance.h"
#include "mutex.h"
#include "jnl.h"
#include "repl_sem.h"
#include "eintr_wrappers.h"
#include "mu_rndwn_file.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "mu_rndwn_replpool.h"
#include "ipcrmid.h"
#include "do_semop.h"
#include "util.h"
#include "gtmmsg.h"

LITREF char             gtm_release_name[];
LITREF int4             gtm_release_name_len;

boolean_t mu_rndwn_replpool(replpool_identifier *replpool_id, int sem_id, int shmid)
{
	int			status, save_errno, semflgs;
	char			*insname;
	boolean_t		sem_created = FALSE;
	sm_uc_ptr_t		start_addr;
	struct semid_ds		semstat;
	struct shmid_ds		shm_buf;
	union semun		semarg;

	error_def(ERR_REPLACCSEM);
	error_def(ERR_TEXT);

	semarg.buf = &semstat;
	insname = replpool_id->instname;
	semflgs = RWDALL;
	if (0 == sem_id || (-1 == semctl(sem_id, 0, IPC_STAT, semarg)))
	{
		semflgs |= IPC_CREAT;
		assert(NUM_SRC_SEMS == NUM_RECV_SEMS);
		if (-1 == (sem_id = init_sem_set_source(IPC_PRIVATE, NUM_SRC_SEMS, semflgs)))
		{
			save_errno = errno;
			util_out_print("Error creating semaphore for instance name !AD, !AD", TRUE,
					LEN_AND_STR(insname), LEN_AND_STR(STRERROR(save_errno)));
		}
		sem_created = TRUE;
	} else
		set_sem_set_src(sem_id);
	/* assert that the same rundown logic can be used for both jnlpool and recvpool segments */
	assert(NUM_SRC_SEMS == NUM_RECV_SEMS);
	assert(JNL_POOL_ACCESS_SEM == RECV_POOL_ACCESS_SEM);
	status = grab_sem(SOURCE, JNL_POOL_ACCESS_SEM);
	if (SS_NORMAL != status)
	{
		save_errno = errno;
		util_out_print("Error during semop for replpool access semaphore (id = !UL) for instance name !AD, !AD", TRUE,
				sem_id, LEN_AND_STR(insname), LEN_AND_STR(STRERROR(save_errno)));
		if (sem_created)
		{
			if (-1 == semctl(sem_id, NUM_SRC_SEMS, IPC_RMID, 0))
			{
				save_errno = errno;
				util_out_print("Error removing replpool access semaphore (id = !UL) for instance name !AD, !AD",
						TRUE, sem_id, LEN_AND_STR(insname), LEN_AND_STR(STRERROR(save_errno)));
			}
		}
		return FALSE;
	}
	if (0 == shmid || -1 == shmctl(shmid, IPC_STAT, &shm_buf))
	{
		CLNUP_REPLPOOL_ACC_SEM(sem_id, insname);
		return TRUE;
	}
	if (-1 == (sm_long_t)(start_addr = shmat(shmid, 0, 0)))
	{
		save_errno = errno;
		util_out_print("Error attaching to replpool segment (id = !UL) for replication instance !AD, !AD", TRUE,
				shmid, LEN_AND_STR(insname), LEN_AND_STR(STRERROR(save_errno)));
		CLNUP_REPLPOOL_ACC_SEM(sem_id, insname);
		return FALSE;
	}
	if (0 != shm_buf.shm_nattch) /* It must be zero before I attach to it */
	{
		util_out_print("Replpool segment (id = !UL) for replication instance !AD is in use by another process.",
				TRUE, shmid, LEN_AND_STR(insname));
		MU_RNDWN_REPLPOOL_RETURN(FALSE);
	}
		/* assert that the identifiers are at the top of replpool control structure */
	assert(0 == offsetof(jnlpool_ctl_struct, jnlpool_id));
	assert(0 == offsetof(recvpool_ctl_struct, recvpool_id));
	memcpy((void *)replpool_id, (void *)start_addr, sizeof(replpool_identifier));
	if (memcmp(replpool_id->label, GDS_RPL_LABEL, GDS_LABEL_SZ - 1))
	{
		if (!memcmp(replpool_id->label, GDS_RPL_LABEL, GDS_LABEL_SZ - 3))
			util_out_print(
				"Incorrect version for the replpool segment (id = !UL) belonging to replication instance !AD",
					TRUE, shmid, LEN_AND_STR(insname));
		else
			util_out_print("Incorrect replpool format for the segment (id = !UL) belonging to replication instance !AD",
					TRUE, shmid, LEN_AND_STR(insname));
		MU_RNDWN_REPLPOOL_RETURN(FALSE);
	}
	if (memcmp(replpool_id->now_running, gtm_release_name, gtm_release_name_len + 1))
	{
		util_out_print("Attempt to access with version !AD, while already using !AD for replpool segment (id = !UL)"
				" belonging to replication instance !AD.", TRUE, gtm_release_name_len, gtm_release_name,
				LEN_AND_STR(replpool_id->now_running), shmid, LEN_AND_STR(insname));
		MU_RNDWN_REPLPOOL_RETURN(FALSE);
	}
	if (-1 == shmdt((caddr_t)start_addr))
	{
		util_out_print("Error detaching from shared memory segment (id = !UL) belonging to the replication instance !AD",
				TRUE, shmid, LEN_AND_STR(insname));
		MU_RNDWN_REPLPOOL_RETURN(FALSE);
	}
	if (0 != shm_rmid(shmid))
	{
		util_out_print("Error deleting replpool segment (id = !UL) belonging to the replication instance !AD",
				TRUE, shmid, LEN_AND_STR(insname));
		MU_RNDWN_REPLPOOL_RETURN(FALSE);
	}
	if (0 != sem_rmid(sem_id))
	{
		util_out_print("Error removing replpool access semaphore (id = !UL) for replication instance !AD", TRUE,
				sem_id, LEN_AND_STR(insname));
	}
	return TRUE;
}
