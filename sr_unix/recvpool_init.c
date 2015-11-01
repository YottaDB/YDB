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
#include <sys/sem.h>
#include <errno.h>
#include <fcntl.h>
#include "gtm_unistd.h"
#include <arpa/inet.h>
#include "gtm_string.h"
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
#include "gtm_logicals.h"
#include "jnl.h"
#include "repl_sem.h"
#include "repl_shutdcode.h"
#include "io.h"
#include "do_shmat.h"
#include "trans_log_name.h"
#include "repl_instance.h"
#include "mu_gv_cur_reg_init.h"
#include "gtmmsg.h"
#include "gtm_sem.h"
#include "ipcrmid.h"
#include "ftok_sems.h"

GBLREF	recvpool_addrs		recvpool;
GBLREF	int			recvpool_shmid;
GBLREF 	uint4			process_id;
GBLREF 	gtmrecv_options_t	gtmrecv_options;
GBLREF	gd_region		*gv_cur_region;
LITREF	char			gtm_release_name[];
LITREF	int4			gtm_release_name_len;

#define MAX_RES_TRIES		620 		/* Also defined in gvcst_init_sysops.c */
#define MEGA_BOUND		(1024*1024)

void recvpool_init(recvpool_user pool_user,
		   boolean_t gtmrecv_startup,
		   boolean_t lock_opt_sem)
{
	boolean_t	shm_created, new_ipc = FALSE;
	char		instname[MAX_FN_LEN + 1];
        char           	machine_name[MAX_MCNAMELEN];
	gd_region	*r_save, *reg;
	int		status, save_errno;
	union semun	semarg;
	struct semid_ds	semstat;
	struct shmid_ds	shmstat;
	repl_inst_fmt	repl_instance;
	sm_long_t	status_l;
	unix_db_info	*udi;
	unsigned int	full_len;

	error_def(ERR_REPLREQRUNDOWN);
	error_def(ERR_REPLINSTUNDEF);
	error_def(ERR_RECVPOOLSETUP);
	error_def(ERR_NORECVPOOL);
	error_def(ERR_TEXT);

        memset(machine_name, 0, sizeof(machine_name));
        if (GETHOSTNAME(machine_name, MAX_MCNAMELEN, status))
                rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to get the hostname"), errno);
	r_save = gv_cur_region;
	mu_gv_cur_reg_init();
	reg = recvpool.recvpool_dummy_reg = gv_cur_region;
	gv_cur_region = r_save;
	udi = FILE_INFO(reg);
	if (!repl_inst_get_name(instname, &full_len, MAX_FN_LEN + 1))
		rts_error(VARLSTCNT(1) ERR_REPLINSTUNDEF);
	memcpy((char *)reg->dyn.addr->fname, instname, full_len);
	reg->dyn.addr->fname_len = full_len;
	udi->fn = (char *)reg->dyn.addr->fname;
	/*
	 * First grab ftok semaphore for replication instance file.  Once we have it locked,
	 * no one else can start up or, shut down replication for this instance.
	 * We will release ftok semaphore when initialization is done.
	 */
	if (!ftok_sem_get(recvpool.recvpool_dummy_reg, TRUE, REPLPOOL_ID, FALSE))
		rts_error(VARLSTCNT(1) ERR_RECVPOOLSETUP);
	repl_inst_get(instname, &repl_instance);
	if (INVALID_SEMID == repl_instance.recvpool_semid)
	{
		if (INVALID_SHMID != repl_instance.recvpool_shmid)
			GTMASSERT;
		if (GTMRECV != pool_user || !gtmrecv_startup)
			 rts_error(VARLSTCNT(4) ERR_NORECVPOOL, 2, full_len, udi->fn);
		new_ipc = TRUE;
	}
	else
	{
		semarg.buf = &semstat;
		if (-1 == semctl(repl_instance.recvpool_semid, 0, IPC_STAT, semarg))
		{
			ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(6) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name));
		}
		else if (semarg.buf->sem_ctime != repl_instance.recvpool_semid_ctime)
		{
			ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(10) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name),
				ERR_TEXT, 2, RTS_ERROR_TEXT("recvpool sem_ctime does not match"));
		}
		if (INVALID_SHMID == repl_instance.recvpool_shmid ||
			(-1 == shmctl(repl_instance.recvpool_shmid, IPC_STAT, &shmstat)))
		{
			ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(6) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name));
		}
		else if (shmstat.shm_ctime != repl_instance.recvpool_shmid_ctime)
		{
			ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(10) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name),
				ERR_TEXT, 2, RTS_ERROR_TEXT("recvpool shm_ctime does not match"));
		}
	}
	udi->semid = repl_instance.recvpool_semid;
	udi->sem_ctime = repl_instance.recvpool_semid_ctime;
	udi->shm_ctime = repl_instance.recvpool_shmid_ctime;
	recvpool_shmid = udi->shmid = repl_instance.recvpool_shmid;
	if (new_ipc)
	{
		assert(NUM_SRC_SEMS == NUM_RECV_SEMS);
		if (INVALID_SEMID == (udi->semid = init_sem_set_recvr(IPC_PRIVATE, NUM_RECV_SEMS, RWDALL | IPC_CREAT)))
		{
			ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0,
				  ERR_TEXT, 2,
			          RTS_ERROR_LITERAL("Error creating recv pool"), REPL_SEM_ERRNO);
		}
		/*
		 * Following will set semaphore RECV_ID_SEM value as GTM_ID.
		 * In case we have orphaned semaphore for some reason, mupip rundown will be
		 * able to identify GTM semaphores checking the value and can remove.
		 */
		semarg.val = GTM_ID;
		if (-1 == semctl(udi->semid, RECV_ID_SEM, SETVAL, semarg))
		{
			save_errno = errno;
			remove_sem_set(RECV);		/* Remove what we created */
			ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				 RTS_ERROR_LITERAL("Error with recvpool semctl"), save_errno);
		}
		/*
		 * Warning: We must read the sem_ctime using IPC_STAT after SETVAL, which changes it.
		 *	    We must NOT do any more SETVAL after this. Our design is to use
		 *	    sem_ctime as creation time of semaphore.
		 */
		semarg.buf = &semstat;
		if (-1 == semctl(udi->semid, RECV_ID_SEM, IPC_STAT, semarg))
		{
			save_errno = errno;
			remove_sem_set(RECV);		/* Remove what we created */
			ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				 RTS_ERROR_LITERAL("Error with recvpool semctl"), save_errno);
		}
		udi->sem_ctime = semarg.buf->sem_ctime;
	} else
		set_sem_set_recvr(udi->semid);
	/* First go for the access control lock */
	status = grab_sem(RECV, RECV_POOL_ACCESS_SEM);
	if (0 != status)
	{
		if (new_ipc)
			remove_sem_set(RECV);
		ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0,
			ERR_TEXT, 2, RTS_ERROR_LITERAL("Error with receive pool semaphores"), REPL_SEM_ERRNO);
	}
	shm_created = FALSE;
	if (new_ipc)
	{
		/* Create the shared memory */
#ifdef __MVS__
		if (-1 == (udi->shmid = recvpool_shmid =
			     shmget(IPC_PRIVATE, ROUND_UP(gtmrecv_options.buffsize, MEGA_BOUND), __IPC_MEGA | IPC_CREAT | RWDALL)))
#else
		if (-1 == (udi->shmid = recvpool_shmid = shmget(IPC_PRIVATE, gtmrecv_options.buffsize, IPC_CREAT | RWDALL)))
#endif
		{
			udi->shmid = recvpool_shmid = INVALID_SHMID;
			save_errno = errno;
			remove_sem_set(RECV);		/* Remove what we created */
			ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error with receive pool creation"), save_errno);
		}
		if (-1 == shmctl(udi->shmid, IPC_STAT, &shmstat))
		{
			save_errno = errno;
			if (0 != shm_rmid(udi->shmid))
				gtm_putmsg(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
					 RTS_ERROR_LITERAL("Error removing recvpool "), errno);
			remove_sem_set(RECV);		/* Remove what we created */
			ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				 RTS_ERROR_LITERAL("Error with recvpool shmctl"), save_errno);
		}
		udi->shm_ctime = shmstat.shm_ctime;
		shm_created = TRUE;
	}
	status_l = (sm_long_t)(recvpool.recvpool_ctl = (recvpool_ctl_ptr_t)do_shmat(recvpool_shmid, 0, 0));
	if (-1 == status_l)
	{
		save_errno = errno;
		if (new_ipc)
			remove_sem_set(RECV);
		else
			rel_sem_immediate(RECV, RECV_POOL_ACCESS_SEM);
		ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error with receive pool shmat"), save_errno);
	}
	if (shm_created)
		recvpool.recvpool_ctl->initialized = FALSE;
	recvpool.upd_proc_local = (upd_proc_local_ptr_t)((sm_uc_ptr_t)recvpool.recvpool_ctl + sizeof(recvpool_ctl_struct));
	recvpool.gtmrecv_local = (gtmrecv_local_ptr_t)((sm_uc_ptr_t)recvpool.upd_proc_local + sizeof(upd_proc_local_struct));
	recvpool.recvdata_base = (sm_uc_ptr_t)recvpool.recvpool_ctl + RECVDATA_BASE_OFF;
	if (GTMRECV == pool_user && gtmrecv_startup)
		recvpool.recvpool_ctl->fresh_start = FALSE;
	if (!recvpool.recvpool_ctl->initialized)
	{
		if (GTMRECV != pool_user || !gtmrecv_startup)
		{
			if (new_ipc)
				remove_sem_set(RECV);
			else
				rel_sem_immediate(RECV, RECV_POOL_ACCESS_SEM);
			ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Receive pool has not been initialized"));
		}
		/* Initialize the shared memory fields */
		QWASSIGNDW(recvpool.recvpool_ctl->start_jnl_seqno, 0);
		recvpool.recvpool_ctl->recvdata_base_off = RECVDATA_BASE_OFF;
		recvpool.recvpool_ctl->recvpool_size = gtmrecv_options.buffsize - recvpool.recvpool_ctl->recvdata_base_off;
		recvpool.recvpool_ctl->write = 0;
		recvpool.recvpool_ctl->write_wrap = recvpool.recvpool_ctl->recvpool_size;
		recvpool.recvpool_ctl->wrapped = FALSE;
		memcpy( (char *)recvpool.recvpool_ctl->recvpool_id.instname, instname, full_len);
		memcpy(recvpool.recvpool_ctl->recvpool_id.label, GDS_RPL_LABEL, GDS_LABEL_SZ);
		memcpy(recvpool.recvpool_ctl->recvpool_id.now_running, gtm_release_name, gtm_release_name_len + 1);
		assert(0 == (offsetof(recvpool_ctl_struct, start_jnl_seqno) % 8));
					/* ensure that start_jnl_seqno starts at an 8 byte boundary */
		assert(0 == offsetof(recvpool_ctl_struct, recvpool_id));
					/* ensure that the pool identifier is at the top of the pool */
		recvpool.recvpool_ctl->recvpool_id.pool_type = RECVPOOL_SEGMENT;

		recvpool.upd_proc_local->upd_proc_shutdown = NO_SHUTDOWN;
		recvpool.upd_proc_local->upd_proc_shutdown_time = -1;
		recvpool.upd_proc_local->upd_proc_pid = 0;
		recvpool.upd_proc_local->upd_proc_pid_prev = 0;
		recvpool.upd_proc_local->updateresync = gtmrecv_options.updateresync;

		recvpool.gtmrecv_local->recv_serv_pid = process_id;
		recvpool.gtmrecv_local->lastrecvd_time = -1;

		recvpool.gtmrecv_local->restart = GTMRECV_NO_RESTART;
		recvpool.gtmrecv_local->statslog = FALSE;
		recvpool.gtmrecv_local->shutdown = NO_SHUTDOWN;
		recvpool.gtmrecv_local->shutdown_time = -1;
		strcpy(recvpool.gtmrecv_local->filter_cmd, gtmrecv_options.filter_cmd);
		recvpool.gtmrecv_local->statslog_file[0] = '\0';
		recvpool.recvpool_ctl->initialized = TRUE;
		recvpool.recvpool_ctl->fresh_start = TRUE;
	}
 	/* If startup, lock out checkhealth and receiver startup */
	if (GTMRECV == pool_user && lock_opt_sem && 0 != grab_sem(RECV, RECV_SERV_OPTIONS_SEM))
	{
		save_errno = REPL_SEM_ERRNO;
		if (new_ipc)
			remove_sem_set(RECV);
		else
			rel_sem_immediate(RECV, RECV_POOL_ACCESS_SEM);
		ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
 	  		  RTS_ERROR_LITERAL("Error with receive pool options semaphore"), save_errno);
	}
	/* Release control lockout now that it is initialized */
	rel_sem(RECV, RECV_POOL_ACCESS_SEM);
	repl_instance.recvpool_shmid = udi->shmid;
	repl_instance.recvpool_semid = udi->semid;
	repl_instance.recvpool_semid_ctime = udi->sem_ctime;
	repl_instance.recvpool_shmid_ctime = udi->shm_ctime;
	repl_inst_put(instname, &repl_instance);
	if (!ftok_sem_release(recvpool.recvpool_dummy_reg, FALSE, FALSE))
		rts_error(VARLSTCNT(1) ERR_RECVPOOLSETUP);
	return;
}
