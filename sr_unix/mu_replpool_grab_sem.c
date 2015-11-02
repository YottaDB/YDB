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

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	recvpool_addrs		recvpool;
GBLREF	gd_region		*gv_cur_region;

/*
 * Description:
 * 	Grab ftok semaphore on replication instance file
 *	Grab all replication semaphores for the instance (both jnlpool and recvpool)
 * 	Release ftok semaphore
 * Parameters:
 * Return Value: TRUE, if succsessful
 *	         FALSE, if fails.
 */
boolean_t mu_replpool_grab_sem(boolean_t immediate)
{
	char			instfilename[MAX_FN_LEN + 1];
	gd_region		*r_save;
	static gd_region 	*replreg;
	int			status, save_errno;
	union semun		semarg;
	struct semid_ds		semstat;
	repl_inst_hdr		repl_instance;
	unix_db_info		*udi;
	unsigned int		full_len;

	error_def(ERR_RECVPOOLSETUP);
	error_def(ERR_JNLPOOLSETUP);
	error_def(ERR_REPLFTOKSEM);
	error_def(ERR_TEXT);

	if (NULL == replreg)
	{
		r_save = gv_cur_region;
		mu_gv_cur_reg_init();
		replreg = gv_cur_region;
		gv_cur_region = r_save;
	}
	jnlpool.jnlpool_dummy_reg = replreg;
	recvpool.recvpool_dummy_reg = replreg;
	if (!repl_inst_get_name(instfilename, &full_len, MAX_FN_LEN + 1, issue_rts_error))
		GTMASSERT;	/* rts_error should have been issued by repl_inst_get_name */
	assert(full_len);
	memcpy((char *)replreg->dyn.addr->fname, instfilename, full_len);
	replreg->dyn.addr->fname_len = full_len;
	udi = FILE_INFO(replreg);
	udi->fn = (char *)replreg->dyn.addr->fname;
	if (!ftok_sem_get(replreg, TRUE, REPLPOOL_ID, immediate))
		rts_error(VARLSTCNT(4) ERR_REPLFTOKSEM, 2, full_len, instfilename);
	repl_inst_read(instfilename, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
	/*
	 * --------------------------
	 * First semaphores of jnlpool
	 * --------------------------
	 */
	if (-1 == (udi->semid = init_sem_set_source(IPC_PRIVATE, NUM_SRC_SEMS, RWDALL | IPC_CREAT)))
	{
		ftok_sem_release(replreg, TRUE, TRUE);
		rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			  RTS_ERROR_LITERAL("Error creating journal pool"), REPL_SEM_ERRNO);
	}
	semarg.val = GTM_ID;
	if (-1 == semctl(udi->semid, SOURCE_ID_SEM, SETVAL, semarg))
	{
		save_errno = errno;
		remove_sem_set(SOURCE);		/* Remove what we created */
		ftok_sem_release(replreg, TRUE, TRUE);
		rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			 RTS_ERROR_LITERAL("Error with jnlpool semctl"), save_errno);
	}
	semarg.buf = &semstat;
	if (-1 == semctl(udi->semid, 0, IPC_STAT, semarg))
	{
		save_errno = errno;
		remove_sem_set(SOURCE);		/* Remove what we created */
		ftok_sem_release(replreg, TRUE, TRUE);
		rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			 RTS_ERROR_LITERAL("Error with jnlpool semctl"), save_errno);
	}
	udi->gt_sem_ctime = semarg.buf->sem_ctime;
	status = grab_sem_all_source();
	if (0 != status)
	{
		remove_sem_set(SOURCE);		/* Remove what we created */
		ftok_sem_release(replreg, TRUE, TRUE);
		rts_error(VARLSTCNT(1) ERR_JNLPOOLSETUP);
	}
	repl_instance.jnlpool_semid = udi->semid;
	repl_instance.jnlpool_semid_ctime = udi->gt_sem_ctime;
	/*
	 * --------------------------
	 * Now semaphores of recvpool
	 * --------------------------
	 */
	assert(NUM_SRC_SEMS == NUM_RECV_SEMS);
	if (-1 == (udi->semid = init_sem_set_recvr(IPC_PRIVATE, NUM_RECV_SEMS, RWDALL | IPC_CREAT)))
	{
		remove_sem_set(SOURCE);
		ftok_sem_release(replreg, TRUE, TRUE);
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0,
			  ERR_TEXT, 2,
			  RTS_ERROR_LITERAL("Error creating recv pool"), REPL_SEM_ERRNO);
	}
	semarg.val = GTM_ID;
	if (-1 == semctl(udi->semid, RECV_ID_SEM, SETVAL, semarg))
	{
		save_errno = errno;
		remove_sem_set(SOURCE);
		remove_sem_set(RECV);
		ftok_sem_release(replreg, TRUE, TRUE);
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
			 RTS_ERROR_LITERAL("Error with recvpool semctl"), save_errno);
	}
	semarg.buf = &semstat;
	if (-1 == semctl(udi->semid, 0, IPC_STAT, semarg)) /* For creation time */
	{
		save_errno = errno;
		remove_sem_set(SOURCE);
		remove_sem_set(RECV);
		ftok_sem_release(replreg, TRUE, TRUE);
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
			 RTS_ERROR_LITERAL("Error with recvpool semctl"), save_errno);
	}
	udi->gt_sem_ctime = semarg.buf->sem_ctime;
	status = grab_sem_all_receive();
	if (0 != status)
	{
		remove_sem_set(SOURCE);
		remove_sem_set(RECV);
		ftok_sem_release(replreg, TRUE, TRUE);
		rts_error(VARLSTCNT(1) ERR_RECVPOOLSETUP);
	}
	repl_instance.recvpool_semid = udi->semid;
	repl_instance.recvpool_semid_ctime = udi->gt_sem_ctime;
	/* Initialize jnlpool.repl_inst_filehdr as it is used later by gtmrecv_fetchresync() */
	assert(NULL == jnlpool.repl_inst_filehdr);
	jnlpool.repl_inst_filehdr = (repl_inst_hdr_ptr_t)malloc(SIZEOF(repl_inst_hdr));
	memcpy(jnlpool.repl_inst_filehdr, &repl_instance, SIZEOF(repl_inst_hdr));
	/* Flush changes to the replication instance file header to disk */
	repl_inst_write(instfilename, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
	/* Now release jnlpool/recvpool ftok semaphore */
	if (!ftok_sem_release(replreg, FALSE, immediate))
	{
		remove_sem_set(SOURCE);
		remove_sem_set(RECV);
		rts_error(VARLSTCNT(4) ERR_REPLFTOKSEM, 2, full_len, instfilename);
	}
	return TRUE;
}
