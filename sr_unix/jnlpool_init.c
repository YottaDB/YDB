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
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_inet.h"
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
#include "gtmrecv.h"
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
#include "ipcrmid.h"
#include "ftok_sems.h"
#include "is_proc_alive.h"
#include "repl_shutdcode.h"
#include "send_msg.h"
#include "heartbeat_timer.h"
#include "lockconst.h"
#include "anticipatory_freeze.h"
#include "have_crit.h"
#include "gtmsource_srv_latch.h"
#include "util.h"			/* For OUT_BUFF_SIZE */

GBLREF	jnlpool_addrs				jnlpool;
GBLREF	recvpool_addrs				recvpool;
GBLREF	jnlpool_ctl_ptr_t			jnlpool_ctl;
GBLREF	sm_uc_ptr_t				jnldata_base;
GBLREF	uint4					process_id;
GBLREF	gd_region				*gv_cur_region;
GBLREF	jnlpool_ctl_ptr_t			temp_jnlpool_ctl;
GBLREF	gtmsource_options_t			gtmsource_options;
GBLREF	boolean_t				pool_init;
GBLREF	seq_num					seq_num_zero;
GBLREF	enum gtmImageTypes			image_type;
GBLREF	node_local_ptr_t			locknl;
GBLREF	uint4					log_interval;
GBLREF	boolean_t				is_updproc;
GBLREF	uint4					mutex_per_process_init_pid;
GBLREF	repl_conn_info_t			*this_side, *remote_side;
GBLREF	int4					strm_index;
GBLREF	is_anticipatory_freeze_needed_t		is_anticipatory_freeze_needed_fnptr;
GBLREF	set_anticipatory_freeze_t		set_anticipatory_freeze_fnptr;
GBLREF	err_ctl					merrors_ctl;
GBLREF	jnl_gbls_t				jgbl;

#ifdef DEBUG
GBLREF		boolean_t	is_updhelper;
#endif

LITREF	char			gtm_release_name[];
LITREF	int4			gtm_release_name_len;

error_def(ERR_ACTIVATEFAIL);
error_def(ERR_JNLPOOLBADSLOT);
error_def(ERR_JNLPOOLSETUP);
error_def(ERR_NOJNLPOOL);
error_def(ERR_PRIMARYISROOT);
error_def(ERR_PRIMARYNOTROOT);
error_def(ERR_REPLINSTNMSAME);
error_def(ERR_REPLINSTNOHIST);
error_def(ERR_REPLINSTSECNONE);
error_def(ERR_REPLINSTSEQORD);
error_def(ERR_REPLREQROLLBACK);
error_def(ERR_REPLREQRUNDOWN);
error_def(ERR_REPLWARN);
error_def(ERR_SRCSRVEXISTS);
error_def(ERR_SRCSRVNOTEXIST);
error_def(ERR_SRCSRVTOOMANY);
error_def(ERR_TEXT);

#define REMOVE_OR_RELEASE_SEM(NEW_IPC)										\
{														\
	if (NEW_IPC)												\
		remove_sem_set(SOURCE);										\
	else													\
		rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);							\
}

#define	DETACH_AND_REMOVE_SHM_AND_SEM										\
{														\
	if (new_ipc)												\
	{													\
		assert(!IS_GTM_IMAGE);	/* Since "gtm_putmsg" is done below ensure it is never GT.M */		\
		if (NULL != jnlpool.jnlpool_ctl)								\
		{												\
			if (-1 == shmdt((caddr_t)jnlpool.jnlpool_ctl))						\
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_REPLWARN, 2,			\
					RTS_ERROR_LITERAL("Could not detach from journal pool"), errno);	\
			jnlpool_ctl = NULL;									\
			jnlpool.jnlpool_ctl = NULL;								\
			jnlpool.repl_inst_filehdr = NULL;							\
			jnlpool.gtmsrc_lcl_array = NULL;							\
			jnlpool.gtmsource_local_array = NULL;							\
			jnlpool.jnldata_base = NULL;								\
			pool_init = FALSE;									\
		}												\
		assert(INVALID_SHMID != udi->shmid);								\
		if (0 != shm_rmid(udi->shmid))									\
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,		\
				 RTS_ERROR_LITERAL("Error removing jnlpool "), errno);				\
		remove_sem_set(SOURCE);										\
	}													\
}

#define	CHECK_SLOT(gtmsourcelocal_ptr)												\
{																\
	if ((GTMSOURCE_DUMMY_STATE != gtmsourcelocal_ptr->gtmsource_state) || (0 != gtmsourcelocal_ptr->gtmsource_pid))		\
	{	/* Slot is in an out-of-design situation. Send an operator log message with enough debugging detail */		\
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLBADSLOT, 5,							\
			 LEN_AND_STR((char *)gtmsourcelocal_ptr->secondary_instname),						\
			gtmsourcelocal_ptr->gtmsource_pid, gtmsourcelocal_ptr->gtmsource_state,					\
			gtmsourcelocal_ptr->gtmsrc_lcl_array_index);								\
	}															\
}

