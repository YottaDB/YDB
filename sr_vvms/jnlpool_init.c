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

#include <ssdef.h>
#include <prtdef.h>
#include <secdef.h>
#include <psldef.h>
#include <descrip.h>
#include <stddef.h>

#include <errno.h>
#include "gtm_inet.h" /* Required for gtmsource.h */
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "repl_sem.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtm_logicals.h"
#include "jnl.h"
#include "repl_shm.h"
#include "io.h"
#include "is_file_identical.h"
#include "trans_log_name.h"
#include "error.h"
#include "mutex.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	sm_uc_ptr_t		jnldata_base;
GBLREF	int4			jnlpool_shmid;
GBLREF	uint4			process_id;
GBLREF	gd_region		*gv_cur_region;
GBLREF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl;
GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	boolean_t		pool_init;
GBLREF	uint4			process_id;

GBLREF	seq_num			seq_num_zero;
LITREF	char			gtm_release_name[];
LITREF	int4			gtm_release_name_len;

static	gd_region		jnlpool_dummy_reg;
static	gd_segment		jnlpool_dummy_seg;
static	file_control		jnlpool_dummy_fc;
static	vms_gds_info		jnlpool_dummy_vdi;
static	sgmnt_addrs		*jnlpool_dummy_sa;

/* Keep track of which resources we have so we can release them if necessary in
   our condition handler.
*/
static VSIG_ATOMIC_T	have_source_access_sem;
static VSIG_ATOMIC_T	have_source_options_sem;
static VSIG_ATOMIC_T	have_source_count_sem;
static VSIG_ATOMIC_T	gsec_is_registered;

#define MAX_RES_TRIES		620 		/* Also defined in gvcst_init_sysops.c */

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_JNLPOOLSETUP);
error_def(ERR_REPLERR);
error_def(ERR_REPLWARN);
error_def(ERR_STACKOFLOW);
error_def(ERR_TEXT);
error_def(ERR_VMSMEMORY);

CONDITION_HANDLER(jnlpool_init_ch)
{
	START_CH;
	if (!(IS_GTM_ERROR(SIGNAL)) || DUMPABLE || SEVERITY == ERROR)
	{	/* Release resources we have aquired */
		if (gsec_is_registered)
		{
			gsec_is_registered = FALSE;
			signoff_from_gsec(jnlpool.shm_lockid);
		}
		if (have_source_count_sem)
		{
			have_source_count_sem = FALSE;
			rel_sem_immediate(SOURCE, SRC_SERV_COUNT_SEM);
		}
		if (have_source_options_sem)
		{
			have_source_options_sem = FALSE;
			rel_sem_immediate(SOURCE, SRC_SERV_OPTIONS_SEM);
		}
		if (have_source_access_sem)
		{
			have_source_access_sem = FALSE;
			rel_sem_immediate(SOURCE, JNL_POOL_ACCESS_SEM);
		}
		NEXTCH;
	}
	/* warning, info, or success */
	CONTINUE;
}

