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
#include <errno.h>
#include <fcntl.h>
#include "gtm_unistd.h"
#include <arpa/inet.h>
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include <sys/sem.h>
#include <sys/shm.h>
#include <stddef.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtm_logicals.h"
#include "mutex.h"
#include "jnl.h"
#include "repl_sem.h"
#include "gtmimagename.h"
#include "io.h"
#include "do_shmat.h"
#include "repl_instance.h"
#include "mu_gv_cur_reg_init.h"
#include "gtmmsg.h"
#include "gtm_sem.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	sm_uc_ptr_t		jnldata_base;
GBLREF	uint4			process_id;
GBLREF	gd_region		*gv_cur_region;
GBLREF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl;
GBLREF 	gtmsource_options_t	gtmsource_options;
GBLREF	boolean_t		pool_init;
GBLREF	unsigned short		dollar_tlevel;
GBLREF	uint4			process_id;
GBLREF	seq_num			seq_num_zero;
GBLREF  enum gtmImageTypes      image_type;
LITREF	char			gtm_release_name[];
LITREF	int4			gtm_release_name_len;

static	gd_region		jnlpool_dummy_reg;
static	gd_segment		jnlpool_dummy_seg;
static	file_control		jnlpool_dummy_fc;
static	unix_db_info		jnlpool_dummy_udi;
static	sgmnt_addrs		*jnlpool_dummy_sa;

#define MEGA_BOUND		1024*1024
#define MAX_RES_TRIES		620 		/* Also defined in gvcst_init_sysops.c */

