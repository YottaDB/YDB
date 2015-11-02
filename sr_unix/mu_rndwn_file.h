/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_RNDWN_DEFINED
#define MU_RNDWN_DEFINED

/* for FTOK_SEM_PER_ID */
#include "gtm_sem.h"

#define SHMID           2

#ifdef __linux__
#define KEY             1
#define IPCS_CMD_STR		"ipcs -m | grep '^0x'"
#define IPCS_SEM_CMD_STR	"ipcs -s | grep '^0x'"
#else
#define KEY             3
/* though the extra blank space is required in AIX under certain cases, we
 * are adding it for all UNIX versions to avoid another ifdef for AIX.
 */
#define IPCS_CMD_STR		"ipcs -m | grep '^m' | sed 's/^m/m /g'"
#define IPCS_SEM_CMD_STR	"ipcs -s | grep '^s' | sed 's/^s/s /g'"
#endif /* __linux__ */

#define SGMNTSIZ        10
#define MAX_PARM_LEN    128
#define MAX_ENTRY_LEN   1024


typedef struct shm_parms_struct
{
	ssize_t		sgmnt_siz;
        int             shmid;
        key_t           key;
} shm_parms;

#define RESET_GV_CUR_REGION						\
{									\
	gv_cur_region = temp_region;					\
	cs_addrs = temp_cs_addrs;					\
	cs_data = temp_cs_data;						\
}

#define RNDWN_ERR(str, reg)						\
{									\
	save_errno = errno;						\
	if (reg)							\
		util_out_print(str, TRUE, DB_LEN_STR(reg));		\
	else								\
		util_out_print(str, TRUE);				\
	util_out_print(STRERROR(save_errno), TRUE);			\
}

#define SEG_SHMATTACH(addr, reg)								\
{												\
	if (-1 == (sm_long_t)(cs_addrs->db_addrs[0] = (sm_uc_ptr_t)				\
				do_shmat(udi->shmid, addr, SHM_RND)))				\
	{											\
		if (EINVAL != errno)								\
			RNDWN_ERR("Error attaching to shared memory for file !AD", (reg));	\
		/* shared memory segment no longer exists */					\
		CLNUP_RNDWN(udi, (reg));							\
		RESET_GV_CUR_REGION;								\
		return FALSE;									\
	}											\
}

#define SEG_MEMMAP(addr, reg)										\
{													\
	if (-1 == (sm_long_t)(cs_addrs->db_addrs[0] = (sm_uc_ptr_t)mmap((caddr_t)addr,          	\
		(size_t)stat_buf.st_size, PROT_READ | PROT_WRITE, GTM_MM_FLAGS, udi->fd, (off_t)0)))	\
	{												\
		RNDWN_ERR("Error mapping memory for file !AD", (reg));					\
		CLNUP_RNDWN(udi, (reg));								\
		RESET_GV_CUR_REGION;									\
		return FALSE;										\
	}												\
}

#define CLNUP_RNDWN(udi, reg)									\
{												\
	close(udi->fd);										\
	if (sem_created)									\
	{											\
		if (-1 == semctl(udi->semid, 0, IPC_RMID))					\
			RNDWN_ERR("Error removing the semaphore for file !AD", (reg));		\
	} else											\
		do_semop(udi->semid, 0, -1, IPC_NOWAIT | SEM_UNDO);				\
	REVERT;											\
	ftok_sem_release(reg, TRUE, TRUE);							\
	if (restore_rndwn_gbl)									\
		RESET_GV_CUR_REGION;								\
}

#define CLNUP_SEM(sem_id, reg)									\
{												\
	if (sem_created)									\
	{											\
		if (-1 == semctl((sem_id), 0, IPC_RMID))					\
			RNDWN_ERR("Error removing the semaphore for file !AD", (reg));		\
	} else											\
		do_semop((sem_id), 0, -1, IPC_NOWAIT | SEM_UNDO);				\
}

#define CONVERT_TO_NUM(ENTRY)						\
{									\
	if (!parm)							\
	{								\
		free(parm_buff);					\
		return NULL;						\
	}								\
	if (cli_is_dcm(parm))						\
		parm_buff->ENTRY = (int)(STRTOUL(parm, NULL, 10));	\
	else if (cli_is_hex(parm + 2))					\
		parm_buff->ENTRY = (int)(STRTOUL(parm, NULL, 16));	\
	else								\
	{								\
		assert(FALSE);						\
		free(parm_buff);					\
		free(parm);						\
		return NULL;						\
	}								\
	free(parm);							\
}

bool mu_rndwn_file(gd_region *reg, bool standalone);
#endif
