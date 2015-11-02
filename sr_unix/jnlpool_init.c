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

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	recvpool_addrs		recvpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	sm_uc_ptr_t		jnldata_base;
GBLREF	uint4			process_id;
GBLREF	gd_region		*gv_cur_region;
GBLREF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl;
GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	boolean_t		pool_init;
GBLREF	uint4			process_id;
GBLREF	seq_num			seq_num_zero;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	node_local_ptr_t	locknl;
GBLREF	uint4			log_interval;
GBLREF	boolean_t		is_updproc;
GBLREF	uint4			mutex_per_process_init_pid;

LITREF	char			gtm_release_name[];
LITREF	int4			gtm_release_name_len;

#define	DETACH_AND_REMOVE_SHM_AND_SEM										\
{														\
	error_def(ERR_REPLWARN);										\
														\
	if (new_ipc)												\
	{													\
		assert(!IS_GTM_IMAGE);	/* Since "gtm_putmsg" is done below ensure it is never GT.M */		\
		if (NULL != jnlpool.jnlpool_ctl)								\
		{												\
			if (-1 == shmdt((caddr_t)jnlpool.jnlpool_ctl))						\
				gtm_putmsg(VARLSTCNT(5) ERR_REPLWARN, 2,					\
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
			gtm_putmsg(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,				\
				 RTS_ERROR_LITERAL("Error removing jnlpool "), errno);				\
		remove_sem_set(SOURCE);										\
	}													\
}

#define	CHECK_SLOT(gtmsourcelocal_ptr)												\
{																\
	error_def(ERR_JNLPOOLBADSLOT);												\
																\
	if ((GTMSOURCE_DUMMY_STATE != gtmsourcelocal_ptr->gtmsource_state) || (0 != gtmsourcelocal_ptr->gtmsource_pid))		\
	{	/* Slot is in an out-of-design situation. Send an operator log message with enough debugging detail */		\
		send_msg(VARLSTCNT(7) ERR_JNLPOOLBADSLOT, 5, LEN_AND_STR((char *)gtmsourcelocal_ptr->secondary_instname),	\
			gtmsourcelocal_ptr->gtmsource_pid, gtmsourcelocal_ptr->gtmsource_state,					\
			gtmsourcelocal_ptr->gtmsrc_lcl_array_index);								\
	}															\
}

/* This routine goes through all slots and checks if there is one slot with an active source server that is CONNECTED to a
 * dual-site secondary. This returns TRUE in that case. In all other cases it returns FALSE. Note that this routine does
 * not grab any locks. It rather expects the caller to hold any locks that matter.
 */
static boolean_t	is_dualsite_secondary_connected(void)
{
	int4			index;
	uint4			gtmsource_pid;
	boolean_t		srv_alive;
	gtmsource_local_ptr_t	gtmsourcelocal_ptr;

	gtmsourcelocal_ptr = &jnlpool.gtmsource_local_array[0];
	for (index = 0; index < NUM_GTMSRC_LCL; index++, gtmsourcelocal_ptr++)
	{
		if ('\0' == gtmsourcelocal_ptr->secondary_instname[0])
			continue;
		gtmsource_pid = gtmsourcelocal_ptr->gtmsource_pid;
		srv_alive = (0 == gtmsource_pid) ? FALSE : is_proc_alive(gtmsource_pid, 0);
		if (!srv_alive)
			continue;
		if ((GTMSOURCE_MODE_ACTIVE == gtmsourcelocal_ptr->mode)
				&& (REPL_PROTO_VER_DUALSITE == gtmsourcelocal_ptr->remote_proto_ver))
			return TRUE;
	}
	return FALSE;
}

void jnlpool_init(jnlpool_user pool_user, boolean_t gtmsource_startup, boolean_t *jnlpool_creator)
{
	boolean_t		new_ipc, is_src_srvr, slot_needs_init, reset_gtmsrclcl_info, hold_onto_ftok_sem, srv_alive;
	char			machine_name[MAX_MCNAMELEN], instfilename[MAX_FN_LEN + 1];
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
	repl_triple		last_triple;
	jnlpool_ctl_ptr_t	tmp_jnlpool_ctl;

	error_def(ERR_ACTIVATEFAIL);
	error_def(ERR_JNLPOOLSETUP);
	error_def(ERR_NOJNLPOOL);
	error_def(ERR_PRIMARYISROOT);
	error_def(ERR_PRIMARYNOTROOT);
	error_def(ERR_REPLINSTNMSAME);
	error_def(ERR_REPLINSTNOHIST);
	error_def(ERR_REPLINSTSEQORD);
	error_def(ERR_REPLREQROLLBACK);
	error_def(ERR_REPLREQRUNDOWN);
	error_def(ERR_REPLUPGRADEPRI);
	error_def(ERR_REPLUPGRADESEC);
	error_def(ERR_REPLINSTSECNONE);
	error_def(ERR_SRCSRVEXISTS);
	error_def(ERR_SRCSRVNOTEXIST);
	error_def(ERR_SRCSRVTOOMANY);
	error_def(ERR_TEXT);

	assert(gtmsource_startup == gtmsource_options.start);
	memset(machine_name, 0, SIZEOF(machine_name));
	if (GETHOSTNAME(machine_name, MAX_MCNAMELEN, status))
		rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to get the hostname"), errno);
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
		assert(is_updproc);	/* Should have already attached to receive pool only in case of update process */
		reg = jnlpool.jnlpool_dummy_reg = recvpool.recvpool_dummy_reg;
	}
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	seg = reg->dyn.addr;
	if (!repl_inst_get_name(instfilename, &full_len, MAX_FN_LEN + 1, issue_rts_error))
		GTMASSERT;	/* rts_error should have been issued by repl_inst_get_name */
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
		rts_error(VARLSTCNT(1) ERR_JNLPOOLSETUP);
	repl_inst_read(udi->fn, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
	is_src_srvr = (GTMSOURCE == pool_user);
	/* If caller is source server and secondary instance name has been specified check if it is different from THIS instance */
	if (is_src_srvr && gtmsource_options.instsecondary)
	{
		if (0 == STRCMP(repl_instance.this_instname, gtmsource_options.secondary_instname))
		{
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(4) ERR_REPLINSTNMSAME, 2, LEN_AND_STR((char *)repl_instance.this_instname));
		}
	}
	new_ipc = FALSE;
	if (INVALID_SEMID == repl_instance.jnlpool_semid)
	{	/* First process to do "jnlpool_init". Create the journal pool. */
		if (INVALID_SHMID != repl_instance.jnlpool_shmid)
			GTMASSERT;
		/* Source server startup is the only command that can create the journal pool. Check that. */
		if (!is_src_srvr || !gtmsource_options.start)
		{
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(4) ERR_NOJNLPOOL, 2, full_len, udi->fn);
		}
		if (repl_instance.crash)
		{
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(4) ERR_REPLREQROLLBACK, 2, full_len, udi->fn);
		}
		new_ipc = TRUE;
		assert(NUM_SRC_SEMS == NUM_RECV_SEMS);
		if (INVALID_SEMID == (udi->semid = init_sem_set_source(IPC_PRIVATE, NUM_SRC_SEMS, RWDALL | IPC_CREAT)))
		{
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error creating journal pool semaphore"), REPL_SEM_ERRNO);
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
			rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
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
			rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error with jnlpool semctl IPC_STAT"), save_errno);
		}
		udi->gt_sem_ctime = semarg.buf->sem_ctime;
		/* Create the shared memory */
		if (-1 == (udi->shmid = shmget(IPC_PRIVATE, gtmsource_options.buffsize, IPC_CREAT | RWDALL)))
		{
			udi->shmid = INVALID_SHMID;
			save_errno = errno;
			remove_sem_set(SOURCE);	/* Remove is fine, since we just created it now */
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Error with journal pool creation"), save_errno);
		}
		if (-1 == shmctl(udi->shmid, IPC_STAT, &shmstat))
		{
			save_errno = errno;
			DETACH_AND_REMOVE_SHM_AND_SEM;	/* remove any sem/shm we had created */
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				 RTS_ERROR_LITERAL("Error with jnlpool shmctl IPC_STAT"), save_errno);
		}
		udi->gt_shm_ctime = shmstat.shm_ctime;
	} else
	{	/* find create time of semaphore from the file header and check if the id is reused by others */
		assert(repl_instance.crash);
		semarg.buf = &semstat;
		if (-1 == semctl(repl_instance.jnlpool_semid, 0, IPC_STAT, semarg))
		{
			save_errno = errno;
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(7) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name), save_errno);
		} else if (semarg.buf->sem_ctime != repl_instance.jnlpool_semid_ctime)
		{
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(10) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name),
				ERR_TEXT, 2, RTS_ERROR_TEXT("jnlpool sem_ctime does not match"));
		}
		/* find create time of shared memory from file header and check if the id is reused by others */
		if (INVALID_SHMID == repl_instance.jnlpool_shmid)
		{
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(10) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name),
				ERR_TEXT, 2, RTS_ERROR_TEXT("jnlpool semid is valid but jnlpool shmid is invalid"));
		} else if (-1 == shmctl(repl_instance.jnlpool_shmid, IPC_STAT, &shmstat))
		{
			save_errno = errno;
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(7) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name), save_errno);
		} else if (shmstat.shm_ctime != repl_instance.jnlpool_shmid_ctime)
		{
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(10) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name),
				ERR_TEXT, 2, RTS_ERROR_TEXT("jnlpool shm_ctime does not match"));
		}
		udi->semid = repl_instance.jnlpool_semid;
		udi->gt_sem_ctime = repl_instance.jnlpool_semid_ctime;
		udi->shmid = repl_instance.jnlpool_shmid;
		udi->gt_shm_ctime = repl_instance.jnlpool_shmid_ctime;
		set_sem_set_src(udi->semid); /* repl_sem.c has some functions which needs some static variable to have the id */
	}
	status_l = (sm_long_t)(tmp_jnlpool_ctl = (jnlpool_ctl_ptr_t)do_shmat(udi->shmid, 0, 0));
	if (-1 == status_l)
	{
		save_errno = errno;
		DETACH_AND_REMOVE_SHM_AND_SEM;	/* remove any sem/shm we had created */
		ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
		/* Assert below ensures we dont try to clean up the journal pool even though we errored out while attaching to it */
		assert(NULL == jnlpool.jnlpool_ctl);
		rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error with journal pool shmat"), save_errno);
	}
	jnlpool.jnlpool_ctl = tmp_jnlpool_ctl;
	/* Set a flag to indicate the journal pool is uninitialized. Do this as soon as attaching to shared memory.
	 * This flag will be reset by "gtmsource_seqno_init" when it is done with setting the jnl_seqno fields.
	 */
	if (new_ipc)
		jnlpool.jnlpool_ctl->pool_initialized = FALSE;
	status = grab_sem(SOURCE, JNL_POOL_ACCESS_SEM);
	if (SS_NORMAL != status)
	{
		DETACH_AND_REMOVE_SHM_AND_SEM;	/* remove any sem/shm we had created */
		ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
		rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Error with journal pool access semaphore"), REPL_SEM_ERRNO);
	}
	assert(SIZEOF(jnlpool_ctl_struct) % 16 == 0);	/* enforce 16-byte alignment for this structure */
	/* Since seqno is an 8-byte quantity and is used in most of the sections below, we require all sections to
	 * be at least 8-byte aligned. In addition we expect that the beginning of the journal data (JNLDATA_BASE_OFF) is
	 * aligned at a boundary that is suitable for journal records (defined by JNL_WRT_END_MASK).
	 */
	assert(JNLPOOL_CTL_SIZE % 8 == 0);
	assert(JNLPOOL_CRIT_SIZE % 8 == 0);
	assert(SIZEOF(repl_inst_hdr) % 8 == 0);
	assert(SIZEOF(gtmsrc_lcl) % 8 == 0);
	assert(SIZEOF(gtmsource_local_struct) % 8 == 0);
	assert(REPL_INST_HDR_SIZE % 8 == 0);
	assert(GTMSRC_LCL_SIZE % 8 == 0);
	assert(GTMSOURCE_LOCAL_SIZE % 8 == 0);
	assert(JNLDATA_BASE_OFF % JNL_WRT_END_MODULUS == 0);
	csa->critical = (mutex_struct_ptr_t)((sm_uc_ptr_t)jnlpool.jnlpool_ctl + JNLPOOL_CTL_SIZE); /* secshr_db_clnup uses this
												    * relationship */
	jnlpool_mutex_spin_parms = (mutex_spin_parms_ptr_t)((sm_uc_ptr_t)csa->critical + CRIT_SPACE);
	csa->nl = (node_local_ptr_t)((sm_uc_ptr_t)jnlpool_mutex_spin_parms + SIZEOF(mutex_spin_parms_struct));
	if (new_ipc)
		memset(csa->nl, 0, SIZEOF(node_local)); /* Make csa->nl->glob_sec_init FALSE */
	csa->now_crit = FALSE;
	jnlpool.repl_inst_filehdr = (repl_inst_hdr_ptr_t)((sm_uc_ptr_t)csa->critical + JNLPOOL_CRIT_SIZE);
	jnlpool.gtmsrc_lcl_array = (gtmsrc_lcl_ptr_t)((sm_uc_ptr_t)jnlpool.repl_inst_filehdr + REPL_INST_HDR_SIZE);
	jnlpool.gtmsource_local_array = (gtmsource_local_ptr_t)((sm_uc_ptr_t)jnlpool.gtmsrc_lcl_array + GTMSRC_LCL_SIZE);
	jnldata_base = jnlpool.jnldata_base = (sm_uc_ptr_t)jnlpool.jnlpool_ctl + JNLDATA_BASE_OFF;
	jnlpool_ctl = jnlpool.jnlpool_ctl;
	jnlpool_ctl->critical_off = (sm_uc_ptr_t)csa->critical - (sm_uc_ptr_t)jnlpool_ctl;
	jnlpool_ctl->filehdr_off = (sm_uc_ptr_t)jnlpool.repl_inst_filehdr - (sm_uc_ptr_t)jnlpool_ctl;
	jnlpool_ctl->srclcl_array_off = (sm_uc_ptr_t)jnlpool.gtmsrc_lcl_array - (sm_uc_ptr_t)jnlpool_ctl;
	jnlpool_ctl->sourcelocal_array_off = (sm_uc_ptr_t)jnlpool.gtmsource_local_array - (sm_uc_ptr_t)jnlpool_ctl;
	if (new_ipc)
	{	/* Need to initialize the different sections of journal pool. Start with the FILE HEADER section */
		repl_instance.jnlpool_semid = udi->semid;
		repl_instance.jnlpool_shmid = udi->shmid;
		repl_instance.jnlpool_semid_ctime = udi->gt_sem_ctime;
		repl_instance.jnlpool_shmid_ctime = udi->gt_shm_ctime;
		memcpy(jnlpool.repl_inst_filehdr, &repl_instance, REPL_INST_HDR_SIZE);	/* Initialize FILE HEADER */
		jnlpool.repl_inst_filehdr->crash = TRUE;
		/* Flush the file header to disk so future callers of "jnlpool_init" see the jnlpool_semid and jnlpool_shmid */
		repl_inst_flush_filehdr();
		/* Initialize GTMSRC_LCL section in journal pool */
		repl_inst_read(udi->fn, (off_t)REPL_INST_HDR_SIZE, (sm_uc_ptr_t)jnlpool.gtmsrc_lcl_array, GTMSRC_LCL_SIZE);
		/* Initialize GTMSOURCE_LOCAL section in journal pool */
		memset(jnlpool.gtmsource_local_array, 0, GTMSOURCE_LOCAL_SIZE);
		gtmsourcelocal_ptr = &jnlpool.gtmsource_local_array[0];
		gtmsrclcl_ptr = &jnlpool.gtmsrc_lcl_array[0];
		for (index = 0; index < NUM_GTMSRC_LCL; index++, gtmsrclcl_ptr++, gtmsourcelocal_ptr++)
		{
			COPY_GTMSRCLCL_TO_GTMSOURCELOCAL(gtmsrclcl_ptr, gtmsourcelocal_ptr);
			gtmsourcelocal_ptr->gtmsource_state = GTMSOURCE_DUMMY_STATE;
			gtmsourcelocal_ptr->gtmsrc_lcl_array_index = index;
		}
	} else if (!jnlpool.jnlpool_ctl->pool_initialized)
	{	/* Source server that created the journal pool died before completing initialization. */
		rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
		ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
		rts_error(VARLSTCNT(10) ERR_REPLREQRUNDOWN, 4, DB_LEN_STR(reg), LEN_AND_STR(machine_name),
			ERR_TEXT, 2, RTS_ERROR_TEXT("Journal pool is incompletely initialized. Run MUPIP RUNDOWN first."));
	}
	slot_needs_init = FALSE;
	/* Do not release ftok semaphore in the following cases as each of them involve the callers writing to the instance file
	 * which requires the ftok semaphore to be held. The callers will take care of releasing the semaphore.
	 * 	a) MUPIP REPLIC -SOURCE -START
	 *		Invoke the function "gtmsource_rootprimary_init"
	 *	b) MUPIP REPLIC -SOURCE -SHUTDOWN
	 *		Invoke the function "gtmsource_flush_jnlpool" from the function "repl_ipc_cleanup"
	 *	c) MUPIP REPLIC -SOURCE -ACTIVATE -ROOTPRIMARY on a journal pool that has updates disabled.
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
						rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
						ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
						/* Assert we did not create shm or sem so no need to remove any */
						assert(!new_ipc);
						rts_error(VARLSTCNT(5) ERR_SRCSRVEXISTS, 3,
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
						rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
						ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
						/* Assert we did not create shm or sem so no need to remove any */
						assert(!new_ipc);
						rts_error(VARLSTCNT(4) ERR_SRCSRVNOTEXIST, 2,
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
					rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
					ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
					/* Assert we did not create shm or sem so no need to remove any */
					assert(!new_ipc);
					rts_error(VARLSTCNT(6) ERR_REPLINSTSECNONE, 4,
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
						rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
						ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
						/* Assert we did not create shm or sem so no need to remove any */
						assert(!new_ipc);
						rts_error(VARLSTCNT(5) ERR_SRCSRVTOOMANY, 3, NUM_GTMSRC_LCL, full_len, udi->fn);
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
					rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
					ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
					/* Assert we did not create shm or sem so no need to remove any */
					assert(!new_ipc);
					rts_error(VARLSTCNT(6) ERR_REPLINSTSECNONE, 4,
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
		assert(!STRCMP(repl_instance.this_instname, jnlpool.repl_inst_filehdr->this_instname));
		/* Check compatibility of caller source server or receiver server command with the current state of journal pool */
		if (!jnlpool.jnlpool_ctl->upd_disabled
			&& ((is_src_srvr && (PROPAGATEPRIMARY_SPECIFIED == gtmsource_options.rootprimary))
				|| (GTMRECEIVE == pool_user)))
		{	/* Journal pool was created as -ROOTPRIMARY and a source server command has specified -PROPAGATEPRIMARY
			 * or a receiver server start is being attempted. Issue error.
			 */
			rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(4) ERR_PRIMARYISROOT, 2, LEN_AND_STR((char *)repl_instance.this_instname));
		} else if (is_src_srvr && jnlpool.jnlpool_ctl->upd_disabled)
		{	/* Source server command issued on a propagating primary */
			if (ROOTPRIMARY_SPECIFIED == gtmsource_options.rootprimary)
			{	/* Journal pool was created with a -PROPAGATEPRIMARY command and current source server command
				 * has specified -ROOTPRIMARY.
				 */
				if (!gtmsource_options.activate)
				{	/* START or DEACTIVATE was specified. Issue incompatibility error right away */
					rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
					ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
					rts_error(VARLSTCNT(4) ERR_PRIMARYNOTROOT, 2,
						LEN_AND_STR((char *)repl_instance.this_instname));
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
						ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
						rts_error(VARLSTCNT(4) ERR_ACTIVATEFAIL, 2,
							LEN_AND_STR(gtmsource_options.secondary_instname));
					} else
						hold_onto_ftok_sem = TRUE;
				}
			}
			if (FALSE != jnlpool_ctl->primary_is_dualsite)
			{	/* Root primary of this propagating primary instance does not support multi-site replication.
				 * Do not allow any ACTIVE source server startups or ACTIVATEs.
				 */
				if (gtmsource_options.start && (GTMSOURCE_MODE_ACTIVE == gtmsource_options.mode)
						|| gtmsource_options.activate)
				{	/* Check if receiver server is indeed alive. If not no connection is active
					 * so reset field to reflect primary is NOT dualsite. It is safe to reset this field
					 * as we have the ftok lock on the instance file at this point.
					 */
					gtmrecv_pid = jnlpool.jnlpool_ctl->gtmrecv_pid;
					srv_alive = (0 == gtmrecv_pid) ? FALSE : is_proc_alive(gtmrecv_pid, 0);
					if (!srv_alive)
						jnlpool.jnlpool_ctl->gtmrecv_pid = 0;
					else
					{
						rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
						ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
						rts_error(VARLSTCNT(4) ERR_REPLUPGRADEPRI, 2,
							LEN_AND_STR((char *)repl_instance.this_instname));
					}
				}
			}
		}
		if (is_src_srvr && (FALSE != jnlpool_ctl->secondary_is_dualsite))
		{
			if (gtmsource_options.start)
			{
				if (!is_dualsite_secondary_connected())
				{	/* For some reason, a dual site secondary had connected but later the connection was
					 * closed and the source server did not reset the field "secondary_is_dualsite".
					 * Fix "jnlpool_ctl->secondary_is_dualsite" to reflect that. */
					jnlpool_ctl->secondary_is_dualsite = FALSE;
				} else
				{
					rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
					ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
					rts_error(VARLSTCNT(8) ERR_REPLUPGRADESEC, 2,
						LEN_AND_STR(gtmsource_options.secondary_instname),
						ERR_TEXT, 2, LEN_AND_LIT("Connecting to a multi-site secondary is not allowed"
						" when already connected to a dual site secondary\n"));
				}
			}
		}
	}
	if (!csa->nl->glob_sec_init)
	{
		assert(new_ipc);
		assert(slot_needs_init);
		if (!is_src_srvr || !gtmsource_options.start)
		{
			assert(FALSE);
			rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			rts_error(VARLSTCNT(6) ERR_JNLPOOLSETUP, 0,
					ERR_TEXT, 2, RTS_ERROR_LITERAL("Journal pool has not been initialized"));
		}
		/* Initialize the shared memory fields. */
		/* Start_jnl_seqno (and jnl_seqno, read_jnl_seqno) need region shared mem to be properly setup. For now set to 0. */
		jnlpool_ctl->start_jnl_seqno = 0;
		jnlpool_ctl->jnl_seqno = 0;
		jnlpool_ctl->max_dualsite_resync_seqno = 0;
		jnlpool_ctl->max_zqgblmod_seqno = 0;
		jnlpool_ctl->jnldata_base_off = JNLDATA_BASE_OFF;
		jnlpool_ctl->jnlpool_size = gtmsource_options.buffsize - jnlpool_ctl->jnldata_base_off;
		assert((jnlpool_ctl->jnlpool_size & ~JNL_WRT_END_MASK) == 0);
		jnlpool_ctl->write = 0;
		jnlpool_ctl->lastwrite_len = 0;
		QWASSIGNDW(jnlpool_ctl->early_write_addr, 0);
		QWASSIGNDW(jnlpool_ctl->write_addr, 0);
		if (0 < jnlpool.repl_inst_filehdr->num_triples)
		{
			status = repl_inst_triple_get(jnlpool.repl_inst_filehdr->num_triples - 1, &last_triple);
			assert(0 == status);
			if (0 != status)
			{
				assert(ERR_REPLINSTNOHIST == status);	/* the only error returned by repl_inst_triple_get() */
				rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
				repl_inst_flush_jnlpool(TRUE);	/* to reset "crash" field in instance file header to FALSE */
				DETACH_AND_REMOVE_SHM_AND_SEM;	/* remove any sem/shm we had created */
				ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
				rts_error(VARLSTCNT(6) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
					LEN_AND_LIT("Error reading last history record in replication instance file"));
			}
			instfilehdr_seqno = jnlpool.repl_inst_filehdr->jnl_seqno;
			assert(last_triple.start_seqno);
			assert(instfilehdr_seqno);
			if (instfilehdr_seqno < last_triple.start_seqno)
			{	/* The jnl seqno in the instance file header is not greater than the last triple's start seqno */
				rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
				repl_inst_flush_jnlpool(TRUE);	/* to reset "crash" field in instance file header to FALSE */
				DETACH_AND_REMOVE_SHM_AND_SEM;	/* remove any sem/shm we had created */
				ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
				rts_error(VARLSTCNT(8) ERR_REPLINSTSEQORD, 6, LEN_AND_LIT("Instance file header"),
					&instfilehdr_seqno, &last_triple.start_seqno, LEN_AND_STR(udi->fn));
			}
			jnlpool_ctl->last_triple_seqno = last_triple.start_seqno;
		} else
			jnlpool_ctl->last_triple_seqno = 0;
		assert(ROOTPRIMARY_UNSPECIFIED != gtmsource_options.rootprimary);
		jnlpool_ctl->upd_disabled = TRUE;	/* truly initialized later by a call to "gtmsource_rootprimary_init" */
		jnlpool_ctl->primary_instname[0] = '\0';
		jnlpool_ctl->primary_is_dualsite = FALSE;
		jnlpool_ctl->secondary_is_dualsite = FALSE;
		jnlpool_ctl->send_losttn_complete = FALSE;
		jnlpool_ctl->this_proto_ver = REPL_PROTO_VER_THIS;
		memcpy(jnlpool_ctl->jnlpool_id.instfilename, seg->fname, seg->fname_len);
		jnlpool_ctl->jnlpool_id.instfilename[seg->fname_len] = '\0';
		memcpy(jnlpool_ctl->jnlpool_id.label, GDS_RPL_LABEL, GDS_LABEL_SZ);
		memcpy(jnlpool_ctl->jnlpool_id.now_running, gtm_release_name, gtm_release_name_len + 1);
		assert(0 == (offsetof(jnlpool_ctl_struct, start_jnl_seqno) % 8));
					/* ensure that start_jnl_seqno starts at an 8 byte boundary */
		assert(0 == offsetof(jnlpool_ctl_struct, jnlpool_id));
					/* ensure that the pool identifier is at the top of the pool */
		jnlpool_ctl->jnlpool_id.pool_type = JNLPOOL_SEGMENT;
		DEBUG_ONLY(locknl = csa->nl;)	/* for DEBUG_ONLY LOCK_HIST macro */
		gtm_mutex_init(reg, NUM_CRIT_ENTRY, FALSE);
		DEBUG_ONLY(locknl = NULL;)	/* restore "locknl" to default value */
		jnlpool_mutex_spin_parms->mutex_hard_spin_count = MUTEX_HARD_SPIN_COUNT;
		jnlpool_mutex_spin_parms->mutex_sleep_spin_count = MUTEX_SLEEP_SPIN_COUNT;
		jnlpool_mutex_spin_parms->mutex_spin_sleep_mask = MUTEX_SPIN_SLEEP_MASK;
		csa->nl->glob_sec_init = TRUE;
		assert(NULL != jnlpool_creator);
		*jnlpool_creator = TRUE;
	} else if (NULL != jnlpool_creator)
		*jnlpool_creator = FALSE;
	assert(!(is_src_srvr && gtmsource_options.start) || slot_needs_init);
	jnlpool.gtmsource_local = gtmsourcelocal_ptr;
	reg->open = TRUE;	/* this is used by t_commit_cleanup/tp_restart/mutex_deadlock_check */
	reg->read_only = FALSE;	/* maintain csa->read_write simultaneously */
	csa->read_write = TRUE;	/* maintain reg->read_only simultaneously */
	if (slot_needs_init)
	{
		assert(is_src_srvr);
		assert(NULL != gtmsourcelocal_ptr);
		assert(gtmsource_options.start || gtmsource_options.showbacklog);
		gtmsourcelocal_ptr->gtmsource_pid = 0;
		gtmsourcelocal_ptr->gtmsource_state = GTMSOURCE_DUMMY_STATE;
		if (gtmsource_options.start)
		{	/* Source server startup needs to initialize source server specific fields in the journal pool */
			assert(NULL != gtmsourcelocal_ptr);
			QWASSIGNDW(gtmsourcelocal_ptr->read_addr, 0);
			gtmsourcelocal_ptr->read = 0;
			gtmsourcelocal_ptr->read_state = READ_POOL;
			gtmsourcelocal_ptr->mode = gtmsource_options.mode;
			assert(gtmsourcelocal_ptr->gtmsrc_lcl_array_index == (gtmsourcelocal_ptr - jnlpool.gtmsource_local_array));
			gtmsourcelocal_ptr->statslog = FALSE;
			gtmsourcelocal_ptr->shutdown = NO_SHUTDOWN;
			gtmsourcelocal_ptr->shutdown_time = -1;
			gtmsourcelocal_ptr->secondary_inet_addr = gtmsource_options.sec_inet_addr;
			gtmsourcelocal_ptr->secondary_port = gtmsource_options.secondary_port;
			STRCPY(gtmsourcelocal_ptr->secondary_host, gtmsource_options.secondary_host);
			STRCPY(gtmsourcelocal_ptr->filter_cmd, gtmsource_options.filter_cmd);
			STRCPY(gtmsourcelocal_ptr->log_file, gtmsource_options.log_file);
			gtmsourcelocal_ptr->log_interval = log_interval = gtmsource_options.src_log_interval;
			gtmsourcelocal_ptr->statslog_file[0] = '\0';
			gtmsourcelocal_ptr->last_flush_resync_seqno = 0;
			gtmsourcelocal_ptr->next_triple_seqno = 0;/* fully initialized when source server connects to receiver */
			gtmsourcelocal_ptr->next_triple_num = 0;  /* fully initialized when source server connects to receiver */
			gtmsourcelocal_ptr->num_triples = 0;  /* fully initialized when source server connects to receiver */
			gtmsourcelocal_ptr->remote_proto_ver = REPL_PROTO_VER_UNINITIALIZED; /* fully initialized when source
												server connects to receiver */
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
			repl_inst_flush_gtmsrc_lcl();
		}
	}
	/* Initialize mutex socket, memory semaphore etc. before any "grab_lock" is done by this process on the journal pool.
	 * The only issue is if this process is going to fork off a child. This is because the initialization is pid specific,
	 * and since the child process is going to inherit the parent's environment (except for the pid), the mutex
	 * initialization for the child would correspond to its parent's pid and not to its own. The source server and
	 * receiver server startup commands are the only ones that satisfy this condition and out of these the receiver server
	 * command does a "grab_lock". This condition is handled by the receiver server child process reinitializating its
	 * mutex structures based on its pid. The source server startup command does not do this initialization since it does
	 * not currently invoke "grab_lock" during its lifetime.
	 */
	if (!gtmsource_options.start)
	{
		assert(!mutex_per_process_init_pid || mutex_per_process_init_pid == process_id);
		if (!mutex_per_process_init_pid)
			mutex_per_process_init();
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
		if (0 != (save_errno = rel_sem(SOURCE, JNL_POOL_ACCESS_SEM)))
		{
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
			/* Assert we did not create shm or sem so no need to remove any */
			assert(!new_ipc);
			rts_error(VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Error in rel_sem"), save_errno);
		}
	}
	if (!hold_onto_ftok_sem && !ftok_sem_release(jnlpool.jnlpool_dummy_reg, FALSE, FALSE))
		rts_error(VARLSTCNT(1) ERR_JNLPOOLSETUP);
	pool_init = TRUE;
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
		jnlpool.repl_inst_filehdr = NULL;
		jnlpool.gtmsrc_lcl_array = NULL;
		jnlpool.gtmsource_local_array = NULL;
		jnlpool.jnldata_base = NULL;
		pool_init = FALSE;
	}
}

