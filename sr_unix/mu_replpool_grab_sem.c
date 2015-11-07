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

#include "gtm_ipc.h"
#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gtm_inet.h"
#include "gtm_fcntl.h"

#include <sys/sem.h>
#include <errno.h>
#include <stddef.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "gtmrecv.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtm_logicals.h"
#include "jnl.h"
#include "repl_sem.h"
#include "repl_shutdcode.h"
#include "io.h"
#include "trans_log_name.h"
#include "repl_instance.h"
#include "mu_gv_cur_reg_init.h"
#include "gtmmsg.h"
#include "gtm_sem.h"
#include "mu_rndwn_replpool.h"
#include "ftok_sems.h"
#include "util.h"
#include "anticipatory_freeze.h"

#define REMOVE_SEM_SET(SEM_CREATED, POOL_TYPE)	/* Assumes 'sem_created_ptr' is is already declared */		\
{														\
	if (SEM_CREATED)											\
		remove_sem_set(JNLPOOL_SEGMENT == POOL_TYPE ? SOURCE : RECV);					\
	*sem_created_ptr = SEM_CREATED = FALSE;									\
}

#define DO_CLNUP_AND_RETURN(SAVE_ERRNO, SEM_CREATED, POOL_TYPE, INSTFILENAME, INSTFILELEN, SEM_ID, FAILED_OP)	\
{														\
	REMOVE_SEM_SET(SEM_CREATED, REPLPOOL_ID);								\
	gtm_putmsg(VARLSTCNT(5) ERR_REPLACCSEM, 3, SEM_ID, INSTFILELEN, INSTFILENAME);				\
	gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT(FAILED_OP), CALLFROM, SAVE_ERRNO);			\
	return -1;												\
}

#define RELEASE_ALREADY_HELD_SEMAPHORE(SEM_SET, SEM_NUM)							\
{														\
	int		status, lcl_save_errno;									\
														\
	assert(holds_sem[SEM_SET][SEM_NUM]);									\
	status = rel_sem_immediate(SEM_SET, SEM_NUM);								\
	if (-1 == status)											\
	{													\
		lcl_save_errno = errno;										\
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("semop()"), CALLFROM, lcl_save_errno);	\
	}													\
}

#define DECR_ALREADY_INCREMENTED_SEMAPHORE(SEM_SET, SEM_NUM)							\
{														\
	int		status, lcl_save_errno;									\
														\
	assert(holds_sem[SEM_SET][SEM_NUM]);									\
	status = decr_sem(SEM_SET, SEM_NUM);									\
	if (-1 == status)											\
	{													\
		lcl_save_errno = errno;										\
		gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("semop()"), CALLFROM, lcl_save_errno);	\
	}													\
}

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	recvpool_addrs		recvpool;
GBLREF	gd_region		*gv_cur_region;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	boolean_t		argumentless_rundown;

error_def(ERR_JNLPOOLSETUP);
error_def(ERR_RECVPOOLSETUP);
error_def(ERR_REPLACCSEM);
error_def(ERR_REPLFTOKSEM);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

/*
 * Description:
 * 	Grab ftok semaphore on replication instance file
 *	Grab all replication semaphores for the instance (both jnlpool and recvpool)
 * 	Release ftok semaphore
 * Parameters:
 * Return Value: SS_NORMAL, if succsessful
 *	         -1, if fails.
 */