void jnlpool_init(jnlpool_user pool_user,
		  boolean_t gtmsource_startup,
		  boolean_t *jnlpool_initialized)
{
	boolean_t	shm_created, new_ipc = FALSE;
        char           	machine_name[MAX_MCNAMELEN];
	gd_region	*r_save, *reg;
	int		status, lcnt, save_errno;
	int		semflgs;
	union semun	semarg;
	struct semid_ds	semstat;
	struct shmid_ds	shmstat;
	repl_inst_fmt	repl_instance;
	sm_long_t	status_l;
	unsigned int	full_len;
	mutex_spin_parms_ptr_t	jnlpool_mutex_spin_parms;
	unix_db_info	*udi;
	sgmnt_addrs	*sa;
	gd_segment	*seg;

	error_def(ERR_REPLREQRUNDOWN);
	error_def(ERR_REPLINSTUNDEF);
	error_def(ERR_JNLPOOLSETUP);
	error_def(ERR_NOJNLPOOL);
	error_def(ERR_TEXT);

        memset(machine_name, 0, sizeof(machine_name));
        if (GETHOSTNAME(machine_name, MAX_MCNAMELEN, status))
                rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to get the hostname"), errno);
	r_save = gv_cur_region;
	mu_gv_cur_reg_init();
	jnlpool.jnlpool_dummy_reg = reg = gv_cur_region;
	gv_cur_region = r_save;
	ASSERT_IN_RANGE(MIN_RN_LEN, sizeof(JNLPOOL_DUMMY_REG_NAME) - 1, MAX_RN_LEN);
	memcpy(reg->rname, JNLPOOL_DUMMY_REG_NAME, sizeof(JNLPOOL_DUMMY_REG_NAME) - 1);
	reg->rname_len = sizeof(JNLPOOL_DUMMY_REG_NAME) - 1;
	reg->rname[reg->rname_len] = 0;
	udi = FILE_INFO(reg);
	sa = &udi->s_addrs;
	seg = reg->dyn.addr;
	if (!repl_inst_get_name((char *)seg->fname, &full_len, sizeof(seg->fname)))
	{
		gtm_putmsg(VARLSTCNT(1) ERR_REPLINSTUNDEF);
		rts_error(VARLSTCNT(1) ERR_JNLPOOLSETUP);
	}
	udi->fn = (char *)seg->fname;
	seg->fname_len = full_len;
	semarg.buf = &semstat;
	/*
	 * First grab ftok semaphore for journal pool instance file. Once we have it locked no one else can
	 * continue to start or, rundown on this replication instance. We will release ftok semaphore,
	 * once we are done with creating journalpool or, done with jnlpool_init.
	 */
	get_lock_jnlpool_ftok_sems(TRUE, FALSE);
	repl_inst_get((char *)seg->fname, &repl_instance);
	if (0 == repl_instance.jnlpool_semid)
	{
		if (0 != repl_instance.jnlpool_shmid)
			GTMASSERT;
		/* we will create new journal pool. None exits for this instance */
		new_ipc = TRUE;
		if (GTMSOURCE != pool_user || !gtmsource_startup)
			 rts_error(VARLSTCNT(4) ERR_NOJNLPOOL, 2, full_len, udi->fn);
	}
	else
	{
		if (0 == repl_instance.jnlpool_shmid)
			GTMASSERT;
		if (-1 == semctl(repl_instance.jnlpool_semid, 0, IPC_STAT, semarg))
			rts_error(VARLSTCNT(6) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name));
		else if (semarg.buf->sem_ctime != repl_instance.jnlpool_semid_ctime)
			rts_error(VARLSTCNT(9) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name),
				ERR_TEXT, 2, RTS_ERROR_TEXT("jnlpool sem_ctime does not match"));
		if (-1 == shmctl(repl_instance.jnlpool_shmid, IPC_STAT, &shmstat))
			rts_error(VARLSTCNT(6) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name));
		else if (shmstat.shm_ctime != repl_instance.jnlpool_shmid_ctime)
			rts_error(VARLSTCNT(9) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name),
				ERR_TEXT, 2, RTS_ERROR_TEXT("jnlpool shm_ctime does not match"));
		/* Journalpool exists. We will attach to it. */
	}
	udi->semid = repl_instance.jnlpool_semid;
	udi->shmid = repl_instance.jnlpool_shmid;
	udi->sem_ctime = repl_instance.jnlpool_semid_ctime;
	udi->shm_ctime = repl_instance.jnlpool_shmid_ctime;
	semflgs = RWDALL;
	if (new_ipc)
	{
		semflgs |= IPC_CREAT;
		assert(NUM_SRC_SEMS == NUM_RECV_SEMS);
		if (-1 == (udi->semid = init_sem_set_source(IPC_PRIVATE, NUM_SRC_SEMS, semflgs)))
		{
			rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				  RTS_ERROR_LITERAL("Error creating journal pool"), REPL_SEM_ERRNO);
		}
		if (-1 == semctl(udi->semid, 0, IPC_STAT, semarg))
			rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				 RTS_ERROR_LITERAL("Error with jnlpool semctl"), errno);
		udi->sem_ctime = semarg.buf->sem_ctime;
	} else
		set_sem_set_src(udi->semid); /* repl_sem.c has some functions which needs some static variable to have the id */
	for (status = -1, lcnt = 0;  status != SS_NORMAL;  lcnt++)
	{
		/* The purpose of this loop is to deal with possibility
	 	 * that the semaphores may be deleted as they are attached */
		status = grab_sem(SOURCE, JNL_POOL_ACCESS_SEM);
		if ((SS_NORMAL != status)
				&& (((EINVAL != errno) && (EIDRM != errno) && (EINTR != errno)) || (MAX_RES_TRIES < lcnt)))
			rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				  RTS_ERROR_LITERAL("Error with journal pool access semaphore"), REPL_SEM_ERRNO);
		if (GTMSOURCE == pool_user && gtmsource_startup)
		{
			/* Get the option lock */
			status = grab_sem(SOURCE, SRC_SERV_OPTIONS_SEM);
			if ((SS_NORMAL != status)
					&& (((EINVAL != errno) && (EIDRM != errno) && (EINTR != errno)) || (MAX_RES_TRIES < lcnt)))
				rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0,
					  ERR_TEXT, 2,
				  	  RTS_ERROR_LITERAL("Error with journal pool option semaphore"), REPL_SEM_ERRNO);

			/* Get the server count semaphore */
			status = grab_sem_immediate(SOURCE, SRC_SERV_COUNT_SEM);
			if (SS_NORMAL != status && EAGAIN == errno)
			{
				rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
				rel_sem_immediate(SOURCE, SRC_SERV_OPTIONS_SEM);
				rts_error(VARLSTCNT(6) ERR_JNLPOOLSETUP, 0,
					  ERR_TEXT, 2, RTS_ERROR_LITERAL("Source Server already exists"));
			}
			if ((SS_NORMAL != status)
					&& (((EINVAL != errno) && (EIDRM != errno) && (EINTR != errno)) || (MAX_RES_TRIES < lcnt)))
				rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0,
					  ERR_TEXT, 2,
				  	  RTS_ERROR_LITERAL("Error with journal pool server count semaphore"), REPL_SEM_ERRNO);
		}
	}
	shm_created = FALSE;
	if (new_ipc)
	{
		/* Create the shared memory */
#if __MVS__
		if (-1 == (udi->shmid = shmget(IPC_PRIVATE,
						ROUND_UP(gtmsource_options.buffsize, MEGA_BOUND), __IPC_MEGA | IPC_CREAT | RWDALL)))
#else
		if (-1 == (udi->shmid = shmget(IPC_PRIVATE, gtmsource_options.buffsize, IPC_CREAT | RWDALL)))
#endif
			rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Error with journal pool creation"), errno);
		if (-1 == shmctl(udi->shmid, IPC_STAT, &shmstat))
			rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				 RTS_ERROR_LITERAL("Error with jnlpool shmctl"), errno);
		udi->shm_ctime = shmstat.shm_ctime;
		shm_created = TRUE;
	}
	status_l = (sm_long_t)(jnlpool.jnlpool_ctl = (jnlpool_ctl_ptr_t)do_shmat(udi->shmid, 0, 0));
	if (-1 == status_l)
		rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("Error with journal pool shmat"), errno);
	sa->critical = (mutex_struct_ptr_t)((sm_uc_ptr_t)jnlpool.jnlpool_ctl + sizeof(jnlpool_ctl_struct));
	jnlpool_mutex_spin_parms = (mutex_spin_parms_ptr_t)((sm_uc_ptr_t)sa->critical + CRIT_SPACE);
	sa->nl = (node_local_ptr_t)
					((sm_uc_ptr_t)sa->critical + CRIT_SPACE + sizeof(mutex_spin_parms_struct));
	if (shm_created)
		memset(sa->nl, 0, sizeof(node_local)); /* Make sa->nl->glob_sec_init FALSE */
	sa->now_crit = FALSE;
	jnlpool.gtmsource_local = (gtmsource_local_ptr_t)((sm_uc_ptr_t)sa->nl + sizeof(node_local));
	jnldata_base = jnlpool.jnldata_base = (sm_uc_ptr_t)jnlpool.jnlpool_ctl + JNLDATA_BASE_OFF;
	jnlpool_ctl = jnlpool.jnlpool_ctl;
	if (GTMSOURCE == pool_user && gtmsource_startup)
	{
		jnlpool.gtmsource_local->gtmsource_pid = 0;
		*jnlpool_initialized = FALSE;
	}
	if (!sa->nl->glob_sec_init)
	{
		if (GTMSOURCE != pool_user || !gtmsource_startup)
			rts_error(VARLSTCNT(6) ERR_JNLPOOLSETUP, 0,
					ERR_TEXT, 2, RTS_ERROR_LITERAL("Journal pool has not been initialized"));
		/* Initialize the shared memory fields. */
		/* Start_jnl_seqno (and jnl_seqno, read_jnl_seqno) need region shared mem to be setup. */
		jnlpool_ctl->jnldata_base_off = JNLDATA_BASE_OFF;
		jnlpool_ctl->jnlpool_size = gtmsource_options.buffsize - jnlpool_ctl->jnldata_base_off;
		assert((jnlpool_ctl->jnlpool_size & ~JNL_WRT_END_MASK) == 0);
		jnlpool_ctl->write = 0;
		jnlpool_ctl->lastwrite_len = 0;
		QWASSIGNDW(jnlpool_ctl->early_write_addr, 0);
		QWASSIGNDW(jnlpool_ctl->write_addr, 0);
		memcpy(jnlpool_ctl->jnlpool_id.instname, seg->fname, seg->fname_len);
		jnlpool_ctl->jnlpool_id.instname[seg->fname_len] = '\0';
		memcpy(jnlpool_ctl->jnlpool_id.label, GDS_RPL_LABEL, GDS_LABEL_SZ);
		memcpy(jnlpool_ctl->jnlpool_id.now_running, gtm_release_name, gtm_release_name_len + 1);
		assert(0 == (offsetof(jnlpool_ctl_struct, start_jnl_seqno) % 8));
					/* ensure that start_jnl_seqno starts at an 8 byte boundary */
		assert(0 == offsetof(jnlpool_ctl_struct, jnlpool_id));
					/* ensure that the pool identifier is at the top of the pool */
		jnlpool_ctl->jnlpool_id.pool_type = JNLPOOL_SEGMENT;
#ifdef DEBUG
		r_save = gv_cur_region; /* Setup gv_cur_region for LOCK_HIST */
		gv_cur_region = reg;
#endif
		gtm_mutex_init(reg, NUM_CRIT_ENTRY, FALSE);
#ifdef DEBUG
		gv_cur_region = r_save; /* Restore gv_cur_region */
#endif
		jnlpool_mutex_spin_parms->mutex_hard_spin_count = MUTEX_HARD_SPIN_COUNT;
		jnlpool_mutex_spin_parms->mutex_sleep_spin_count = MUTEX_SLEEP_SPIN_COUNT;
		jnlpool_mutex_spin_parms->mutex_spin_sleep_mask = MUTEX_SPIN_SLEEP_MASK;
		QWASSIGNDW(jnlpool.gtmsource_local->read_addr, 0);
		jnlpool.gtmsource_local->read = 0;
		jnlpool.gtmsource_local->read_state = READ_POOL;
		jnlpool.gtmsource_local->mode = gtmsource_options.mode;
		QWASSIGN(jnlpool.gtmsource_local->lastsent_jnl_seqno, seq_num_zero); /* 0 indicates nothing has been sent yet */
		jnlpool.gtmsource_local->lastsent_time = -1;
		jnlpool.gtmsource_local->statslog = FALSE;
		jnlpool.gtmsource_local->shutdown = FALSE;
		jnlpool.gtmsource_local->shutdown_time = -1;
		jnlpool.gtmsource_local->secondary_inet_addr = gtmsource_options.sec_inet_addr;
		jnlpool.gtmsource_local->secondary_port = gtmsource_options.secondary_port;
		strcpy(jnlpool.gtmsource_local->secondary, gtmsource_options.secondary_host);
		strcpy(jnlpool.gtmsource_local->filter_cmd, gtmsource_options.filter_cmd);
		strcpy(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file);
		jnlpool.gtmsource_local->statslog_file[0] = '\0';
		sa->nl->glob_sec_init = TRUE;
		*jnlpool_initialized = TRUE;
		/* we initialized jnlpool for the first time, so write it to the instance file
		 */
		repl_instance.jnlpool_semid = udi->semid;
		repl_instance.jnlpool_shmid = udi->shmid;
		repl_instance.jnlpool_semid_ctime = udi->sem_ctime;
		repl_instance.jnlpool_shmid_ctime = udi->shm_ctime;
		repl_inst_put((char *)seg->fname, &repl_instance);
	}
	temp_jnlpool_ctl->jnlpool_size = jnlpool_ctl->jnlpool_size;
	/* Release control lockout now that it is initialized.
	 * Source Server will release the control lockout only after
	 * other fields shared with GTM processes are initialized */
	if (GTMSOURCE != pool_user || !gtmsource_startup)
	{
		if (0 != (save_errno = rel_sem(SOURCE, JNL_POOL_ACCESS_SEM)))
			rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error in rel_sem"), save_errno);
		rel_jnlpool_ftok_sems(FALSE, FALSE);

	}
	pool_init = TRUE; /* This is done for properly setting the updata_disable flag by active/passive
				source server in jnl_file_open */
	return;
}

void jnlpool_detach(void)
{
	error_def(ERR_REPLWARN);

	if (TRUE == pool_init)
	{
		if (-1 == shmdt((caddr_t)jnlpool_ctl))
			rts_error(VARLSTCNT(5) ERR_REPLWARN, 2, RTS_ERROR_LITERAL("Could not detach from journal pool"), errno);
		jnlpool_ctl = NULL;
		jnlpool.jnlpool_ctl = NULL;
		pool_init = FALSE;
	}

}
