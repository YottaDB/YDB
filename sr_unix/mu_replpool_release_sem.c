/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
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
#include "gtmmsg.h"
#include "gtm_sem.h"
#include "mu_rndwn_replpool.h"
#include "ftok_sems.h"
#include "anticipatory_freeze.h"

#define DO_CLNUP_AND_RETURN(SAVE_ERRNO, INSTFILENAME, INSTFILELEN, SEM_ID, FAILED_OP)	\
{														\
	gtm_putmsg(VARLSTCNT(5) ERR_REPLACCSEM, 3, SEM_ID, INSTFILELEN, INSTFILENAME);				\
	gtm_putmsg(VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT(FAILED_OP), CALLFROM, SAVE_ERRNO);			\
	return -1;												\
}

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	recvpool_addrs		recvpool;
GBLREF	gd_region		*gv_cur_region;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	boolean_t		argumentless_rundown;

error_def(ERR_REPLACCSEM);

int mu_replpool_release_sem(repl_inst_hdr_ptr_t repl_inst_filehdr, char pool_type, boolean_t remove_sem)
{
	int			save_errno, instfilelen, status, sem_id, semval;
	uint4			semnum;
	char			*instfilename;
	gd_region		*replreg;
#	ifdef DEBUG
	unix_db_info		*udi;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	assert((NULL != jnlpool.jnlpool_dummy_reg) && (jnlpool.jnlpool_dummy_reg == recvpool.recvpool_dummy_reg));
	replreg = jnlpool.jnlpool_dummy_reg;
	DEBUG_ONLY(udi = FILE_INFO(jnlpool.jnlpool_dummy_reg));
	assert(udi->grabbed_ftok_sem || jgbl.mur_rollback); /* Rollback already holds standalone access so no need for ftok lock */
	instfilename = (char *)replreg->dyn.addr->fname;
	instfilelen = replreg->dyn.addr->fname_len;
	assert((NULL != instfilename) && (0 != instfilelen) && ('\0' == instfilename[instfilelen]));
	assert((JNLPOOL_SEGMENT == pool_type) || (RECVPOOL_SEGMENT == pool_type));
	if (JNLPOOL_SEGMENT == pool_type)
	{
		sem_id = repl_inst_filehdr->jnlpool_semid;
		semval = semctl(sem_id, SRC_SERV_COUNT_SEM, GETVAL);
		/* mu_replpool_grab_sem always increments the counter semaphore. So, it should be 1 at this point. In addition,
		 * if not ONLINE ROLLBACK or RUNDOWN with Anticipatory Freeze, we wait for the counter to become zero before
		 * grabbing it. The only exception where semval can be greater than 1 is if we are ONLINE ROLLBACK or RUNDOWN -REG
		 * with anticipatory freeze scheme in effect.
		 */
		assert((1 == semval) || ((1 <= semval)
			&& (jgbl.onlnrlbk || (!jgbl.mur_rollback && !argumentless_rundown && ANTICIPATORY_FREEZE_AVAILABLE))));
		remove_sem &= (1 == semval); /* we can remove the sem if the caller intends to and the counter semaphore is 1 */
		if (0 < semval)
		{
			status = decr_sem(SOURCE, SRC_SERV_COUNT_SEM);
			if (SS_NORMAL != status)
			{
				save_errno = errno;
				DO_CLNUP_AND_RETURN(save_errno, instfilename, instfilelen, sem_id, "semop()");
			}
		} else if (-1 == semval)
		{
			save_errno = errno;
			DO_CLNUP_AND_RETURN(save_errno, instfilename, instfilelen, sem_id, "semctl()");
		}
		holds_sem[SOURCE][SRC_SERV_COUNT_SEM] = FALSE;
		assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
		assert(1 == semctl(sem_id, JNL_POOL_ACCESS_SEM, GETVAL)); /* we hold the access control semaphore */
		status = rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
		assert(SS_NORMAL == status); /* We hold it. So, we should be able to release it */
		holds_sem[SOURCE][JNL_POOL_ACCESS_SEM] = FALSE;
		/* Now that we have released the access control semaphore, see if we can remove the semaphore altogether from the
		 * system. For this, we should be holding the FTOK on the replication instance AND the counter semaphore should be
		 * 0. If we are called from mu_rndwn_repl_instance, we are guaranteed we will be holding the FTOK. But, if we are
		 * ROLLBACK (online or noonline), we need not hold the FTOK, if somebody else (possibly a source or receiver server
		 * startup command) is holding it. In that case, we don't remove the semaphore, because any process that is waiting
		 * on the access control will now get an EIDRM error which is NOT user friendly. So, just release the access
		 * control and let the waiting process do the clean up.
		 */
		if (remove_sem)
		{
			status = remove_sem_set(SOURCE);
			if (SS_NORMAL != status)
			{
				save_errno = errno;
				DO_CLNUP_AND_RETURN(save_errno, instfilename, instfilelen, sem_id, "sem_rmid()");
			}
			repl_inst_filehdr->jnlpool_semid = INVALID_SEMID;
			repl_inst_filehdr->jnlpool_semid_ctime = 0;
		}
	} else
	{
		sem_id = repl_inst_filehdr->recvpool_semid;
		for (semnum = RECV_SERV_COUNT_SEM; semnum <= UPD_PROC_COUNT_SEM; semnum++)
		{
			semval = semctl(sem_id, semnum, GETVAL);
			assert((1 == semval) || jgbl.onlnrlbk);
			remove_sem &= (1 == semval);
			if (0 < semval)
			{
				status = decr_sem(RECV, semnum);
				if (SS_NORMAL != status)
				{
					save_errno = errno;
					DO_CLNUP_AND_RETURN(save_errno, instfilename, instfilelen, sem_id, "semop()");
				}
			} else if (-1 == semval)
			{
				save_errno = errno;
				DO_CLNUP_AND_RETURN(save_errno, instfilename, instfilelen, sem_id, "semctl()");
			}
			holds_sem[RECV][semnum] = FALSE;
		}
		assert(holds_sem[RECV][RECV_POOL_ACCESS_SEM] && holds_sem[RECV][RECV_SERV_OPTIONS_SEM]);
		assert(1 == semctl(sem_id, RECV_POOL_ACCESS_SEM, GETVAL) && (1 == semctl(sem_id, RECV_SERV_OPTIONS_SEM, GETVAL)));
		status = rel_sem_immediate(RECV, RECV_SERV_OPTIONS_SEM);
		assert(SS_NORMAL == status); /* We hold it. So, we should be able to release it */
		status = rel_sem_immediate(RECV, RECV_POOL_ACCESS_SEM);
		assert(SS_NORMAL == status); /* We hold it. So, we should be able to release it */
		holds_sem[RECV][RECV_POOL_ACCESS_SEM] = FALSE;
		holds_sem[RECV][RECV_SERV_OPTIONS_SEM] = FALSE;
		/* Now that we have released the access control semaphore, see if we can remove the semaphore altogether from the
		 * system. For this, we should be holding the FTOK on the replication instance AND the counter semaphore should be
		 * 0. If we are called from mu_rndwn_repl_instance, we are guaranteed we will be holding the FTOK. But, if we are
		 * ROLLBACK (online or noonline), we need not hold the FTOK, if somebody else (possibly a source or receiver server
		 * startup command) is holding it. In that case, we don't remove the semaphore, because any process that is waiting
		 * on the access control will now get an EIDRM error which is NOT user friendly. So, just release the access
		 * control and let the waiting process do the clean up.
		 */
		if (remove_sem)
		{
			status = remove_sem_set(RECV);
			if (SS_NORMAL != status)
			{
				save_errno = errno;
				DO_CLNUP_AND_RETURN(save_errno, instfilename, instfilelen, sem_id, "sem_rmid()");
			}
			repl_inst_filehdr->recvpool_semid = INVALID_SEMID;
			repl_inst_filehdr->recvpool_semid_ctime = 0;
		}
	}
	return SS_NORMAL;
}