int mu_replpool_grab_sem(repl_inst_hdr_ptr_t repl_inst_filehdr, char pool_type, boolean_t *sem_created_ptr, boolean_t immediate)
{
	int			status, save_errno, sem_id, semval, semnum, instfilelen;
	time_t			sem_ctime;
	boolean_t		sem_created, force_increment;
	char			*instfilename;
	union semun		semarg;
	struct semid_ds		semstat;
	gd_region		*replreg;
	DEBUG_ONLY(unix_db_info	*udi;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	*sem_created_ptr = sem_created = FALSE; /* assume semaphore not created by default */
	force_increment = (jgbl.onlnrlbk || (!jgbl.mur_rollback && !argumentless_rundown && ANTICIPATORY_FREEZE_AVAILABLE));
	/* First ensure that the caller has grabbed the ftok semaphore on the replication instance file */
	assert((NULL != jnlpool.jnlpool_dummy_reg) && (jnlpool.jnlpool_dummy_reg == recvpool.recvpool_dummy_reg));
	replreg = jnlpool.jnlpool_dummy_reg;
	DEBUG_ONLY(udi = FILE_INFO(jnlpool.jnlpool_dummy_reg));
	assert(udi->grabbed_ftok_sem); /* the caller should have grabbed ftok semaphore */
	instfilename = (char *)replreg->dyn.addr->fname;
	instfilelen = replreg->dyn.addr->fname_len;
	assert((NULL != instfilename) && (0 != instfilelen) && ('\0' == instfilename[instfilelen]));
	assert((JNLPOOL_SEGMENT == pool_type) || (RECVPOOL_SEGMENT == pool_type));
	if (JNLPOOL_SEGMENT == pool_type)
	{
		sem_id = repl_inst_filehdr->jnlpool_semid;
		sem_ctime = repl_inst_filehdr->jnlpool_semid_ctime;
	}
	else
	{
		sem_id = repl_inst_filehdr->recvpool_semid;
		sem_ctime = repl_inst_filehdr->recvpool_semid_ctime;
	}
	semarg.buf = &semstat;
	if ((INVALID_SEMID == sem_id) || (-1 == semctl(sem_id, 0, IPC_STAT, semarg)) || (sem_ctime != semarg.buf->sem_ctime))
	{	/* Semaphore doesn't exist. Create new ones */
		if (JNLPOOL_SEGMENT == pool_type)
		{
			repl_inst_filehdr->jnlpool_semid = INVALID_SEMID; /* Invalidate previous semid in file header */
			repl_inst_filehdr->jnlpool_semid_ctime = 0;
			sem_id = init_sem_set_source(IPC_PRIVATE, NUM_SRC_SEMS, RWDALL | IPC_CREAT);
		} else
		{
			repl_inst_filehdr->recvpool_semid = INVALID_SEMID; /* Invalidate previous semid in file header */
			repl_inst_filehdr->recvpool_semid_ctime = 0;
			sem_id = init_sem_set_recvr(IPC_PRIVATE, NUM_RECV_SEMS, RWDALL | IPC_CREAT);
		}
		if (INVALID_SEMID == sem_id)
		{
			save_errno = errno;
			DO_CLNUP_AND_RETURN(save_errno, sem_created, pool_type, instfilename, instfilelen, sem_id, "semget()");
		}
		*sem_created_ptr = sem_created = TRUE;
		semarg.val = GTM_ID;
		if (-1 == semctl(sem_id, SOURCE_ID_SEM, SETVAL, semarg))
		{
			save_errno = errno;
			DO_CLNUP_AND_RETURN(save_errno, sem_created, pool_type, instfilename, instfilelen, sem_id, "semctl()");
		}
		semarg.buf = &semstat;
		if (-1 == semctl(sem_id, DB_CONTROL_SEM, IPC_STAT, semarg))
		{
			save_errno = errno;
			DO_CLNUP_AND_RETURN(save_errno, sem_created, pool_type, instfilename, instfilelen, sem_id, "semctl()");
		}
		sem_ctime = semarg.buf->sem_ctime;
	} else if (JNLPOOL_SEGMENT == pool_type)
		set_sem_set_src(sem_id);
	else
		set_sem_set_recvr(sem_id);
	/* Semaphores are setup. Grab them */
	if (JNLPOOL_SEGMENT == pool_type)
	{
		assert(!jgbl.onlnrlbk || !immediate);
		if (!immediate)
			status = grab_sem(SOURCE, JNL_POOL_ACCESS_SEM); /* want to wait in case of online rollback */
		else
			status = grab_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
		if (SS_NORMAL != status)
		{
			save_errno = errno;
			DO_CLNUP_AND_RETURN(save_errno, sem_created, pool_type, instfilename, instfilelen, sem_id, "semop()");
		}
		semval = semctl(sem_id, SRC_SERV_COUNT_SEM, GETVAL);
		if (-1 == semval)
		{
			save_errno = errno;
			RELEASE_ALREADY_HELD_SEMAPHORE(SOURCE, JNL_POOL_ACCESS_SEM);
			DO_CLNUP_AND_RETURN(save_errno, sem_created, pool_type, instfilename, instfilelen, sem_id, "semctl()");
		}
		if (0 < semval && !force_increment)
		{
			RELEASE_ALREADY_HELD_SEMAPHORE(SOURCE, JNL_POOL_ACCESS_SEM);
			util_out_print("Replpool semaphore (id = !UL) for replication instance !AD is in use by another process.",
						TRUE, sem_id, instfilelen, instfilename);
			REMOVE_SEM_SET(sem_created, pool_type);
			return -1;
		}
		status = incr_sem(SOURCE, SRC_SERV_COUNT_SEM);
		if (SS_NORMAL != status)
		{
			save_errno = errno;
			RELEASE_ALREADY_HELD_SEMAPHORE(SOURCE, JNL_POOL_ACCESS_SEM);
			assert(FALSE); /* we hold it, so there is no reason why we cannot release it */
			DO_CLNUP_AND_RETURN(save_errno, sem_created, pool_type, instfilename, instfilelen, sem_id, "semop()");
		}
		holds_sem[SOURCE][SRC_SERV_COUNT_SEM] = TRUE;
		repl_inst_filehdr->jnlpool_semid = sem_id;
		repl_inst_filehdr->jnlpool_semid_ctime = sem_ctime;
	}
	else
	{
		if (!immediate)
			status = grab_sem(RECV, RECV_POOL_ACCESS_SEM);
		else
			status = grab_sem_immediate(RECV, RECV_POOL_ACCESS_SEM);
		if (SS_NORMAL != status)
		{
			save_errno = errno;
			DO_CLNUP_AND_RETURN(save_errno, sem_created, pool_type, instfilename, instfilelen, sem_id, "semop()");
		}
		if (!immediate)
			status = grab_sem(RECV, RECV_SERV_OPTIONS_SEM);
		else
			status = grab_sem_immediate(RECV, RECV_SERV_OPTIONS_SEM);
		if (SS_NORMAL != status)
		{
			save_errno = errno;
			RELEASE_ALREADY_HELD_SEMAPHORE(RECV, RECV_POOL_ACCESS_SEM);
			DO_CLNUP_AND_RETURN(save_errno, sem_created, pool_type, instfilename, instfilelen, sem_id, "semop()");
		}
		assert(RECV_SERV_COUNT_SEM + 1 == UPD_PROC_COUNT_SEM);
		for (semnum = RECV_SERV_COUNT_SEM; semnum <= UPD_PROC_COUNT_SEM; semnum++)
		{
			semval = semctl(sem_id, semnum, GETVAL);
			if (-1 == semval)
			{
				save_errno = errno;
				RELEASE_ALREADY_HELD_SEMAPHORE(RECV, RECV_POOL_ACCESS_SEM);
				RELEASE_ALREADY_HELD_SEMAPHORE(RECV, RECV_SERV_OPTIONS_SEM);
				DO_CLNUP_AND_RETURN(save_errno, sem_created, pool_type, instfilename, instfilelen,
							sem_id, "semctl()");
			}
			if ((0 < semval) && !jgbl.onlnrlbk)
			{
				RELEASE_ALREADY_HELD_SEMAPHORE(RECV, RECV_POOL_ACCESS_SEM);
				RELEASE_ALREADY_HELD_SEMAPHORE(RECV, RECV_SERV_OPTIONS_SEM);
				if (UPD_PROC_COUNT_SEM == semnum)
				{	/* Need to decrement RECV_SERV_COUNT_SEM before returning to the caller since we have
					 * incremented it before
					 */
					DECR_ALREADY_INCREMENTED_SEMAPHORE(RECV, RECV_SERV_COUNT_SEM);
				}
				util_out_print("Replpool semaphore (id = !UL) for replication instance !AD is in use by "
						"another process.", TRUE, sem_id, instfilelen, instfilename);
				REMOVE_SEM_SET(sem_created, pool_type);
				return -1;
			}
			status = incr_sem(RECV, semnum);
			if (SS_NORMAL != status)
			{
				save_errno = errno;
				if (UPD_PROC_COUNT_SEM == semnum)
				{	/* Need to decrement RECV_SERV_COUNT_SEM before returning to the caller since we have
					 * incremented it before
					 */
					DECR_ALREADY_INCREMENTED_SEMAPHORE(RECV, RECV_SERV_COUNT_SEM);
				}
				DO_CLNUP_AND_RETURN(save_errno, sem_created, pool_type, instfilename, instfilelen,
							sem_id, "semop()");
			}
			holds_sem[RECV][semnum] = TRUE;
		}
		repl_inst_filehdr->recvpool_semid = sem_id;
		repl_inst_filehdr->recvpool_semid_ctime = sem_ctime;
	}
	return SS_NORMAL;
}
