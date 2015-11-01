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

boolean_t mu_rndwn_replpool(replpool_identifier *replpool_id, int sem_id, int shm_id)
{
	int			semval, status, save_errno;
	char			*insname;
	boolean_t		sem_created = FALSE;
	sm_uc_ptr_t		start_addr;
	struct semid_ds		semstat;
	struct shmid_ds		shm_buf;
	union semun		semarg;

	error_def(ERR_REPLPOOLINST);
	error_def(ERR_REPLACCSEM);
	error_def(ERR_SYSCALL);
	error_def(ERR_TEXT);

	assert(NUM_SRC_SEMS == NUM_RECV_SEMS);
	/* assert that the same rundown logic can be used for both jnlpool and recvpool segments */
	assert(NUM_SRC_SEMS == NUM_RECV_SEMS);
	assert(JNL_POOL_ACCESS_SEM == RECV_POOL_ACCESS_SEM);
	if (INVALID_SHMID == shm_id)
		return TRUE;
	semarg.buf = &semstat;
	insname = replpool_id->instname;
	if (INVALID_SEMID == sem_id || (-1 == semctl(sem_id, 0, IPC_STAT, semarg)))
	{
		if (INVALID_SEMID == (sem_id = init_sem_set_source(IPC_PRIVATE, NUM_SRC_SEMS, RWDALL | IPC_CREAT)))
		{
			save_errno = errno;
			gtm_putmsg(VARLSTCNT(5) ERR_REPLACCSEM, 3, sem_id, RTS_ERROR_STRING(insname));
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semget()"), CALLFROM, save_errno);
			return FALSE;
		}
		sem_created = TRUE;
	} else
		set_sem_set_src(sem_id);
	status = grab_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
	if (SS_NORMAL != status)
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_REPLACCSEM, 3, sem_id, RTS_ERROR_STRING(insname));
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semget()"), CALLFROM, save_errno);
		CLNUP_REPLPOOL_ACC_SEM(sem_id, insname);
		return FALSE;
	}
	if (-1 == (semval = semctl(sem_id, SRC_SERV_COUNT_SEM, GETVAL)))
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_REPLACCSEM, 3, sem_id, RTS_ERROR_STRING(insname));
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semget()"), CALLFROM, save_errno);
		CLNUP_REPLPOOL_ACC_SEM(sem_id, insname);
		return FALSE;
	}
	if (0 < semval)
	{
		util_out_print("Replpool semaphore (id = !UL) for replication instance !AD is in use by another process.",
				TRUE, sem_id, LEN_AND_STR(insname));
		CLNUP_REPLPOOL_ACC_SEM(sem_id, insname);
		return FALSE;
	}
	if (-1 == shmctl(shm_id, IPC_STAT, &shm_buf))
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_REPLPOOLINST, 3, shm_id, RTS_ERROR_STRING(insname));
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("shmctl()"), CALLFROM, save_errno);
		CLNUP_REPLPOOL_ACC_SEM(sem_id, insname);
		return FALSE;
	}
	if (0 != shm_buf.shm_nattch) /* It must be zero before I attach to it */
	{
		util_out_print("Replpool segment (id = !UL) for replication instance !AD is in use by another process.",
				TRUE, shm_id, LEN_AND_STR(insname));
		CLNUP_REPLPOOL_ACC_SEM(sem_id, insname);
		return FALSE;
	}
	if (-1 == (sm_long_t)(start_addr = (sm_uc_ptr_t) do_shmat(shm_id, 0, 0)))
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_REPLPOOLINST, 3, shm_id, RTS_ERROR_STRING(insname));
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("shmat()"), CALLFROM, save_errno);
		CLNUP_REPLPOOL_ACC_SEM(sem_id, insname);
		return FALSE;
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
					TRUE, shm_id, LEN_AND_STR(insname));
		else
			util_out_print("Incorrect replpool format for the segment (id = !UL) belonging to replication instance !AD",
					TRUE, shm_id, LEN_AND_STR(insname));
		MU_RNDWN_REPLPOOL_RETURN(FALSE);
	}
	if (memcmp(replpool_id->now_running, gtm_release_name, gtm_release_name_len + 1))
	{
		util_out_print("Attempt to access with version !AD, while already using !AD for replpool segment (id = !UL)"
				" belonging to replication instance !AD.", TRUE, gtm_release_name_len, gtm_release_name,
				LEN_AND_STR(replpool_id->now_running), shm_id, LEN_AND_STR(insname));
		MU_RNDWN_REPLPOOL_RETURN(FALSE);
	}
	if (-1 == shmdt((caddr_t)start_addr))
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_REPLPOOLINST, 3, shm_id, RTS_ERROR_STRING(insname));
		gtm_putmsg(VARLSTCNT(7) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("shmdt()"), CALLFROM, save_errno);
		CLNUP_REPLPOOL_ACC_SEM(sem_id, insname);
		return FALSE;
	}
	if (0 != shm_rmid(shm_id))
	{
		gtm_putmsg(VARLSTCNT(5) ERR_REPLPOOLINST, 3, shm_id, RTS_ERROR_STRING(insname));
		gtm_putmsg(VARLSTCNT(7) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("shm_ctl()"), CALLFROM);
		MU_RNDWN_REPLPOOL_RETURN(FALSE);
	}
	if (0 != sem_rmid(sem_id))
	{
		gtm_putmsg(VARLSTCNT(5) ERR_REPLACCSEM, 3, sem_id, RTS_ERROR_STRING(insname));
		gtm_putmsg(VARLSTCNT(7) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semget()"), CALLFROM);
		return FALSE;
	}
	return TRUE;
}