void jnlpool_init(jnlpool_user pool_user, boolean_t gtmsource_startup, boolean_t *jnlpool_creator)
{
	boolean_t		new_ipc, is_src_srvr, slot_needs_init, reset_gtmsrclcl_info, hold_onto_ftok_sem, srv_alive;
	boolean_t		skip_locks;
	char			machine_name[MAX_MCNAMELEN], instfilename[MAX_FN_LEN + 1], scndry_msg[OUT_BUFF_SIZE];
	gd_region		*r_save, *reg;
	int			status, save_errno;
	int4			index;
	union semun		semarg;
	struct semid_ds		semstat;
	struct shmid_ds		shmstat;
	repl_inst_hdr		repl_instance;
	sm_long_t		status_l;
	unsigned int		full_len;
	mutex_spin_parms_ptr_t	jnlpool_mutex_spin_parms;
	unix_db_info		*udi;
	sgmnt_addrs		*csa;
	gd_segment		*seg;
	gtmsrc_lcl_ptr_t	gtmsrclcl_ptr;
	gtmsource_local_ptr_t	gtmsourcelocal_ptr, reuse_slot_ptr;
	uint4			gtmsource_pid, gtmrecv_pid;
	gtmsource_state_t	gtmsource_state;
	seq_num			reuse_slot_seqnum, instfilehdr_seqno;
	repl_histinfo		last_histinfo;
	jnlpool_ctl_ptr_t	tmp_jnlpool_ctl;
	DEBUG_ONLY(int4		semval;)
	DEBUG_ONLY(boolean_t	sem_created = FALSE;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(gtmsource_startup == gtmsource_options.start);
	skip_locks = (gtmsource_options.setfreeze && (gtmsource_options.freezeval == FALSE)) ||
			gtmsource_options.showfreeze;
	memset(machine_name, 0, SIZEOF(machine_name));
	if (GETHOSTNAME(machine_name, MAX_MCNAMELEN, status))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to get the hostname"), errno);
	if (NULL == recvpool.recvpool_dummy_reg)
	{
		r_save = gv_cur_region;
		mu_gv_cur_reg_init();
		jnlpool.jnlpool_dummy_reg = reg = gv_cur_region;
		gv_cur_region = r_save;
		ASSERT_IN_RANGE(MIN_RN_LEN, SIZEOF(JNLPOOL_DUMMY_REG_NAME) - 1, MAX_RN_LEN);
		MEMCPY_LIT(reg->rname, JNLPOOL_DUMMY_REG_NAME);
		reg->rname_len = STR_LIT_LEN(JNLPOOL_DUMMY_REG_NAME);
		reg->rname[reg->rname_len] = 0;
	} else
	{	/* Have already attached to the receive pool. Use the receive pool region for the journal pool as well as they
		 * both correspond to one and the same file (replication instance file). We need to do this to ensure that an
		 * "ftok_sem_get" done with either "jnlpool.jnlpool_dummy_reg" region or "recvpool.recvpool_dummy_reg" region
		 * locks the same entity.
		 */
		assert(is_updproc || ((GTMRELAXED == pool_user) && is_updhelper));
		reg = jnlpool.jnlpool_dummy_reg = recvpool.recvpool_dummy_reg;
	}
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	seg = reg->dyn.addr;
	assert(!udi->s_addrs.hold_onto_crit); /* so that we can do unconditional grab_locks and rel_locks */
	assertpro(repl_inst_get_name(instfilename, &full_len, MAX_FN_LEN + 1, issue_rts_error));  /* rts_error should have been
												   * issued by repl_inst_get_name */
	assert((recvpool.recvpool_dummy_reg != jnlpool.jnlpool_dummy_reg)
		|| (seg->fname_len == full_len) && !STRCMP(seg->fname, instfilename));
	if (recvpool.recvpool_dummy_reg != jnlpool.jnlpool_dummy_reg)
	{	/* Fill in fields only if this is the first time this process is opening the replication instance file */
		memcpy((char *)seg->fname, instfilename, full_len);
		udi->fn = (char *)seg->fname;
		seg->fname_len = full_len;
		seg->fname[full_len] = '\0';
	}
	/* First grab ftok semaphore for replication instance file.  Once we have it locked, no one else can start up
	 * or shut down replication for this instance. We will release ftok semaphore when initialization is done.
	 */
	if (!ftok_sem_get(jnlpool.jnlpool_dummy_reg, TRUE, REPLPOOL_ID, FALSE))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JNLPOOLSETUP);
	repl_inst_read(udi->fn, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
	is_src_srvr = (GTMSOURCE == pool_user);
	/* If caller is source server and secondary instance name has been specified check if it is different from THIS instance */
	if (is_src_srvr && gtmsource_options.instsecondary)
	{
		if (0 == STRCMP(repl_instance.inst_info.this_instname, gtmsource_options.secondary_instname))
		{
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_REPLINSTNMSAME, 2,
					 LEN_AND_STR((char *)repl_instance.inst_info.this_instname));
		}
	}
	new_ipc = FALSE;
	if (INVALID_SEMID == repl_instance.jnlpool_semid)
	{	/* First process to do "jnlpool_init". Create the journal pool. */
		assertpro(INVALID_SHMID == repl_instance.jnlpool_shmid);
		/* Source server startup is the only command that can create the journal pool. Check that. */
		if (!is_src_srvr || !gtmsource_options.start)
		{
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			if (GTMRELAXED == pool_user)
				return;
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOJNLPOOL, 2, full_len, udi->fn);
		}
		if (repl_instance.crash)
		{
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			SNPRINTF(scndry_msg, OUT_BUFF_SIZE, "Instance file header has crash field set to TRUE");
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_REPLREQROLLBACK, 2, full_len, udi->fn, ERR_TEXT, 2,
					LEN_AND_STR(scndry_msg));
		}
		DEBUG_ONLY(sem_created = TRUE);
		new_ipc = TRUE;
		assert(NUM_SRC_SEMS == NUM_RECV_SEMS);
		if (INVALID_SEMID == (udi->semid = init_sem_set_source(IPC_PRIVATE, NUM_SRC_SEMS, RWDALL | IPC_CREAT)))
		{
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error creating journal pool semaphore"), errno);
		}
		/* Following will set semaphore SOURCE_ID_SEM value as GTM_ID. In case we have orphaned semaphore
		 * for some reason, mupip rundown will be able to identify GTM semaphores checking the value and can remove.
		 */
		semarg.val = GTM_ID;
		if (-1 == semctl(udi->semid, SOURCE_ID_SEM, SETVAL, semarg))
		{
			save_errno = errno;
			remove_sem_set(SOURCE);		/* Remove what we created */
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error with jnlpool semctl SETVAL"), save_errno);
		}
		/* Warning: We must read the sem_ctime using IPC_STAT after SETVAL, which changes it. We must NOT do any
		 * more SETVAL after this. Our design is to use sem_ctime as creation time of semaphore.
		 */
		semarg.buf = &semstat;
		if (-1 == semctl(udi->semid, SOURCE_ID_SEM, IPC_STAT, semarg))
		{
			save_errno = errno;
			remove_sem_set(SOURCE);		/* Remove what we created */
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error with jnlpool semctl IPC_STAT"), save_errno);
		}
		udi->gt_sem_ctime = semarg.buf->sem_ctime;
	} else
	{	/* find create time of semaphore from the file header and check if the id is reused by others */
		assert(repl_instance.crash || jgbl.mur_rollback);
		semarg.buf = &semstat;
		if (-1 == semctl(repl_instance.jnlpool_semid, DB_CONTROL_SEM, IPC_STAT, semarg))
		{
			save_errno = errno;
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			SNPRINTF(scndry_msg, OUT_BUFF_SIZE, "Error with semctl on Journal Pool SEMID (%d)",
					repl_instance.jnlpool_semid);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_REPLREQROLLBACK, 2, full_len, udi->fn,
					ERR_TEXT, 2, LEN_AND_STR(scndry_msg), save_errno);
		} else if (semarg.buf->sem_ctime != repl_instance.jnlpool_semid_ctime)
		{
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			SNPRINTF(scndry_msg, OUT_BUFF_SIZE, "Creation time for Journal Pool SEMID (%d) is %d; Expected %d",
					repl_instance.jnlpool_semid, semarg.buf->sem_ctime, repl_instance.jnlpool_semid_ctime);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_REPLREQROLLBACK, 2, full_len, udi->fn, ERR_TEXT, 2,
					LEN_AND_STR(scndry_msg));
		}
		udi->semid = repl_instance.jnlpool_semid;
		udi->gt_sem_ctime = repl_instance.jnlpool_semid_ctime;
		set_sem_set_src(udi->semid); /* repl_sem.c has some functions which needs some static variable to have the id */
	}
	assert((INVALID_SEMID != udi->semid) && (0 != udi->gt_sem_ctime));
	assert(!udi->grabbed_access_sem);
	if (!skip_locks)
	{
		status = grab_sem(SOURCE, JNL_POOL_ACCESS_SEM);
		if (SS_NORMAL != status)
		{
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error with journal pool access semaphore"), errno);
		}
		udi->grabbed_access_sem = TRUE;
		udi->counter_acc_incremented = TRUE;
	}
	if (INVALID_SHMID == repl_instance.jnlpool_shmid)
	{	/* We have an INVALID shmid in the file header. There are three ways this can happen
		 *
		 * 1. A rollback started off with either no journal pool or rundown'd an existing journal pool and created new
		 *    semaphores, but got killed in the middle. At this point, if a new source server starts up, it will notice
		 *    a valid usable semid, but will find an invalid shmid.
		 *
		 * 2. A rollback started off with either no journal pool or rundown'd an existing journal pool and created new
		 *    semaphores. Before it goes to mur_close_files, lets say a source server started. It will acquire the ftok
		 *    semaphore, but will be waiting for the access control semaphore which rollback is holding. Rollback, on the
		 *    other hand, will see if the ftok semaphore is available BEFORE removing the semaphores from the system. But,
		 *    since source server is holding the ftok, Rollback, will not remove the access control semaphore. But, would
		 *    just let go of them and exit (repl_instance.file_corrupt can be either TRUE or FALSE depending on whether
		 *    Rollback completed successfully or not).
		 *
		 * 3. A fresh startup.
		 */
		/* Ensure that NO one has yet incremented the SRC_SERV_COUNT_SEM (as implied by all the 3 cases above) */
		assert(0 == (semval = semctl(udi->semid, SRC_SERV_COUNT_SEM, GETVAL))); /* semval = number of processes attached */
		new_ipc = TRUE; /* need to create new IPC */
	} else if (-1 == shmctl(repl_instance.jnlpool_shmid, IPC_STAT, &shmstat))
	{	/* shared memory ID was removed form the system by an IPCRM command or we have a permission issue (or such) */
		save_errno = errno;
		REMOVE_OR_RELEASE_SEM(new_ipc);
		ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
		SNPRINTF(scndry_msg, OUT_BUFF_SIZE, "Error with semctl on Journal Pool SHMID (%d)", repl_instance.jnlpool_shmid);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_REPLREQROLLBACK, 2, full_len, udi->fn, ERR_TEXT, 2,
				LEN_AND_STR(scndry_msg), save_errno);
	} else if (shmstat.shm_ctime != repl_instance.jnlpool_shmid_ctime)
	{	/* shared memory was possibly reused (causing shm_ctime and jnlpool_shmid_ctime to be different. We can't rely
		 * on the shmid as it could be connected to a valid instance file in a different environment. Create new IPCs
		 */
		new_ipc = TRUE; /* need to create new IPC */
	} else
	{
		udi->shmid = repl_instance.jnlpool_shmid;
		udi->gt_shm_ctime = repl_instance.jnlpool_shmid_ctime;
	}
	/* Source server startup is the only command that can create the journal pool. Check that. */
	if (new_ipc && (!is_src_srvr || !gtmsource_options.start))
	{
		REMOVE_OR_RELEASE_SEM(new_ipc);
		ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
		if (GTMRELAXED == pool_user)
			return;
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOJNLPOOL, 2, full_len, udi->fn);
	}
	if (repl_instance.file_corrupt)
	{	/* Indicates that a prior rollback was killed and so requires a re-run. It is also possible this process started
		 * waiting on the semaphores during a concurrent rollback and so has a stale file header values. Read the instance
		 * file header again to see if the file_corrupt field is still TRUE.
		 */
		repl_inst_read(udi->fn, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
		assert((udi->shmid == repl_instance.jnlpool_shmid) && (udi->gt_shm_ctime == repl_instance.jnlpool_shmid_ctime));
		assert(sem_created || ((udi->semid == repl_instance.jnlpool_semid) && (udi->gt_sem_ctime ==
											repl_instance.jnlpool_semid_ctime)));
		if (repl_instance.file_corrupt)
		{
			REMOVE_OR_RELEASE_SEM(new_ipc);
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			SNPRINTF(scndry_msg, OUT_BUFF_SIZE, "Instance file header has file_corrupt field set to TRUE");
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_REPLREQROLLBACK, 2, full_len, udi->fn, ERR_TEXT, 2,
					LEN_AND_STR(scndry_msg));
		}
	}
	if (new_ipc)
	{	/* create new shared memory */
		if (-1 == (udi->shmid = shmget(IPC_PRIVATE, gtmsource_options.buffsize, RWDALL | IPC_CREAT)))
		{
			udi->shmid = INVALID_SHMID;
			save_errno = errno;
			remove_sem_set(SOURCE);
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Error with journal pool creation"), save_errno);
		}
		if (-1 == shmctl(udi->shmid, IPC_STAT, &shmstat))
		{
			save_errno = errno;
			DETACH_AND_REMOVE_SHM_AND_SEM; /* remove any sem/shm we had created */
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Error with jnlpool shmctl IPC_STAT"), save_errno);
		}
		udi->gt_shm_ctime = shmstat.shm_ctime;
	}
	assert((INVALID_SHMID != udi->shmid) && (0 != udi->gt_shm_ctime));
	status_l = (sm_long_t)(tmp_jnlpool_ctl = (jnlpool_ctl_ptr_t)do_shmat(udi->shmid, 0, 0));
	if (-1 == status_l)
	{
		save_errno = errno;
		DETACH_AND_REMOVE_SHM_AND_SEM;	/* remove any sem/shm we had created */
		ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
		/* Assert below ensures we dont try to clean up the journal pool even though we errored out while attaching to it */
		assert(NULL == jnlpool.jnlpool_ctl);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error with journal pool shmat"), save_errno);
	}
	jnlpool.jnlpool_ctl = tmp_jnlpool_ctl;
	/* Set a flag to indicate the journal pool is uninitialized. Do this as soon as attaching to shared memory.
	 * This flag will be reset by "gtmsource_seqno_init" when it is done with setting the jnl_seqno fields.
	 */
	if (new_ipc)
		jnlpool.jnlpool_ctl->pool_initialized = FALSE;
	assert(SIZEOF(jnlpool_ctl_struct) % 16 == 0);	/* enforce 16-byte alignment for this structure */
	/* Since seqno is an 8-byte quantity and is used in most of the sections below, we require all sections to
	 * be at least 8-byte aligned. In addition we expect that the beginning of the journal data (JNLDATA_BASE_OFF) is
	 * aligned at a boundary that is suitable for journal records (defined by JNL_WRT_END_MASK).
	 */
	assert(JNLPOOL_CTL_SIZE % 8 == 0);
	/* The assert below trips, if node_local struct is unaligned in gdsbt.h. If you have added a new field, verify that filler
	 *  arrays are adjusted accordingly.
	 */
	assert(JNLPOOL_CRIT_SIZE % 8 == 0);
	assert(SIZEOF(repl_inst_hdr) % 8 == 0);
	assert(SIZEOF(gtmsrc_lcl) % 8 == 0);
	assert(SIZEOF(gtmsource_local_struct) % 8 == 0);
	assert(REPL_INST_HDR_SIZE % 8 == 0);
	assert(GTMSRC_LCL_SIZE % 8 == 0);
	assert(GTMSOURCE_LOCAL_SIZE % 8 == 0);
	assert(JNLDATA_BASE_OFF % JNL_WRT_END_MODULUS == 0);
	/* Ensure that the overhead in the journal pool is never greater than gtmsource_options.buffsize as that would indicate a
	 * out-of-design situation
	 */
	assert(!gtmsource_options.start ||
		((JNLPOOL_CTL_SIZE + JNLPOOL_CRIT_SIZE + REPL_INST_HDR_SIZE + GTMSRC_LCL_SIZE) < gtmsource_options.buffsize));
	/* jnlpool_ctl has an array of size MERRORS_ARRAY_SZ which holds one byte of information for each error in the merrors.msg.
	 * Whenever the below assert fails, the MERRORS_ARRAY_SZ has to be increased while maintaining the 16 byte alignment of the
	 * journal pool.
	 */
	assert(MERRORS_ARRAY_SZ > merrors_ctl.msg_cnt);
	csa->critical = (mutex_struct_ptr_t)((sm_uc_ptr_t)jnlpool.jnlpool_ctl + JNLPOOL_CTL_SIZE); /* secshr_db_clnup uses this
												    * relationship */
	jnlpool_mutex_spin_parms = (mutex_spin_parms_ptr_t)((sm_uc_ptr_t)csa->critical + JNLPOOL_CRIT_SPACE);
	csa->nl = (node_local_ptr_t)((sm_uc_ptr_t)jnlpool_mutex_spin_parms + SIZEOF(mutex_spin_parms_struct));
	if (new_ipc)
		memset(csa->nl, 0, SIZEOF(node_local)); /* Make csa->nl->glob_sec_init FALSE */
	csa->now_crit = FALSE;
	csa->onln_rlbk_cycle = jnlpool.jnlpool_ctl->onln_rlbk_cycle; /* Take a copy of the latest onln_rlbk_cycle */
	jnlpool.repl_inst_filehdr = (repl_inst_hdr_ptr_t)((sm_uc_ptr_t)csa->critical + JNLPOOL_CRIT_SIZE);
	jnlpool.gtmsrc_lcl_array = (gtmsrc_lcl_ptr_t)((sm_uc_ptr_t)jnlpool.repl_inst_filehdr + REPL_INST_HDR_SIZE);
	jnlpool.gtmsource_local_array = (gtmsource_local_ptr_t)((sm_uc_ptr_t)jnlpool.gtmsrc_lcl_array + GTMSRC_LCL_SIZE);
	jnldata_base = jnlpool.jnldata_base = (sm_uc_ptr_t)jnlpool.jnlpool_ctl + JNLDATA_BASE_OFF;
	jnlpool_ctl = jnlpool.jnlpool_ctl;
	assert(!mutex_per_process_init_pid || mutex_per_process_init_pid == process_id);
	if (!mutex_per_process_init_pid)
		mutex_per_process_init();
	START_HEARTBEAT_IF_NEEDED;
	if (new_ipc)
	{
		jnlpool_ctl->instfreeze_environ_inited = FALSE;
		if (ANTICIPATORY_FREEZE_AVAILABLE && !init_anticipatory_freeze_errors())
		{
			DETACH_AND_REMOVE_SHM_AND_SEM;	/* remove any sem/shm we had created */
			udi->grabbed_access_sem = FALSE;
			udi->counter_acc_incremented = FALSE;
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Error initializing custom errors"));
		}
		jnlpool_ctl->critical_off = (sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)jnlpool_ctl;
		jnlpool_ctl->filehdr_off = (sm_uc_ptr_t)jnlpool.repl_inst_filehdr - (sm_uc_ptr_t)jnlpool_ctl;
		jnlpool_ctl->srclcl_array_off = (sm_uc_ptr_t)jnlpool.gtmsrc_lcl_array - (sm_uc_ptr_t)jnlpool_ctl;
		jnlpool_ctl->sourcelocal_array_off = (sm_uc_ptr_t)jnlpool.gtmsource_local_array - (sm_uc_ptr_t)jnlpool_ctl;
		/* Need to initialize the different sections of journal pool. Start with the FILE HEADER section */
		repl_instance.jnlpool_semid = udi->semid;
		repl_instance.jnlpool_shmid = udi->shmid;
		repl_instance.jnlpool_semid_ctime = udi->gt_sem_ctime;
		repl_instance.jnlpool_shmid_ctime = udi->gt_shm_ctime;
		memcpy(jnlpool.repl_inst_filehdr, &repl_instance, REPL_INST_HDR_SIZE);	/* Initialize FILE HEADER */
		jnlpool.repl_inst_filehdr->crash = TRUE;
		/* Since we are creating the journal pool, initialize the mutex structures in the shared memory for later
		 * grab_locks to work correctly
		 */
		DEBUG_ONLY(locknl = csa->nl;)	/* for DEBUG_ONLY LOCK_HIST macro */
		gtm_mutex_init(reg, DEFAULT_NUM_CRIT_ENTRY, FALSE);
		DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
		jnlpool_mutex_spin_parms->mutex_hard_spin_count = MUTEX_HARD_SPIN_COUNT;
		jnlpool_mutex_spin_parms->mutex_sleep_spin_count = MUTEX_SLEEP_SPIN_COUNT;
		jnlpool_mutex_spin_parms->mutex_spin_sleep_mask = MUTEX_SPIN_SLEEP_MASK;
		jnlpool_mutex_spin_parms->mutex_que_entry_space_size = DEFAULT_NUM_CRIT_ENTRY;
		assert(!skip_locks);
		grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
		/* Flush the file header to disk so future callers of "jnlpool_init" see the jnlpool_semid and jnlpool_shmid */
		repl_inst_flush_filehdr();
		/* Initialize GTMSRC_LCL section in journal pool */
		repl_inst_read(udi->fn, (off_t)REPL_INST_HDR_SIZE, (sm_uc_ptr_t)jnlpool.gtmsrc_lcl_array, GTMSRC_LCL_SIZE);
		rel_lock(jnlpool.jnlpool_dummy_reg);
		/* Initialize GTMSOURCE_LOCAL section in journal pool */
		memset(jnlpool.gtmsource_local_array, 0, GTMSOURCE_LOCAL_SIZE);
		gtmsourcelocal_ptr = &jnlpool.gtmsource_local_array[0];
		gtmsrclcl_ptr = &jnlpool.gtmsrc_lcl_array[0];
		for (index = 0; index < NUM_GTMSRC_LCL; index++, gtmsrclcl_ptr++, gtmsourcelocal_ptr++)
		{
			COPY_GTMSRCLCL_TO_GTMSOURCELOCAL(gtmsrclcl_ptr, gtmsourcelocal_ptr);
			gtmsourcelocal_ptr->gtmsource_state = GTMSOURCE_DUMMY_STATE;
			gtmsourcelocal_ptr->gtmsrc_lcl_array_index = index;
			/* since we are setting up the journal pool for the first time, use this time to initialize the
			 * gtmsource_srv_latch as well
			 */
			SET_LATCH_GLOBAL(&gtmsourcelocal_ptr->gtmsource_srv_latch, LOCK_AVAILABLE);
		}
	} else if (!jnlpool.jnlpool_ctl->pool_initialized)
	{	/* Source server that created the journal pool died before completing initialization. */
		if (udi->grabbed_access_sem)
			rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
		udi->grabbed_access_sem = FALSE;
		udi->counter_acc_incremented = FALSE;
		ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name),
			ERR_TEXT, 2, RTS_ERROR_TEXT("Journal pool is incompletely initialized. Run MUPIP RUNDOWN first."));
	}
	slot_needs_init = FALSE;
	/* Do not release ftok semaphore in the following cases as each of them involve the callers writing to the instance file
	 * which requires the ftok semaphore to be held. The callers will take care of releasing the semaphore.
	 * 	a) MUPIP REPLIC -SOURCE -START
	 *		Invoke the function "gtmsource_rootprimary_init"
	 *	b) MUPIP REPLIC -SOURCE -SHUTDOWN
	 *		Invoke the function "gtmsource_flush_jnlpool" from the function "repl_ipc_cleanup"
	 *	c) MUPIP REPLIC -SOURCE -ACTIVATE -ROOTPRIMARY (or -UPDOK) on a journal pool that has updates disabled.
	 *		Invoke the function "gtmsource_rootprimary_init"
	 */
	hold_onto_ftok_sem = is_src_srvr && (gtmsource_options.start || gtmsource_options.shut_down);
	/* Determine "gtmsourcelocal_ptr" to later initialize jnlpool.gtmsource_local */
	if (!is_src_srvr || !gtmsource_options.instsecondary)
	{	/* GT.M or Update process or receiver server or a source server command that did not specify INSTSECONDARY */
		gtmsourcelocal_ptr = NULL;
	} else
	{	/* In jnlpool.gtmsource_local_array, find the structure which corresponds to the input secondary instance name.
		 * Each gtmsource_local structure in the array is termed a slot. A slot is used if the "secondary_instname"
		 * (the secondary instance name) member has a non-zero value. A slot is unused otherwise. Below is a tabulation
		 * of the possible cases and actions for each of the source server commands.
		 *
		 *	-------------------------------------------------------------------------------------------------------
		 *				Used Slot found		Unused slot found	No slot found
		 *	-------------------------------------------------------------------------------------------------------
		 *	activate		Set gtmsource_local	REPLINSTSECNONE		REPLINSTSECNONE
		 *	deactivate		Set gtmsource_local	REPLINSTSECNONE		REPLINSTSECNONE
		 *	showbacklog		Set gtmsource_local	REPLINSTSECNONE		REPLINSTSECNONE
		 *	checkhealth		Set gtmsource_local	REPLINSTSECNONE		REPLINSTSECNONE
		 *	statslog		Set gtmsource_local	REPLINSTSECNONE		REPLINSTSECNONE
		 *	changelog		Set gtmsource_local	REPLINSTSECNONE		REPLINSTSECNONE
		 *	stopsourcefilter	Set gtmsource_local	REPLINSTSECNONE		REPLINSTSECNONE
		 *	start			AAAA			BBBB    		CCCC
		 *	shutdown		Set gtmsource_local	REPLINSTSECNONE		REPLINSTSECNONE
		 *	needrestart		Set gtmsource_local	DDDD			DDDD
		 *
		 *	AAAA : if the slot has a gtmsource_pid value that is still alive, then issue error message SRCSRVEXISTS
		 *		Else initialize the slot for the new source server.
		 *
		 *	BBBB : Set gtmsource_local to this slot. Initialize slot.
		 *
		 *	CCCC : Slot reuse algorithm. Scan through all the slots again looking for those with a
		 *		gtmsource_state field value 0 (GTMSOURCE_DUMMY_STATE) or those with a gtmsource_pid that does
		 *		not exist.  Note down the connect_jnl_seqno field of all these slots.  Set gtmsource_local to that
		 *		slot with the least value for connect_jnl_seqno. Initialize that slot. If no slot is found issue
		 *		SRCSRVTOOMANY error
		 *
		 *	DDDD : Set gtmsource_local to NULL. The caller gtmsource_needrestart.c will issue the appropriate
		 *		message.
		 *
		 *	Slot Initialization : Set "read_jnl_seqno" to 1.
		 */
		reuse_slot_ptr = NULL;
		gtmsourcelocal_ptr = &jnlpool.gtmsource_local_array[0];
		for ( index = 0; index < NUM_GTMSRC_LCL; index++, gtmsourcelocal_ptr++)
		{
			if ((NULL == reuse_slot_ptr) && ('\0' == gtmsourcelocal_ptr->secondary_instname[0]))
				reuse_slot_ptr = gtmsourcelocal_ptr;
			if (0 == STRCMP(gtmsource_options.secondary_instname, gtmsourcelocal_ptr->secondary_instname))
			{	/* Found matching slot */
				gtmsource_state = gtmsourcelocal_ptr->gtmsource_state;
				gtmsource_pid = gtmsourcelocal_ptr->gtmsource_pid;
				/* Check if source server is already running for this secondary instance */
				if ((0 != gtmsource_pid) && is_proc_alive(gtmsource_pid, 0))
				{	/* Source server is already up and running for this secondary instance */
					if (gtmsource_options.start)
					{
						assert(!skip_locks);
						rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
						udi->grabbed_access_sem = FALSE;
						udi->counter_acc_incremented = FALSE;
						ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
						/* Assert we did not create shm or sem so no need to remove any */
						assert(!new_ipc);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SRCSRVEXISTS, 3,
							LEN_AND_STR(gtmsource_options.secondary_instname), gtmsource_pid);
					}
				} else
				{	/* Source server is not running for this secondary instance */
					if (gtmsource_options.start)
					{	/* We want to reinitialize the source server related fields in "gtmsource_local"
						 * but do NOT want to reinitialize any fields that intersect with "gtmsrc_lcl"
						 */
						CHECK_SLOT(gtmsourcelocal_ptr);
						slot_needs_init = TRUE;
						reset_gtmsrclcl_info = FALSE;
					} else if (!gtmsource_options.needrestart && !gtmsource_options.showbacklog
							&& !gtmsource_options.checkhealth)
					{	/* If NEEDRESTART, we dont care if the source server is alive or not. All that
						 * we care about is if the primary and secondary did communicate or not. That
						 * will be determined in gtmsource_needrestart.c. Do not trigger an error here.
						 * If SHOWBACKLOG or CHECKHEALTH, do not trigger an error as slot was found
						 * even though the source server is not alive. We can generate backlog/checkhealth
						 * information using values from the matched slot.
						 */
						if (udi->grabbed_access_sem)
							rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
						udi->grabbed_access_sem = FALSE;
						udi->counter_acc_incremented = FALSE;
						ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
						/* Assert we did not create shm or sem so no need to remove any */
						assert(!new_ipc);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SRCSRVNOTEXIST, 2,
							LEN_AND_STR(gtmsource_options.secondary_instname));
					}
				}
				break;
			}
		}
		if (NUM_GTMSRC_LCL == index)
		{	/* Did not find a matching slot. Use the unused slot if it was already found and if appropriate. */
			if (gtmsource_options.needrestart)
			{	/* If -NEEDRESTART is specified, set gtmsource_local to NULL. The caller function
				 * "gtmsource_needrestart" will print the appropriate message.
				 */
				gtmsourcelocal_ptr = NULL;
			} else if (NULL == reuse_slot_ptr)
			{	/* No used or unused slot found. Issue REPLINSTSECNONE error except for -start */
				if (!gtmsource_options.start)
				{
					if (udi->grabbed_access_sem)
						rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
					udi->grabbed_access_sem = FALSE;
					udi->counter_acc_incremented = FALSE;
					ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
					/* Assert we did not create shm or sem so no need to remove any */
					assert(!new_ipc);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLINSTSECNONE, 4,
						LEN_AND_STR(gtmsource_options.secondary_instname), full_len, udi->fn);
				} else
				{	/* Find a used slot that can be reused. Find one with least value of "connect_jnl_seqno". */
					reuse_slot_seqnum = MAX_SEQNO;
					gtmsourcelocal_ptr = &jnlpool.gtmsource_local_array[0];
					for (index = 0; index < NUM_GTMSRC_LCL; index++, gtmsourcelocal_ptr++)
					{
						gtmsource_state = gtmsourcelocal_ptr->gtmsource_state;
						gtmsource_pid = gtmsourcelocal_ptr->gtmsource_pid;
						if ((0 == gtmsource_pid) || !is_proc_alive(gtmsource_pid, 0))
						{	/* Slot can be reused */
							if (gtmsourcelocal_ptr->connect_jnl_seqno < reuse_slot_seqnum)
							{
								reuse_slot_seqnum = gtmsourcelocal_ptr->connect_jnl_seqno;
								reuse_slot_ptr = gtmsourcelocal_ptr;
							}
						}
					}
					if (NULL == reuse_slot_ptr)
					{
						if (udi->grabbed_access_sem)
							rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
						udi->grabbed_access_sem = FALSE;
						udi->counter_acc_incremented = FALSE;
						ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
						/* Assert we did not create shm or sem so no need to remove any */
						assert(!new_ipc);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_SRCSRVTOOMANY, 3, NUM_GTMSRC_LCL,
								full_len, udi->fn);
					} else
					{	/* We want to reinitialize the source server related fields in "gtmsource_local"
						 * as well as reinitialize any fields that intersect with "gtmsrc_lcl" as this
						 * slot is being reused for a different secondary instance than is currently stored.
						 */
						gtmsourcelocal_ptr = reuse_slot_ptr;
						CHECK_SLOT(gtmsourcelocal_ptr);
						slot_needs_init = TRUE;
						reset_gtmsrclcl_info = TRUE;
					}
				}
			} else
			{	/* No used slot found. But an unused slot was found. */
				if (gtmsource_options.start)
				{	/* Initialize the unused slot. We want to reinitialize the source server related fields
					 * in "gtmsource_local" as well as reinitialize any fields that intersect with "gtmsrc_lcl"
					 * as this slot is being reused for the first time for any secondary instance name.
					 */
					gtmsourcelocal_ptr = reuse_slot_ptr;
					CHECK_SLOT(gtmsourcelocal_ptr);
					slot_needs_init = TRUE;
					reset_gtmsrclcl_info = TRUE;
				} else
				{	/* One of the following qualifiers has been specified. Issue error.
					 *   ACTIVATE, CHANGELOG, CHECKHEALTH, DEACTIVATE, SHOWBACKLOG,
					 *   STATSLOG, SHUTDOWN or STOPSOURCEFILTER
					 */
					if (udi->grabbed_access_sem)
						rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
					udi->grabbed_access_sem = FALSE;
					udi->counter_acc_incremented = FALSE;
					ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
					/* Assert we did not create shm or sem so no need to remove any */
					assert(!new_ipc);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_REPLINSTSECNONE, 4,
						LEN_AND_STR(gtmsource_options.secondary_instname), full_len, udi->fn);
				}
			}
		}
	}
	assert((ROOTPRIMARY_UNSPECIFIED == gtmsource_options.rootprimary)
		|| (PROPAGATEPRIMARY_SPECIFIED == gtmsource_options.rootprimary)
		|| (ROOTPRIMARY_SPECIFIED == gtmsource_options.rootprimary));
	if (!new_ipc)
	{	/* We did not create shm or sem so no need to remove any of them for any "rts_error" within this IF */
		assert(!STRCMP(repl_instance.inst_info.this_instname, jnlpool.repl_inst_filehdr->inst_info.this_instname));
		/* Check compatibility of caller source server or receiver server command with the current state of journal pool */
		if (!jnlpool.jnlpool_ctl->upd_disabled)
		{
			if (((is_src_srvr && (PROPAGATEPRIMARY_SPECIFIED == gtmsource_options.rootprimary))
				|| ((GTMRECEIVE == pool_user) && !jnlpool.repl_inst_filehdr->is_supplementary)))
			{	/* Journal pool was created as -ROOTPRIMARY (or -UPDOK) and a source server command has
				 * specified -PROPAGATEPRIMARY (or -UPDNOTOK) or a receiver server command is being attempted
				 * on a non-supplementary instance. Issue error.
				 */
				if (udi->grabbed_access_sem)
					rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
				udi->grabbed_access_sem = FALSE;
				udi->counter_acc_incremented = FALSE;
				ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_PRIMARYISROOT, 2,
					LEN_AND_STR((char *)repl_instance.inst_info.this_instname));
			}
		} else if (is_src_srvr)
		{	/* Source server command issued on a propagating primary */
			if (ROOTPRIMARY_SPECIFIED == gtmsource_options.rootprimary)
			{	/* Journal pool was created with a -PROPAGATEPRIMARY command and current source server command
				 * has specified -ROOTPRIMARY (or -UPDOK).
				 */
				assert(!skip_locks);
				if (!gtmsource_options.activate)
				{	/* START or DEACTIVATE was specified. Issue incompatibility error right away */
					rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
					udi->grabbed_access_sem = FALSE;
					udi->counter_acc_incremented = FALSE;
					ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_PRIMARYNOTROOT, 2,
						LEN_AND_STR((char *)repl_instance.inst_info.this_instname));
				} else
				{	/* ACTIVATE was specified. Check if there is only one process attached to the journal
					 * pool. If yes, we are guaranteed that it is indeed the passive source server process
					 * that we are trying to activate (we can assert this because we know for sure the source
					 * server corresponding to this slot "gtmsourcelocal_ptr" is alive and running or else we
					 * would have issued a SRCSRVNOTEXIST error earlier). and that this is a case of a
					 * transition from propagating primary to root primary. If not, issue ACTIVATEFAIL error.
					 */
					assert(NULL != gtmsourcelocal_ptr);
					if (1 != shmstat.shm_nattch)
					{
						rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
						udi->grabbed_access_sem = FALSE;
						udi->counter_acc_incremented = FALSE;
						ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ACTIVATEFAIL, 2,
							LEN_AND_STR(gtmsource_options.secondary_instname));
					} else
						hold_onto_ftok_sem = TRUE;
				}
			}
		}
	}
	if (!csa->nl->glob_sec_init)
	{
		assert(new_ipc);
		assert(slot_needs_init);
		assert(!skip_locks);
		assert(GTMRELAXED != pool_user);
		if (!is_src_srvr || !gtmsource_options.start)
		{
			assert(FALSE);
			if (udi->grabbed_access_sem)
				rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
			udi->grabbed_access_sem = FALSE;
			udi->counter_acc_incremented = FALSE;
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JNLPOOLSETUP, 0,
					ERR_TEXT, 2, RTS_ERROR_LITERAL("Journal pool has not been initialized"));
		}
		/* Initialize the shared memory fields. */
		/* Start_jnl_seqno (and jnl_seqno, read_jnl_seqno) need region shared mem to be properly setup. For now set to 0. */
		jnlpool_ctl->start_jnl_seqno = 0;
		jnlpool_ctl->jnl_seqno = 0;
		jnlpool_ctl->max_zqgblmod_seqno = 0;
		jnlpool_ctl->jnldata_base_off = JNLDATA_BASE_OFF;
		jnlpool_ctl->jnlpool_size = gtmsource_options.buffsize - jnlpool_ctl->jnldata_base_off;
		assert((jnlpool_ctl->jnlpool_size & ~JNL_WRT_END_MASK) == 0);
		jnlpool_ctl->write = 0;
		jnlpool_ctl->lastwrite_len = 0;
		QWASSIGNDW(jnlpool_ctl->early_write_addr, 0);
		QWASSIGNDW(jnlpool_ctl->write_addr, 0);
		if (0 < jnlpool.repl_inst_filehdr->num_histinfo)
		{
			grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
			status = repl_inst_histinfo_get(jnlpool.repl_inst_filehdr->num_histinfo - 1, &last_histinfo);
			rel_lock(jnlpool.jnlpool_dummy_reg);
			assert(0 == status);
			if (0 != status)
			{
				assert(ERR_REPLINSTNOHIST == status);	/* the only error returned by repl_inst_histinfo_get() */
				grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
				repl_inst_flush_jnlpool(TRUE, TRUE); /* to reset "crash" field in instance file header to FALSE */
				rel_lock(jnlpool.jnlpool_dummy_reg);
				DETACH_AND_REMOVE_SHM_AND_SEM;	/* remove any sem/shm we had created */
				udi->grabbed_access_sem = FALSE;
				udi->counter_acc_incremented = FALSE;
				ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Error reading last history record in replication instance file"));
			}
			instfilehdr_seqno = jnlpool.repl_inst_filehdr->jnl_seqno;
			assert(last_histinfo.start_seqno);
			assert(instfilehdr_seqno);
			if (instfilehdr_seqno < last_histinfo.start_seqno)
			{	/* The jnl seqno in the instance file header is not greater than the last histinfo's start seqno */
				grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
				repl_inst_flush_jnlpool(TRUE, TRUE); /* to reset "crash" field in instance file header to FALSE */
				rel_lock(jnlpool.jnlpool_dummy_reg);
				DETACH_AND_REMOVE_SHM_AND_SEM;	/* remove any sem/shm we had created */
				udi->grabbed_access_sem = FALSE;
				udi->counter_acc_incremented = FALSE;
				ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_REPLINSTSEQORD, 6, LEN_AND_LIT("Instance file header"),
					&instfilehdr_seqno, &last_histinfo.start_seqno, LEN_AND_STR(udi->fn));
			}
			jnlpool_ctl->last_histinfo_seqno = last_histinfo.start_seqno;
		} else
			jnlpool_ctl->last_histinfo_seqno = 0;
		assert(ROOTPRIMARY_UNSPECIFIED != gtmsource_options.rootprimary);
		jnlpool_ctl->upd_disabled = TRUE;	/* truly initialized later by a call to "gtmsource_rootprimary_init" */
		jnlpool_ctl->primary_instname[0] = '\0';
		jnlpool_ctl->send_losttn_complete = FALSE;
		memcpy(jnlpool_ctl->jnlpool_id.instfilename, seg->fname, seg->fname_len);
		jnlpool_ctl->jnlpool_id.instfilename[seg->fname_len] = '\0';
		memcpy(jnlpool_ctl->jnlpool_id.label, GDS_RPL_LABEL, GDS_LABEL_SZ);
		memcpy(jnlpool_ctl->jnlpool_id.now_running, gtm_release_name, gtm_release_name_len + 1);
		assert(0 == (offsetof(jnlpool_ctl_struct, start_jnl_seqno) % 8));
					/* ensure that start_jnl_seqno starts at an 8 byte boundary */
		assert(0 == offsetof(jnlpool_ctl_struct, jnlpool_id));
					/* ensure that the pool identifier is at the top of the pool */
		jnlpool_ctl->jnlpool_id.pool_type = JNLPOOL_SEGMENT;
		csa->nl->glob_sec_init = TRUE;
		assert(NULL != jnlpool_creator);
		*jnlpool_creator = TRUE;
	} else if (NULL != jnlpool_creator)
		*jnlpool_creator = FALSE;
	/* If this is a supplementary instance, initialize strm_index to a non-default value.
	 * In case of an update process and a root primary supplementary instance, "strm_index"
	 * will be initialized to a non-zero value later in updproc.c.
	 */
	if (jnlpool.repl_inst_filehdr->is_supplementary)
	{	/* The value of 0 is possible in rare cases, if a process does jnlpool_init more than once
		 * (possible if the first jnlpool_init failed say due to a REPLINSTUNDEF error). In that
		 * case, we are anyways going to set it to the exact same value so allow that in the assert.
		 */
		assert((INVALID_SUPPL_STRM == strm_index) || (0 == strm_index));
		strm_index = 0;
	}
	assert(!(is_src_srvr && gtmsource_options.start) || slot_needs_init);
	jnlpool.gtmsource_local = gtmsourcelocal_ptr;
	assert((NULL == gtmsourcelocal_ptr)
			|| (gtmsourcelocal_ptr->gtmsrc_lcl_array_index == (gtmsourcelocal_ptr - jnlpool.gtmsource_local_array)));
	reg->open = TRUE;	/* this is used by t_commit_cleanup/tp_restart/mutex_deadlock_check */
	reg->read_only = FALSE;	/* maintain csa->read_write simultaneously */
	csa->read_write = TRUE;	/* maintain reg->read_only simultaneously */
	if (slot_needs_init)
	{
		assert(is_src_srvr);
		assert(NULL != gtmsourcelocal_ptr);
		assert(gtmsource_options.start || gtmsource_options.showbacklog);
		assert(!skip_locks);
		assert(GTMRELAXED != pool_user);
		gtmsourcelocal_ptr->gtmsource_pid = 0;
		gtmsourcelocal_ptr->gtmsource_state = GTMSOURCE_DUMMY_STATE;
		if (gtmsource_options.start)
		{	/* Source server startup needs to initialize source server specific fields in the journal pool */
			assert(NULL != gtmsourcelocal_ptr);
			QWASSIGNDW(gtmsourcelocal_ptr->read_addr, 0);
			gtmsourcelocal_ptr->read = 0;
			gtmsourcelocal_ptr->read_state = READ_POOL;
			gtmsourcelocal_ptr->mode = gtmsource_options.mode;
			gtmsourcelocal_ptr->statslog = FALSE;
			gtmsourcelocal_ptr->shutdown = NO_SHUTDOWN;
			gtmsourcelocal_ptr->shutdown_time = -1;
			gtmsourcelocal_ptr->secondary_port = gtmsource_options.secondary_port;
			STRCPY(gtmsourcelocal_ptr->secondary_host, gtmsource_options.secondary_host);
			STRCPY(gtmsourcelocal_ptr->filter_cmd, gtmsource_options.filter_cmd);
			STRCPY(gtmsourcelocal_ptr->log_file, gtmsource_options.log_file);
			gtmsourcelocal_ptr->log_interval = log_interval = gtmsource_options.src_log_interval;
			gtmsourcelocal_ptr->statslog_file[0] = '\0';
			gtmsourcelocal_ptr->last_flush_resync_seqno = 0;
			gtmsourcelocal_ptr->next_histinfo_seqno = 0;/* fully initialized when source server connects to receiver */
			gtmsourcelocal_ptr->next_histinfo_num = 0;  /* fully initialized when source server connects to receiver */
			gtmsourcelocal_ptr->num_histinfo = 0;  /* fully initialized when source server connects to receiver */
			if (GTMSOURCE_MODE_ACTIVE == gtmsource_options.mode)
			{
				gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT] =
					gtmsource_options.connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT];
				gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD] =
					gtmsource_options.connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD];
				gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD] =
					gtmsource_options.connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD];
				gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_ALERT_PERIOD] =
					gtmsource_options.connect_parms[GTMSOURCE_CONN_ALERT_PERIOD];
				gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD] =
					gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD];
				gtmsourcelocal_ptr->connect_parms[GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT] =
					gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT];
			}
			/* At this point online rollback cannot be concurrently running because we hold the journal pool access
			 * control semaphore. So, go ahead and initialize the gtmsource_srv_latch for this source server.
			 */
			SET_LATCH_GLOBAL(&gtmsourcelocal_ptr->gtmsource_srv_latch, LOCK_AVAILABLE);
		}
		if (reset_gtmsrclcl_info)
		{	/* Initialize all fields of "gtmsource_local" that are also present in the corresponding "gtmsrc_lcl" */
			gtmsourcelocal_ptr->read_jnl_seqno = 1;	/* fully initialized when source server connects to receiver */
			memcpy(gtmsourcelocal_ptr->secondary_instname, gtmsource_options.secondary_instname, MAX_INSTNAME_LEN - 1);
			gtmsourcelocal_ptr->connect_jnl_seqno = 0; /* fully initialized when source server connects to receiver */
			gtmsourcelocal_ptr->send_losttn_complete = FALSE;
			/* Now make the corresponding changes from gtmsource_local to the gtmsrc_lcl structure and flush to disk.
			 * This assumes "jnlpool.gtmsource_local" is set appropriately.
			 */
			grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
			repl_inst_flush_gtmsrc_lcl();
			rel_lock(jnlpool.jnlpool_dummy_reg);
		}
	}
	/* Assert that gtmsource_local is set to non-NULL value for all those qualifiers that care about it */
	assert(!(gtmsource_options.start || gtmsource_options.activate || gtmsource_options.deactivate
			|| gtmsource_options.stopsourcefilter || gtmsource_options.changelog || gtmsource_options.statslog)
		|| (NULL != jnlpool.gtmsource_local));
	assert((NULL == jnlpool.gtmsource_local)
		|| !STRCMP(jnlpool.gtmsource_local->secondary_instname, gtmsource_options.secondary_instname));
	temp_jnlpool_ctl->jnlpool_size = jnlpool_ctl->jnlpool_size;
	/* Release control lockout now that this process has attached to the journal pool except if caller is source server.
	 * Source Server will release the control lockout only after it is done with
	 *	a) initializing other fields in the pool (in case of source server startup) or
	 *	b) the actual command (in case of any other source server command like checkhealth, shutdown, showbacklog etc.)
	 */
	if (!is_src_srvr)
	{
		if (udi->grabbed_access_sem)
		{
			if (0 != (save_errno = rel_sem(SOURCE, JNL_POOL_ACCESS_SEM)))
			{
				ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
				/* Assert we did not create shm or sem so no need to remove any */
				assert(!new_ipc);
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Error in rel_sem"), save_errno);
			}
			udi->grabbed_access_sem = FALSE;
			udi->counter_acc_incremented = FALSE;
		}
	} else
	{
		this_side = &jnlpool_ctl->this_side;
		remote_side = &gtmsourcelocal_ptr->remote_side;	/* Set global variable now. Structure will be initialized
								 * later when source server connects to receiver */
	}
	if (!hold_onto_ftok_sem && !ftok_sem_release(jnlpool.jnlpool_dummy_reg, FALSE, FALSE))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JNLPOOLSETUP);
	pool_init = TRUE;
	ENABLE_FREEZE_ON_ERROR;
	return;
}

void jnlpool_detach(void)
{
	int		status, save_errno;

	if (TRUE == pool_init)
	{
		rel_lock(jnlpool.jnlpool_dummy_reg);
		mutex_cleanup(jnlpool.jnlpool_dummy_reg);
		if (jnlpool.gtmsource_local && (process_id == jnlpool.gtmsource_local->gtmsource_srv_latch.u.parts.latch_pid))
 			rel_gtmsource_srv_latch(&jnlpool.gtmsource_local->gtmsource_srv_latch);
 		JNLPOOL_SHMDT(status, save_errno);
 		if (0 > status)
 			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_REPLWARN, 2,
					RTS_ERROR_LITERAL("Could not detach from journal pool"), save_errno);
		jnlpool.repl_inst_filehdr = NULL;
		jnlpool.gtmsrc_lcl_array = NULL;
		jnlpool.gtmsource_local_array = NULL;
		jnlpool.jnldata_base = NULL;
		pool_init = FALSE;
	}
}

