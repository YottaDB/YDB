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
#include "gtm_inet.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_fcntl.h"

#include "gtm_ipc.h"
#include <sys/shm.h>
#include <errno.h>
#include <stddef.h>
#include <sys/sem.h>

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
#include "gtm_sem.h"
#include "do_shmat.h"	/* for do_shmat() prototype */

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;

#define	RELEASE_SEM(SEM_GRABBED, POOL_TYPE, STATUS, SEM_ID, INSTFILENAME)					\
{														\
	if (SEM_GRABBED)											\
	{													\
		if (JNLPOOL_SEGMENT == POOL_TYPE)								\
			STATUS = rel_sem(SOURCE, JNL_POOL_ACCESS_SEM);	/* journal pool */			\
		else												\
			STATUS = rel_sem(RECV, RECV_POOL_ACCESS_SEM);	/* receive pool */			\
		SEM_GRABBED = FALSE;										\
		if (STATUS)											\
			gtm_putmsg(VARLSTCNT(10) ERR_REPLACCSEM, 3, SEM_ID, RTS_ERROR_STRING(INSTFILENAME),     \
			ERR_TEXT, 2, LEN_AND_LIT("Error releasing semaphore"), errno);                          \
	}													\
}

#define CLNUP_REPLPOOL_ACC_SEM(SEM_ID, INSTFILENAME, SEM_GRABBED, SEM_CREATED, POOL_TYPE, STATUS)		\
{														\
	RELEASE_SEM(SEM_GRABBED ,POOL_TYPE, STATUS, SEM_ID, INSTFILENAME);					\
	if (SEM_CREATED)											\
	{													\
		if (-1 == semctl(sem_id, 0, IPC_RMID))								\
			gtm_putmsg(VARLSTCNT(10) ERR_REPLACCSEM, 3, SEM_ID, RTS_ERROR_STRING(INSTFILENAME),	\
				ERR_TEXT, 2, LEN_AND_LIT("Error removing semaphore"), errno);			\
	}													\
}

#define MU_RNDWN_REPLPOOL_RETURN(RETVAL, SEM_ID, INSTFILENAME, SEM_GRABBED, SEM_CREATED, POOL_TYPE, STATUS)	\
{														\
	shmdt((void *)start_addr);										\
	CLNUP_REPLPOOL_ACC_SEM(SEM_ID, INSTFILENAME, SEM_GRABBED, SEM_CREATED, POOL_TYPE, STATUS);		\
	return RETVAL;												\
}

LITREF char             gtm_release_name[];
LITREF int4             gtm_release_name_len;

