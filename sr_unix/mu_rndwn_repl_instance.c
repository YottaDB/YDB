/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_inet.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"

#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <errno.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "iosp.h"
#include "gtm_stdio.h"
#include "gtmio.h"
#include "gtm_string.h"
#include "repl_instance.h"
#include "gtm_logicals.h"
#include "repl_sem.h"
#include "mu_rndwn_replpool.h"
#include "mu_rndwn_repl_instance.h"
#include "mu_gv_cur_reg_init.h"
#include "gtm_sem.h"
#include "gtmmsg.h"
#include "gtm_ipc.h"
#include "eintr_wrappers.h"
#include "ftok_sems.h"
#include "mu_rndwn_all.h"
#include "util.h"
#ifdef UNIX
#include "ipcrmid.h"	/* for sem_rmid() prototype */
#endif

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	recvpool_addrs		recvpool;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gd_region		*ftok_sem_reg;

#define TMP_BUF_LEN     50

/*
 * This will rundown a replication instance journal (and receiver) pool.
 *	Input Parameter:
 *		replpool_id of the instance. Instance file name must be null terminated in replpool_id.
 * Returns :
 *	TRUE,  if successful.
 *	FALSE, otherwise.
 */
boolean_t mu_rndwn_repl_instance(replpool_identifier *replpool_id, boolean_t immediate, boolean_t rndwn_both_pools)
{
	boolean_t		jnlpool_stat = TRUE, recvpool_stat = TRUE;
	char			*instfilename, shmid_buff[TMP_BUF_LEN];
	gd_region		*r_save;
	repl_inst_hdr		repl_instance;
	static	gd_region	*reg = NULL;
	struct semid_ds		semstat;
	struct shmid_ds		shmstat;
	union semun		semarg;
	uchar_ptr_t		ret_ptr;
	unix_db_info		*udi;
	int			save_errno;

	error_def(ERR_MUJPOOLRNDWNSUC);
	error_def(ERR_MURPOOLRNDWNSUC);
	error_def(ERR_MUJPOOLRNDWNFL);
	error_def(ERR_MURPOOLRNDWNFL);
	error_def(ERR_SEMREMOVED);
	error_def(ERR_REPLACCSEM);
	error_def(ERR_SYSCALL);

	if (NULL == reg)
	{
		r_save = gv_cur_region;
		mu_gv_cur_reg_init();
		reg = gv_cur_region;
		gv_cur_region = r_save;
	}
	jnlpool.jnlpool_dummy_reg = reg;
	recvpool.recvpool_dummy_reg = reg;
	instfilename = replpool_id->instfilename;
	reg->dyn.addr->fname_len = strlen(instfilename);
	assert(0 == instfilename[reg->dyn.addr->fname_len]);
	memcpy((char *)reg->dyn.addr->fname, instfilename, reg->dyn.addr->fname_len + 1);
	udi = FILE_INFO(reg);
	udi->fn = (char *)reg->dyn.addr->fname;
	/* Lock replication instance using ftok semaphore */
	if (!ftok_sem_get(reg, TRUE, REPLPOOL_ID, immediate))
		return FALSE;
	repl_inst_read(instfilename, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
	semarg.buf = &semstat;
	assert(rndwn_both_pools || JNLPOOL_SEGMENT == replpool_id->pool_type || RECVPOOL_SEGMENT == replpool_id->pool_type);
	if (rndwn_both_pools || (JNLPOOL_SEGMENT == replpool_id->pool_type))
	{	/* --------------------------
		 * First rundown Journal pool
		 * --------------------------
		 */
		if (INVALID_SEMID != repl_instance.jnlpool_semid)
			if ((-1 == semctl(repl_instance.jnlpool_semid, 0, IPC_STAT, semarg)) ||
					(semarg.buf->sem_ctime != repl_instance.jnlpool_semid_ctime))
				repl_instance.jnlpool_semid = INVALID_SEMID;
		if (INVALID_SHMID != repl_instance.jnlpool_shmid)
			if ((-1 == shmctl(repl_instance.jnlpool_shmid, IPC_STAT, &shmstat)) ||
					(shmstat.shm_ctime != repl_instance.jnlpool_shmid_ctime))
				repl_instance.jnlpool_shmid = INVALID_SHMID;
		if (INVALID_SHMID != repl_instance.jnlpool_shmid)
		{
			replpool_id->pool_type = JNLPOOL_SEGMENT;
			jnlpool_stat = mu_rndwn_replpool(replpool_id, repl_instance.jnlpool_semid, repl_instance.jnlpool_shmid);
			ret_ptr = i2asc((uchar_ptr_t)shmid_buff, repl_instance.jnlpool_shmid);
			*ret_ptr = '\0';
			if (rndwn_both_pools)
				gtm_putmsg(VARLSTCNT(6) (jnlpool_stat ? ERR_MUJPOOLRNDWNSUC : ERR_MUJPOOLRNDWNFL),
					4, LEN_AND_STR(shmid_buff), LEN_AND_STR(replpool_id->instfilename));
		} else if (INVALID_SEMID != repl_instance.jnlpool_semid)
		{
			if (0 == sem_rmid(repl_instance.jnlpool_semid))
			{	/* note that shmid_buff used here is actually a buffer to hold semid (not shmid) */
				ret_ptr = i2asc((uchar_ptr_t)shmid_buff, repl_instance.jnlpool_semid);
				*ret_ptr = '\0';
				gtm_putmsg(VARLSTCNT(9) ERR_MUJPOOLRNDWNSUC, 4, LEN_AND_STR(shmid_buff),
					LEN_AND_STR(replpool_id->instfilename), ERR_SEMREMOVED, 1, repl_instance.jnlpool_semid);
			} else
			{
				save_errno = errno;
				gtm_putmsg(VARLSTCNT(13) ERR_REPLACCSEM, 3, repl_instance.jnlpool_semid,
					RTS_ERROR_STRING(instfilename), ERR_SYSCALL, 5, RTS_ERROR_LITERAL("jnlpool sem_rmid()"),
					CALLFROM, save_errno);
			}
			/* Note that jnlpool_stat is not set to FALSE in case sem_rmid() fails above. This is because the
			 * journal pool is anyway not present and it is safer to reset the sem/shmids in the instance file.
			 * The only thing this might cause is a stranded semaphore but that is considered better than getting
			 * errors due to not resetting instance file.
			 */
		}
		if (jnlpool_stat)	/* Reset instance file for jnlpool info */
			repl_inst_jnlpool_reset();
	}
	if (rndwn_both_pools || (RECVPOOL_SEGMENT == replpool_id->pool_type))
	{	/* --------------------------
		 * Now rundown Receivpool
		 * --------------------------
		 */
		if (INVALID_SEMID != repl_instance.recvpool_semid)
			if ((-1 == semctl(repl_instance.recvpool_semid, 0, IPC_STAT, semarg)) ||
					(semarg.buf->sem_ctime != repl_instance.recvpool_semid_ctime))
				repl_instance.recvpool_semid = INVALID_SEMID;
		if (INVALID_SHMID != repl_instance.recvpool_shmid)
			if ((-1 == shmctl(repl_instance.recvpool_shmid, IPC_STAT, &shmstat)) ||
					(shmstat.shm_ctime != repl_instance.recvpool_shmid_ctime))
				repl_instance.recvpool_shmid = INVALID_SHMID;
		if (INVALID_SHMID != repl_instance.recvpool_shmid)
		{
			replpool_id->pool_type = RECVPOOL_SEGMENT;
			recvpool_stat = mu_rndwn_replpool(replpool_id, repl_instance.recvpool_semid, repl_instance.recvpool_shmid);
			ret_ptr = i2asc((uchar_ptr_t)shmid_buff, repl_instance.recvpool_shmid);
			*ret_ptr = '\0';
			if (rndwn_both_pools)
				gtm_putmsg(VARLSTCNT(6) (recvpool_stat ? ERR_MURPOOLRNDWNSUC : ERR_MURPOOLRNDWNFL),
					4, LEN_AND_STR(shmid_buff), LEN_AND_STR(replpool_id->instfilename));
		} else if (INVALID_SEMID != repl_instance.recvpool_semid)
		{
			if (0 == sem_rmid(repl_instance.recvpool_semid))
			{	/* note that shmid_buff used here is actually a buffer to hold semid (not shmid) */
				ret_ptr = i2asc((uchar_ptr_t)shmid_buff, repl_instance.recvpool_semid);
				*ret_ptr = '\0';
				gtm_putmsg(VARLSTCNT(9) ERR_MURPOOLRNDWNSUC, 4, LEN_AND_STR(shmid_buff),
					LEN_AND_STR(replpool_id->instfilename), ERR_SEMREMOVED, 1, repl_instance.recvpool_semid);
			} else
			{
				save_errno = errno;
				gtm_putmsg(VARLSTCNT(13) ERR_REPLACCSEM, 3, repl_instance.recvpool_semid,
					RTS_ERROR_STRING(instfilename), ERR_SYSCALL, 5, RTS_ERROR_LITERAL("recvpool sem_rmid()"),
					CALLFROM, save_errno);
			}
			/* Note that recvpool_stat is not set to FALSE in case sem_rmid() fails above. This is because the
			 * journal pool is anyway not present and it is safer to reset the sem/shmids in the instance file.
			 * The only thing this might cause is a stranded semaphore but that is considered better than getting
			 * errors due to not resetting instance file.
			 */
		}
		if (recvpool_stat)	/* Reset instance file for recvpool info */
			repl_inst_recvpool_reset();
	}
	/* Release replication instance ftok semaphore lock */
	if (!ftok_sem_release(reg, TRUE, immediate))
		return FALSE;
	return (jnlpool_stat && recvpool_stat);
}
