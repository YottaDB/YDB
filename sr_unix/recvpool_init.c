/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_inet.h"
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
#include "interlock.h"
#include "repl_msg.h"
#include "gtmsource.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	recvpool_addrs		recvpool;
GBLREF	int			recvpool_shmid;
GBLREF 	uint4			process_id;
GBLREF 	gtmrecv_options_t	gtmrecv_options;
GBLREF	gd_region		*gv_cur_region;

LITREF	char			gtm_release_name[];
LITREF	int4			gtm_release_name_len;

#define MAX_RES_TRIES		620 		/* Also defined in gvcst_init_sysops.c */

void recvpool_init(recvpool_user pool_user, boolean_t gtmrecv_startup)
{
	boolean_t	shm_created, new_ipc = FALSE;
	char		instfilename[MAX_FN_LEN + 1];
        char           	machine_name[MAX_MCNAMELEN];
	gd_region	*r_save, *reg;
	int		status, save_errno;
	union semun	semarg;
	struct semid_ds	semstat;
	struct shmid_ds	shmstat;
	repl_inst_hdr	repl_instance;
	sm_long_t	status_l;
	unix_db_info	*udi;
	unsigned int	full_len;
	sgmnt_addrs	*repl_csa;

	error_def(ERR_REPLREQRUNDOWN);
	error_def(ERR_RECVPOOLSETUP);
	error_def(ERR_NORECVPOOL);
	error_def(ERR_TEXT);

        memset(machine_name, 0, SIZEOF(machine_name));
        if (GETHOSTNAME(machine_name, MAX_MCNAMELEN, status))
                rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to get the hostname"), errno);
	if (NULL == jnlpool.jnlpool_dummy_reg)
	{
		r_save = gv_cur_region;
		mu_gv_cur_reg_init();
		reg = recvpool.recvpool_dummy_reg = gv_cur_region;
		gv_cur_region = r_save;
		assert(!gtmrecv_options.start);	/* Should not be receiver server startup as that would have attached to jnl pool */
	} else
	{	/* Have already attached to the journal pool. Use the journal pool region for the receive pool as well as they
		 * both correspond to one and the same file (replication instance file). We need to do this to ensure that an
		 * "ftok_sem_get" done with either "jnlpool.jnlpool_dummy_reg" region or "recvpool.recvpool_dummy_reg" region
		 * locks the same entity.
		 * Should have already attached to journal pool only for receiver server startup or shutdown. Assert that.
		 */
		assert(gtmrecv_options.start || gtmrecv_options.shut_down);
		reg = recvpool.recvpool_dummy_reg = jnlpool.jnlpool_dummy_reg;
	}
	udi = FILE_INFO(reg);
	if (!repl_inst_get_name(instfilename, &full_len, MAX_FN_LEN + 1, issue_rts_error))
		GTMASSERT;	/* rts_error should have been issued by repl_inst_get_name */
	assert((recvpool.recvpool_dummy_reg != jnlpool.jnlpool_dummy_reg)
		|| (reg->dyn.addr->fname_len == full_len) && !STRCMP(reg->dyn.addr->fname, instfilename));
	if (recvpool.recvpool_dummy_reg != jnlpool.jnlpool_dummy_reg)
	{	/* Fill in fields only if this is the first time this process is opening the replication instance file */
		memcpy((char *)reg->dyn.addr->fname, instfilename, full_len);
		reg->dyn.addr->fname_len = full_len;
		udi->fn = (char *)reg->dyn.addr->fname;
	}
	/* First grab ftok semaphore for replication instance file.  Once we have it locked, no one else can start up
	 * or, shut down replication for this instance. We will release ftok semaphore when initialization is done.
	 */
	if (!ftok_sem_get(recvpool.recvpool_dummy_reg, TRUE, REPLPOOL_ID, FALSE))
		rts_error(VARLSTCNT(1) ERR_RECVPOOLSETUP);
	repl_inst_read(instfilename, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
	if (INVALID_SEMID == repl_instance.recvpool_semid)
	{
		if (INVALID_SHMID != repl_instance.recvpool_shmid)
			GTMASSERT;
		if (GTMRECV != pool_user || !gtmrecv_startup)
			rts_error(VARLSTCNT(4) ERR_NORECVPOOL, 2, full_len, udi->fn);
		new_ipc = TRUE;
	} else
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
	udi->gt_sem_ctime = repl_instance.recvpool_semid_ctime;
	udi->gt_shm_ctime = repl_instance.recvpool_shmid_ctime;
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
		udi->gt_sem_ctime = semarg.buf->sem_ctime;
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
		if (-1 == (udi->shmid = recvpool_shmid = shmget(IPC_PRIVATE, gtmrecv_options.buffsize, IPC_CREAT | RWDALL)))
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
		udi->gt_shm_ctime = shmstat.shm_ctime;
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
	recvpool.upd_proc_local = (upd_proc_local_ptr_t)((sm_uc_ptr_t)recvpool.recvpool_ctl   + RECVPOOL_CTL_SIZE);
	recvpool.gtmrecv_local  = (gtmrecv_local_ptr_t)((sm_uc_ptr_t)recvpool.upd_proc_local + UPD_PROC_LOCAL_SIZE);
	recvpool.upd_helper_ctl = (upd_helper_ctl_ptr_t)((sm_uc_ptr_t)recvpool.gtmrecv_local  + GTMRECV_LOCAL_SIZE);
	recvpool.recvdata_base  = (sm_uc_ptr_t)recvpool.recvpool_ctl + RECVDATA_BASE_OFF;
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
		recvpool.recvpool_ctl->start_jnl_seqno = 0;
		recvpool.recvpool_ctl->jnl_seqno = 0;
		recvpool.recvpool_ctl->recvdata_base_off = RECVDATA_BASE_OFF;
		recvpool.recvpool_ctl->recvpool_size = gtmrecv_options.buffsize - recvpool.recvpool_ctl->recvdata_base_off;
		recvpool.recvpool_ctl->write = 0;
		recvpool.recvpool_ctl->write_wrap = recvpool.recvpool_ctl->recvpool_size;
		recvpool.recvpool_ctl->wrapped = FALSE;
		memcpy( (char *)recvpool.recvpool_ctl->recvpool_id.instfilename, instfilename, full_len);
		memcpy(recvpool.recvpool_ctl->recvpool_id.label, GDS_RPL_LABEL, GDS_LABEL_SZ);
		memcpy(recvpool.recvpool_ctl->recvpool_id.now_running, gtm_release_name, gtm_release_name_len + 1);
		assert(0 == (offsetof(recvpool_ctl_struct, start_jnl_seqno) % 8));
					/* ensure that start_jnl_seqno starts at an 8 byte boundary */
		assert(0 == offsetof(recvpool_ctl_struct, recvpool_id));
					/* ensure that the pool identifier is at the top of the pool */
		recvpool.recvpool_ctl->recvpool_id.pool_type = RECVPOOL_SEGMENT;
		memset(&recvpool.recvpool_ctl->last_valid_triple, 0, SIZEOF(repl_triple));
		memset(&recvpool.recvpool_ctl->last_rcvd_triple, 0, SIZEOF(repl_triple));
		recvpool.recvpool_ctl->max_dualsite_resync_seqno = 0;

		recvpool.upd_proc_local->upd_proc_shutdown = NO_SHUTDOWN;
		recvpool.upd_proc_local->upd_proc_shutdown_time = -1;
		recvpool.upd_proc_local->upd_proc_pid = 0;
		recvpool.upd_proc_local->upd_proc_pid_prev = 0;
		recvpool.upd_proc_local->updateresync = gtmrecv_options.updateresync;
		recvpool.gtmrecv_local->recv_serv_pid = process_id;
		assert(NULL != jnlpool.jnlpool_ctl);
		if (NULL != jnlpool.jnlpool_ctl)
			jnlpool.jnlpool_ctl->gtmrecv_pid = process_id;
		recvpool.gtmrecv_local->lastrecvd_time = -1;
		recvpool.gtmrecv_local->restart = GTMRECV_NO_RESTART;
		recvpool.gtmrecv_local->statslog = FALSE;
		recvpool.gtmrecv_local->shutdown = NO_SHUTDOWN;
		recvpool.gtmrecv_local->shutdown_time = -1;
		STRCPY(recvpool.gtmrecv_local->filter_cmd, gtmrecv_options.filter_cmd);
		recvpool.gtmrecv_local->statslog_file[0] = '\0';
		recvpool.gtmrecv_local->last_valid_remote_proto_ver = REPL_PROTO_VER_UNINITIALIZED;
		recvpool.gtmrecv_local->remote_proto_ver = REPL_PROTO_VER_UNINITIALIZED;
			/* above two fields are fully initialized when receiver server connects to the source server */
		memset(recvpool.upd_helper_ctl, 0, SIZEOF(*recvpool.upd_helper_ctl));
		SET_LATCH_GLOBAL(&recvpool.upd_helper_ctl->pre_read_lock, LOCK_AVAILABLE);
		recvpool.recvpool_ctl->initialized = TRUE;
		recvpool.recvpool_ctl->fresh_start = TRUE;
	}
	if (new_ipc)
	{	/* Flush shmid/semid changes to the instance file header if this process created the receive pool.
		 * Also update the instance file header section in the journal pool with the recvpool sem/shm ids.
		 * Before updating jnlpool fields, ensure the journal pool lock is grabbed.
		 */
		assert(NULL != jnlpool.repl_inst_filehdr);
		DEBUG_ONLY(repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;)
		assert(!repl_csa->hold_onto_crit);	/* so it is ok to invoke "grab_lock" and "rel_lock" unconditionally */
		grab_lock(jnlpool.jnlpool_dummy_reg);
		jnlpool.repl_inst_filehdr->recvpool_shmid = udi->shmid;
		jnlpool.repl_inst_filehdr->recvpool_semid = udi->semid;
		jnlpool.repl_inst_filehdr->recvpool_shmid_ctime = udi->gt_shm_ctime;
		jnlpool.repl_inst_filehdr->recvpool_semid_ctime = udi->gt_sem_ctime;
		rel_lock(jnlpool.jnlpool_dummy_reg);
		/* Flush the file header to disk so future callers of "jnlpool_init" see the jnlpool_semid and jnlpool_shmid */
		repl_inst_flush_filehdr();
	}
	/* Release control lockout and ftok semaphore now that the receive pool has been attached.
	 * The only exception is receiver server shutdown command. In this case, all these locks will be released
	 * once the receiver server shutdown is actually complete. Note that if -UPDATEONLY or -HELPERS is specified
	 * in the shutdown, we do not want to hold any of these locks.
	 */
	if ((GTMRECV != pool_user) || !gtmrecv_options.shut_down || gtmrecv_options.updateonly || gtmrecv_options.helpers)
	{
		rel_sem(RECV, RECV_POOL_ACCESS_SEM);
		if (!ftok_sem_release(recvpool.recvpool_dummy_reg, FALSE, FALSE))
			rts_error(VARLSTCNT(1) ERR_RECVPOOLSETUP);
	}
 	/* If receiver server startup, grab the options semaphore to lock out checkhealth, statslog or changelog.
	 * Ideally one should grab this before releasing the ftok and access semaphore but the issue with this is that
	 * if we do, we can cause a deadlock. Hence we do this just after releasing the ftok and access semaphore. This
	 * introduces a small window of instructions where a checkhealth, changelog, statslog etc. can sneak in right
	 * after we release the ftok and access lock but before we grab the options semaphore and attach to a receive pool
	 * that is uninitialized. This will be addressed by C9F12-002766.
	 */
	if ((GTMRECV == pool_user) && gtmrecv_options.start && (0 != grab_sem(RECV, RECV_SERV_OPTIONS_SEM)))
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
	return;
}