boolean_t mu_rndwn_replpool(replpool_identifier *replpool_id, int sem_id, int shm_id)
{
	int			semval, status, save_errno;
	char			*instfilename, pool_type;
	boolean_t		sem_created = FALSE, sem_grabbed = FALSE;
	sm_uc_ptr_t		start_addr;
	struct semid_ds		semstat;
	struct shmid_ds		shm_buf;
	union semun		semarg;
	unix_db_info		*udi;
	sgmnt_addrs		*csa;

	error_def(ERR_REPLPOOLINST);
	error_def(ERR_REPLACCSEM);
	error_def(ERR_SYSCALL);
	error_def(ERR_TEXT);

	if (INVALID_SHMID == shm_id)
		return TRUE;
	semarg.buf = &semstat;
	instfilename = replpool_id->instfilename;
	pool_type = replpool_id->pool_type;
	assert((JNLPOOL_SEGMENT == pool_type) || (RECVPOOL_SEGMENT == pool_type));
	if (INVALID_SEMID == sem_id || (-1 == semctl(sem_id, 0, IPC_STAT, semarg)))
	{
		if (JNLPOOL_SEGMENT == pool_type)
			sem_id = init_sem_set_source(IPC_PRIVATE, NUM_SRC_SEMS, RWDALL | IPC_CREAT);
		else
			sem_id = init_sem_set_recvr(IPC_PRIVATE, NUM_RECV_SEMS, RWDALL | IPC_CREAT);
		if (INVALID_SEMID == sem_id)
		{
			save_errno = errno;
			gtm_putmsg(VARLSTCNT(5) ERR_REPLACCSEM, 3, sem_id, RTS_ERROR_STRING(instfilename));
			gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semget()"), CALLFROM, save_errno);
			return FALSE;
		}
		sem_created = TRUE;
	} else if (JNLPOOL_SEGMENT == pool_type)
		set_sem_set_src(sem_id);
	else
		set_sem_set_recvr(sem_id);
	if (JNLPOOL_SEGMENT == pool_type)
		status = grab_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);	/* journal pool */
	else
		status = grab_sem_immediate(RECV, RECV_POOL_ACCESS_SEM);	/* receive pool */
	if (SS_NORMAL != status)
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_REPLACCSEM, 3, sem_id, RTS_ERROR_STRING(instfilename));
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semget()"), CALLFROM, save_errno);
		CLNUP_REPLPOOL_ACC_SEM(sem_id, instfilename, sem_grabbed, sem_created, pool_type, status);
		return FALSE;
	}
	sem_grabbed = TRUE;
	if (JNLPOOL_SEGMENT == pool_type)
		semval = semctl(sem_id, SRC_SERV_COUNT_SEM, GETVAL);
	else
		semval = semctl(sem_id, RECV_SERV_COUNT_SEM, GETVAL);
	if (-1 == semval)
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_REPLACCSEM, 3, sem_id, RTS_ERROR_STRING(instfilename));
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semget()"), CALLFROM, save_errno);
		CLNUP_REPLPOOL_ACC_SEM(sem_id, instfilename, sem_grabbed, sem_created, pool_type, status);
		return FALSE;
	}
	if (0 < semval)
	{
		util_out_print("Replpool semaphore (id = !UL) for replication instance !AD is in use by another process.",
				TRUE, sem_id, LEN_AND_STR(instfilename));
		CLNUP_REPLPOOL_ACC_SEM(sem_id, instfilename, sem_grabbed, sem_created, pool_type, status);
		return FALSE;
	}
	if (-1 == shmctl(shm_id, IPC_STAT, &shm_buf))
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_REPLPOOLINST, 3, shm_id, RTS_ERROR_STRING(instfilename));
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("shmctl()"), CALLFROM, save_errno);
		CLNUP_REPLPOOL_ACC_SEM(sem_id, instfilename, sem_grabbed, sem_created, pool_type, status);
		return FALSE;
	}
	if (0 != shm_buf.shm_nattch) /* It must be zero before I attach to it */
	{
		util_out_print("Replpool segment (id = !UL) for replication instance !AD is in use by another process.",
				TRUE, shm_id, LEN_AND_STR(instfilename));
		CLNUP_REPLPOOL_ACC_SEM(sem_id, instfilename, sem_grabbed, sem_created, pool_type, status);
		return FALSE;
	}
	if (-1 == (sm_long_t)(start_addr = (sm_uc_ptr_t) do_shmat(shm_id, 0, 0)))
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_REPLPOOLINST, 3, shm_id, RTS_ERROR_STRING(instfilename));
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("shmat()"), CALLFROM, save_errno);
		CLNUP_REPLPOOL_ACC_SEM(sem_id, instfilename, sem_grabbed, sem_created, pool_type, status);
		return FALSE;
	}
	/* assert that the identifiers are at the top of replpool control structure */
	assert(0 == offsetof(jnlpool_ctl_struct, jnlpool_id));
	assert(0 == offsetof(recvpool_ctl_struct, recvpool_id));
	memcpy((void *)replpool_id, (void *)start_addr, SIZEOF(replpool_identifier));
	if (memcmp(replpool_id->label, GDS_RPL_LABEL, GDS_LABEL_SZ - 1))
	{
		if (!memcmp(replpool_id->label, GDS_RPL_LABEL, GDS_LABEL_SZ - 3))
			util_out_print(
				"Incorrect version for the replpool segment (id = !UL) belonging to replication instance !AD",
					TRUE, shm_id, LEN_AND_STR(instfilename));
		else
			util_out_print("Incorrect replpool format for the segment (id = !UL) belonging to replication instance !AD",
					TRUE, shm_id, LEN_AND_STR(instfilename));
		MU_RNDWN_REPLPOOL_RETURN(FALSE, sem_id, instfilename, sem_grabbed, sem_created, pool_type, status);
	}
	if (memcmp(replpool_id->now_running, gtm_release_name, gtm_release_name_len + 1))
	{
		util_out_print("Attempt to access with version !AD, while already using !AD for replpool segment (id = !UL)"
				" belonging to replication instance !AD.", TRUE, gtm_release_name_len, gtm_release_name,
				LEN_AND_STR(replpool_id->now_running), shm_id, LEN_AND_STR(instfilename));
		MU_RNDWN_REPLPOOL_RETURN(FALSE, sem_id, instfilename, sem_grabbed, sem_created, pool_type, status);
	}
	if (JNLPOOL_SEGMENT == pool_type)
	{	/* Initialize variables to simulate a "jnlpool_init". This is required by "repl_inst_flush_jnlpool" called below */
		jnlpool_ctl = jnlpool.jnlpool_ctl = (jnlpool_ctl_ptr_t)start_addr;
		assert(NULL != jnlpool.jnlpool_dummy_reg);
		udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
		csa = &udi->s_addrs;
		csa->critical = (mutex_struct_ptr_t)((sm_uc_ptr_t)jnlpool.jnlpool_ctl + JNLPOOL_CTL_SIZE);
			/* secshr_db_clnup uses this relationship */
		assert(jnlpool.jnlpool_ctl->filehdr_off);
		assert(jnlpool.jnlpool_ctl->srclcl_array_off > jnlpool.jnlpool_ctl->filehdr_off);
		assert(jnlpool.jnlpool_ctl->sourcelocal_array_off > jnlpool.jnlpool_ctl->srclcl_array_off);
		/* Initialize "jnlpool.repl_inst_filehdr" and related fields as "repl_inst_flush_jnlpool" relies on that */
		jnlpool.repl_inst_filehdr = (repl_inst_hdr_ptr_t)((sm_uc_ptr_t)jnlpool.jnlpool_ctl
									+ jnlpool.jnlpool_ctl->filehdr_off);
		jnlpool.gtmsrc_lcl_array = (gtmsrc_lcl_ptr_t)((sm_uc_ptr_t)jnlpool.jnlpool_ctl
									+ jnlpool.jnlpool_ctl->srclcl_array_off);
		jnlpool.gtmsource_local_array = (gtmsource_local_ptr_t)((sm_uc_ptr_t)jnlpool.jnlpool_ctl
										+ jnlpool.jnlpool_ctl->sourcelocal_array_off);
		repl_inst_flush_jnlpool(FALSE);	/* flush instance file header and gtmsrc_lcl structures from jnlpool to disk */
		/* Now that jnlpool has been flushed and there is going to be no journal pool, reset "jnlpool.repl_inst_filehdr"
		 * as otherwise other routines (e.g. "repl_inst_recvpool_reset") are affected by whether this is NULL or not.
		 */
		jnlpool.jnlpool_ctl = NULL;
		jnlpool_ctl = NULL;
		jnlpool.repl_inst_filehdr = NULL;
		jnlpool.gtmsrc_lcl_array = NULL;
		jnlpool.gtmsource_local_array = NULL;
		jnlpool.jnldata_base = NULL;
	}
	if (-1 == shmdt((caddr_t)start_addr))
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_REPLPOOLINST, 3, shm_id, RTS_ERROR_STRING(instfilename));
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("shmdt()"), CALLFROM, save_errno);
		CLNUP_REPLPOOL_ACC_SEM(sem_id, instfilename, sem_grabbed, sem_created, pool_type, status);
		return FALSE;
	}
	if (0 != shm_rmid(shm_id))
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_REPLPOOLINST, 3, shm_id, RTS_ERROR_STRING(instfilename));
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("shm_ctl()"), CALLFROM, save_errno);
		MU_RNDWN_REPLPOOL_RETURN(FALSE, sem_id, instfilename, sem_grabbed, sem_created, pool_type, status);
	}
	RELEASE_SEM(sem_grabbed, pool_type, status, sem_id, instfilename);
	if (0 != sem_rmid(sem_id))
	{
		save_errno = errno;
		gtm_putmsg(VARLSTCNT(5) ERR_REPLACCSEM, 3, sem_id, RTS_ERROR_STRING(instfilename));
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("semget()"), CALLFROM, save_errno);
		return FALSE;
	}
	return TRUE;
}