void jnlpool_init(jnlpool_user pool_user,
		  boolean_t gtmsource_startup,
		  boolean_t *jnlpool_initialized)
{
	mstr 			log_nam, trans_log_nam;
	char		        trans_buff[MAX_FN_LEN+1];
	int4		        status;
	uint4			ustatus;
	unsigned int		full_len;
	boolean_t	        shm_created;
	struct dsc$descriptor_s name_dsc;
	char			res_name[MAX_NAME_LEN + 2]; /* +1 for the terminator and another +1 for the length stored in [0] by
								global_name() */
	gds_file_id		file_id;
	mutex_spin_parms_ptr_t	jnlpool_mutex_spin_parms;

	have_source_access_sem = FALSE;
	have_source_options_sem = FALSE;
	have_source_count_sem = FALSE;
	gsec_is_registered = FALSE;
	ESTABLISH(jnlpool_init_ch);
	memset((uchar_ptr_t)&jnlpool, 0, SIZEOF(jnlpool_addrs));
	memset((uchar_ptr_t)&jnlpool_dummy_reg, 0, SIZEOF(gd_region));
	memset((uchar_ptr_t)&jnlpool_dummy_seg, 0, SIZEOF(gd_segment));
	memset((uchar_ptr_t)&jnlpool_dummy_fc, 0, SIZEOF(file_control));
	memset((uchar_ptr_t)&jnlpool_dummy_vdi, 0, SIZEOF(vms_gds_info));

	jnlpool.jnlpool_dummy_reg = &jnlpool_dummy_reg;
	MEMCPY_LIT(jnlpool_dummy_reg.rname, JNLPOOL_DUMMY_REG_NAME);
	jnlpool_dummy_reg.rname_len = STR_LIT_LEN(JNLPOOL_DUMMY_REG_NAME);
	jnlpool_dummy_reg.dyn.addr = &jnlpool_dummy_seg;
	jnlpool_dummy_reg.dyn.addr->acc_meth = dba_bg; /* To keep tp_change_reg happy */
	jnlpool_dummy_seg.file_cntl = &jnlpool_dummy_fc;
	jnlpool_dummy_fc.file_info = &jnlpool_dummy_vdi;
	jnlpool_dummy_sa = &jnlpool_dummy_vdi.s_addrs;

	log_nam.addr = GTM_GBLDIR;
	log_nam.len = SIZEOF(GTM_GBLDIR) - 1;

	if (SS_NORMAL != trans_log_name(&log_nam, &trans_log_nam, trans_buff))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("gtm$gbldir not defined"));
	trans_buff[trans_log_nam.len] = '\0';
	full_len = trans_log_nam.len;
	if (!get_full_path(&trans_buff, trans_log_nam.len, &trans_buff, &full_len, SIZEOF(trans_buff), &ustatus))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Failed to get full path for gtm$gbldir"), ustatus);
	trans_log_nam.len = full_len;	/* since on vax, mstr.len is a 'short' */
	memcpy(jnlpool_dummy_seg.fname, trans_buff, trans_log_nam.len);
	jnlpool_dummy_seg.fname_len = trans_log_nam.len;
	jnlpool_dummy_seg.fname[jnlpool_dummy_seg.fname_len] = '\0';

	/* Get Journal Pool Resource Name : name_dsc holds the resource name */
	set_gdid_from_file((gd_id *)&file_id, trans_buff, trans_log_nam.len);
	global_name("GT$P", &file_id, res_name); /* P - Stands for Journal Pool */
	name_dsc.dsc$a_pointer = &res_name[1];
        name_dsc.dsc$w_length = res_name[0];
        name_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
        name_dsc.dsc$b_class = DSC$K_CLASS_S;
	name_dsc.dsc$a_pointer[name_dsc.dsc$w_length] = '\0';

	/*
 	 * We need to do some journal pool locking. The lock
	 * to obtain is the journal pool access control lock
	 * which is also used as the rundown lock. When one has this, no
	 * new attaches to the journal pool are allowed. Also, a
	 * rundown cannot occur. We will get this lock,
 	 * then initialize the fields (if not already initialized)
	 * before we release the access control lock.
	 * Refer to repl_sem.h for an enumeration of semaphores in the sem-set
 	 */

	assert(NUM_SRC_SEMS == NUM_RECV_SEMS);
	if (0 != init_sem_set_source(&name_dsc))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			  RTS_ERROR_LITERAL("Error with journal pool sem init."), REPL_SEM_ERRNO);
	if (0 != grab_sem(SOURCE, JNL_POOL_ACCESS_SEM))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			  RTS_ERROR_LITERAL("Error with journal pool access semaphore"), REPL_SEM_ERRNO);
	have_source_access_sem = TRUE;
	if (GTMSOURCE == pool_user && gtmsource_startup)
	{
		/* Get the option semaphore */
		if (0 != grab_sem(SOURCE, SRC_SERV_OPTIONS_SEM))
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0,
				  ERR_TEXT, 2, RTS_ERROR_LITERAL("Error with journal pool option semaphore"), REPL_SEM_ERRNO);
		}
		have_source_options_sem = TRUE;
		/* Get the server count semaphore, to make sure that somebody else hasn't created one already */
		if (0 != grab_sem_immediate(SOURCE, SRC_SERV_COUNT_SEM))
		{
			if (REPL_SEM_NOT_GRABBED)
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JNLPOOLSETUP, 0,
					  ERR_TEXT, 2, RTS_ERROR_LITERAL("Source Server already exists"));
			} else
			{
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
					  RTS_ERROR_LITERAL("Error with journal pool server count semaphore"), REPL_SEM_ERRNO);
			}
		}
		have_source_count_sem = TRUE;
	}

	/* At this point, if (Source-serve AND Startup) all the 3 sems have been grabbed
	   		  else JNL_POOL_ACCESS_SEM alone has been grabbed
	 */

	/* Store the jnlpool key */
	memcpy(jnlpool.vms_jnlpool_key.name, name_dsc.dsc$a_pointer, name_dsc.dsc$w_length);
	jnlpool.vms_jnlpool_key.desc.dsc$a_pointer = jnlpool.vms_jnlpool_key.name;
	jnlpool.vms_jnlpool_key.desc.dsc$w_length = name_dsc.dsc$w_length;
	jnlpool.vms_jnlpool_key.desc.dsc$b_dtype = DSC$K_DTYPE_T;
	jnlpool.vms_jnlpool_key.desc.dsc$b_class = DSC$K_CLASS_S;

	/* Registering with the global section involves grabbing a lock on the jnlpool global section
	 * in the ConcurrentRead mode (CR). This lock will be used when deleting the jnlpool (in signoff_from_gsec())
	 * to make sure that nobody else is attached to the jnlpool global section when detaching from it*/

	if (SS$_NORMAL != (status = register_with_gsec(&jnlpool.vms_jnlpool_key.desc, &jnlpool.shm_lockid)))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
			RTS_ERROR_LITERAL("Unable to get lock on jnlpool"), status);
	gsec_is_registered = TRUE;

	if (GTMSOURCE != pool_user || !gtmsource_startup)
	{	/* Global section should already exist */
		if (SS$_NORMAL != (status = map_shm(SOURCE, &name_dsc, jnlpool.shm_range)))
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Journal pool does not exist"), status);
		}
		shm_created = FALSE;
	} else
	{
		status = create_and_map_shm(SOURCE, &name_dsc, gtmsource_options.buffsize, jnlpool.shm_range);
		if (SS$_CREATED == status)
			shm_created = TRUE;
		else if (SS$_NORMAL == status)
			shm_created = FALSE;
		else
		{
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_JNLPOOLSETUP, 0,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to create or map to jnlpool global section"), status);
		}
	}


	jnlpool_shmid = 1; /* A value > 0, as an indication of the existance of recvpool gsec */

	/* Initialize journal pool pointers to point to appropriate shared memory locations */

        jnlpool.jnlpool_ctl = ROUND_UP((unsigned long)jnlpool.shm_range[0], (2*BITS_PER_UCHAR));
	jnlpool_dummy_sa->critical = (mutex_struct_ptr_t)((sm_uc_ptr_t)jnlpool.jnlpool_ctl + JNLPOOL_CTL_SIZE); /* secshr_db_clnup
														 * uses this
														 * relationship */
	jnlpool_mutex_spin_parms = (mutex_spin_parms_ptr_t)((sm_uc_ptr_t)jnlpool_dummy_sa->critical + JNLPOOL_CRIT_SPACE);
 	jnlpool_dummy_sa->nl = (node_local_ptr_t)((sm_uc_ptr_t)jnlpool_mutex_spin_parms + SIZEOF(mutex_spin_parms_struct));
	if (shm_created)
		memset(jnlpool_dummy_sa->nl, 0, SIZEOF(node_local)); /* Make jnlpool_dummy_sa->nl->glob_sec_init FALSE */
	jnlpool_dummy_sa->now_crit = FALSE;
 	jnlpool.gtmsource_local = (gtmsource_local_ptr_t)((sm_uc_ptr_t)jnlpool_dummy_sa->nl + SIZEOF(node_local));
	jnldata_base = jnlpool.jnldata_base = (sm_uc_ptr_t)jnlpool.jnlpool_ctl + JNLDATA_BASE_OFF;
	jnlpool_ctl = jnlpool.jnlpool_ctl;

	if (GTMSOURCE == pool_user && gtmsource_startup)
	{
		jnlpool.gtmsource_local->gtmsource_pid = 0;
		*jnlpool_initialized = FALSE;
	}

	if (!jnlpool_dummy_sa->nl->glob_sec_init) /* Shared memory is created by this process */
	{
		if (GTMSOURCE != pool_user || !gtmsource_startup)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JNLPOOLSETUP, 0,
					       ERR_TEXT, 2, RTS_ERROR_LITERAL("Journal pool has not been initialized"));

		/* Initialize the shared memory fields */

		/*
		 * start_jnl_seqno (and jnl_seqno, read_jnl_seqno) need
		 * region shared mem to be setup.
		 */

		jnlpool_ctl->jnldata_base_off = JNLDATA_BASE_OFF;
		jnlpool_ctl->jnlpool_size = gtmsource_options.buffsize - jnlpool_ctl->jnldata_base_off;
		assert((jnlpool_ctl->jnlpool_size & ~JNL_WRT_END_MASK) == 0);
		assert(!(SIZEOF(replpool_identifier) % BITS_PER_UCHAR));
		jnlpool_ctl->write = 0;
		jnlpool_ctl->lastwrite_len = 0;
		QWASSIGNDW(jnlpool_ctl->early_write_addr, 0);
		QWASSIGNDW(jnlpool_ctl->write_addr, 0);
		strcpy(jnlpool_ctl->jnlpool_id.repl_pool_key, name_dsc.dsc$a_pointer);
		memcpy(jnlpool_ctl->jnlpool_id.gtmgbldir, jnlpool_dummy_seg.fname, jnlpool_dummy_seg.fname_len);
		jnlpool_ctl->jnlpool_id.gtmgbldir[jnlpool_dummy_seg.fname_len] = '\0';
		memcpy(jnlpool_ctl->jnlpool_id.label, GDS_RPL_LABEL, GDS_LABEL_SZ);         /* why -1 heree ???? */
		memcpy(jnlpool_ctl->jnlpool_id.now_running, gtm_release_name, gtm_release_name_len + 1);
		assert(0 == (offsetof(jnlpool_ctl_struct, start_jnl_seqno) % BITS_PER_UCHAR));
			/* ensure that start_jnl_seqno starts at an 8 byte boundary */
		assert(0 == offsetof(jnlpool_ctl_struct, jnlpool_id));
			/* ensure that the pool identifier is at the top of the pool */
		jnlpool_ctl->jnlpool_id.pool_type = JNLPOOL_SEGMENT;
		mutex_init(jnlpool_dummy_sa->critical, DEFAULT_NUM_CRIT_ENTRY, FALSE);
		jnlpool_mutex_spin_parms->mutex_hard_spin_count = MUTEX_HARD_SPIN_COUNT;
		jnlpool_mutex_spin_parms->mutex_sleep_spin_count = MUTEX_SLEEP_SPIN_COUNT;
		jnlpool_mutex_spin_parms->mutex_spin_sleep_mask = MUTEX_SPIN_SLEEP_MASK;
		jnlpool_mutex_spin_parms->mutex_que_entry_space_size = DEFAULT_NUM_CRIT_ENTRY;
		QWASSIGNDW(jnlpool.gtmsource_local->read_addr, 0);
		jnlpool.gtmsource_local->read = 0;
		jnlpool.gtmsource_local->read_state = READ_POOL;
		jnlpool.gtmsource_local->mode = gtmsource_options.mode;
		QWASSIGN(jnlpool.gtmsource_local->lastsent_jnl_seqno, seq_num_zero); /* 0 indicates nothing has been sent yet */
		jnlpool.gtmsource_local->lastsent_time = -1;

		jnlpool.gtmsource_local->statslog = FALSE;
		jnlpool.gtmsource_local->shutdown = FALSE;
		jnlpool.gtmsource_local->shutdown_time = -1;
		jnlpool.gtmsource_local->secondary_port = gtmsource_options.secondary_port;
		strcpy(jnlpool.gtmsource_local->secondary_host, gtmsource_options.secondary_host);
		strcpy(jnlpool.gtmsource_local->filter_cmd, gtmsource_options.filter_cmd);
		strcpy(jnlpool.gtmsource_local->log_file, gtmsource_options.log_file);
		jnlpool.gtmsource_local->statslog_file[0] = '\0';

		jnlpool_dummy_sa->nl->glob_sec_init = TRUE;
		*jnlpool_initialized = TRUE;
	}

	temp_jnlpool_ctl->jnlpool_size = jnlpool_ctl->jnlpool_size;

	/* Release control lockout now that it is initialized.
	 * Source Server will release the control lockout only after
	 * other fields shared with GTM processes are initialized */
	if (GTMSOURCE != pool_user || !gtmsource_startup)
	{
		have_source_access_sem = FALSE;
		rel_sem(SOURCE, JNL_POOL_ACCESS_SEM);
	}
	pool_init = TRUE; /* This is done for properly setting the updata_disable flag by active/passive
				source server in jnl_file_open */
	jnlpool_dummy_reg.open = TRUE;	/* this is used by t_commit_cleanup/tp_restart/mutex_deadlock_check */
	jnlpool_dummy_reg.read_only = FALSE;	/* maintain csa->read_write simultaneously */
	jnlpool_dummy_sa->read_write = TRUE;	/* maintain reg->read_only simultaneously */
	REVERT;
	return;
}

void	jnlpool_detach(void)
{
	int4 status;

	if (TRUE == pool_init)
	{
		/* Delete expanded virtual address space */
		if (SS$_NORMAL != (status = detach_shm(jnlpool.shm_range)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_REPLWARN, 2,
					RTS_ERROR_LITERAL("Could not detach from journal pool"), status);
		if (SS$_NORMAL != (status = signoff_from_gsec(jnlpool.shm_lockid)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_REPLERR, 0,
					ERR_TEXT, 2, RTS_ERROR_LITERAL("Error dequeueing lock on jnlpool global section"), status);
		jnlpool_ctl = NULL;
		memset(&jnlpool, 0, SIZEOF(jnlpool));
		pool_init = FALSE;
	}
}
