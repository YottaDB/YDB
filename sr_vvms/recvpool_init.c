/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_inet.h"
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
#include "jnl.h"
#include "gtmrecv.h"
#include "gtm_logicals.h"
#include "repl_shm.h"
#include "repl_shutdcode.h"
#include "io.h"
#include "is_file_identical.h"
#include "trans_log_name.h"
#include "interlock.h"

GBLREF	recvpool_addrs		recvpool;
GBLREF	int			recvpool_shmid;
GBLREF 	uint4			process_id;
GBLREF 	gtmrecv_options_t	gtmrecv_options;

error_def(ERR_RECVPOOLSETUP);
error_def(ERR_TEXT);

LITREF  char                    gtm_release_name[];
LITREF  int4                    gtm_release_name_len;

#define MAX_RES_TRIES		620 		/* Also defined in gvcst_init_sysops.c */
#define MEGA_BOUND		(1024*1024)

void recvpool_init(recvpool_user pool_user,
		   boolean_t gtmrecv_startup,
		   boolean_t lock_opt_sem)
{
	mstr 			log_nam, trans_log_nam;
	char		        trans_buff[MAX_FN_LEN+1];
	key_t		        recvpool_key;
	int4		        status;
	uint4			ustatus;
	int			lcnt;
	unsigned int		full_len;
	sm_long_t	        status_l;
	boolean_t	        shm_created;
	int		        semflgs;
	struct dsc$descriptor_s name_dsc;
	char			res_name[MAX_NAME_LEN + 2]; /* +1 for null terminator and another +1 for the length stored in [0]
								by global_name() */
	gds_file_id		file_id;

	log_nam.addr = GTM_GBLDIR;
	log_nam.len = SIZEOF(GTM_GBLDIR) - 1;
	if (SS_NORMAL != trans_log_name(&log_nam, &trans_log_nam, trans_buff))
		rts_error(VARLSTCNT(6) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("gtm$gbldir not defined"));
	trans_buff[trans_log_nam.len] = '\0';
	full_len = trans_log_nam.len;
	if (!get_full_path(&trans_buff, trans_log_nam.len, &trans_buff, &full_len, MAX_TRANS_NAME_LEN, &ustatus))
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Failed to get full path for gtm$gbldir"), ustatus);
	trans_log_nam.len = full_len;	/* since on vax, mstr.len is a 'short' */
	/* Get Journal Pool Resource Name : name_dsc holds the resource name */
	set_gdid_from_file((gd_id_ptr_t)&file_id, trans_buff, trans_log_nam.len);
	global_name("GT$R", &file_id, res_name); /* R - Stands for Receiver Pool */
	name_dsc.dsc$a_pointer = &res_name[1];
        name_dsc.dsc$w_length = res_name[0];
        name_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
        name_dsc.dsc$b_class = DSC$K_CLASS_S;
	name_dsc.dsc$a_pointer[name_dsc.dsc$w_length] = '\0';

	/*
 	 * We need to do some receive pool locking. The first lock
	 * to obtain is the receive pool access control
	 * lock which is also used as the rundown lock. When one has
	 * this, no new attaches to the receive pool are allowed.
	 * Also, a rundown cannot occur. We will get this lock,
 	 * then initialize the fields (if not already initialized)
	 * before we release the access control lock.
	 * Refer to repl_sem.h for an enumeration of semaphores in the sem-set
 	 */

	assert(NUM_SRC_SEMS == NUM_RECV_SEMS);
	if(0 != init_sem_set_recvr(&name_dsc))
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0,
				       ERR_TEXT, 2, RTS_ERROR_LITERAL("Error with receiver pool sem init."), REPL_SEM_ERRNO);
	if(0 != grab_sem(RECV, RECV_POOL_ACCESS_SEM))
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0,
			  	       ERR_TEXT, 2, RTS_ERROR_LITERAL("Error with receive pool semaphores"), REPL_SEM_ERRNO);

	/* Store the recvpool key */
	memcpy(recvpool.vms_recvpool_key.name, name_dsc.dsc$a_pointer, name_dsc.dsc$w_length);
	recvpool.vms_recvpool_key.desc.dsc$a_pointer = recvpool.vms_recvpool_key.name;
	recvpool.vms_recvpool_key.desc.dsc$w_length = name_dsc.dsc$w_length;
	recvpool.vms_recvpool_key.desc.dsc$b_dtype = DSC$K_DTYPE_T;
	recvpool.vms_recvpool_key.desc.dsc$b_class = DSC$K_CLASS_S;

	/* Registering with the global section involves grabbing a lock on the recvpool global section
	 * in the ConcurrentRead mode (CR). This lock will be used when deleting the recvpool (in signoff_from_gsec())
	 * to make sure that nobody else is attached to the recvpool global section when detaching from it*/

	if (SS$_NORMAL != (status = register_with_gsec(&recvpool.vms_recvpool_key.desc, &recvpool.shm_lockid)))
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Unable to get lock on recvpool"), status);

	if (GTMRECV != pool_user || !gtmrecv_startup)
	{	/* Global section should already exist */
		if(SS$_NORMAL != (status = map_shm(RECV, &name_dsc, recvpool.shm_range)))
		{
			signoff_from_gsec(recvpool.shm_lockid);
			rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
					RTS_ERROR_LITERAL("Receive pool does not exist"), status);
		}
		shm_created = FALSE;
	} else
	{
		status = create_and_map_shm(RECV, &name_dsc, gtmrecv_options.buffsize, recvpool.shm_range);
		if (SS$_CREATED == status)
			shm_created = TRUE;
		else if (SS$_NORMAL == status)
			shm_created = FALSE;
		else
		{
			signoff_from_gsec(recvpool.shm_lockid);
			rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unalble to create or map to recvpool global section"), status);
		}
	}

	recvpool_shmid = 1; /* A value > 0, as an indication of the existance of recvpool gsec */

	/* Initialize receiver pool pointers to point to appropriate shared memory locations */
        recvpool.recvpool_ctl = recvpool.shm_range[0];
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
		if (pool_user != GTMRECV || !gtmrecv_startup)
			rts_error(VARLSTCNT(6) ERR_RECVPOOLSETUP, 0,
					       ERR_TEXT, 2, RTS_ERROR_LITERAL("Receive pool has not been initialized"));

		/* Initialize the shared memory fields */
		QWASSIGNDW(recvpool.recvpool_ctl->start_jnl_seqno, 0);
		recvpool.recvpool_ctl->recvdata_base_off = RECVDATA_BASE_OFF;
		recvpool.recvpool_ctl->recvpool_size = gtmrecv_options.buffsize - recvpool.recvpool_ctl->recvdata_base_off;
		recvpool.recvpool_ctl->write = 0;
		recvpool.recvpool_ctl->write_wrap = recvpool.recvpool_ctl->recvpool_size;
		strcpy(recvpool.recvpool_ctl->recvpool_id.repl_pool_key, name_dsc.dsc$a_pointer);
		recvpool.recvpool_ctl->wrapped = FALSE;
		strcpy(recvpool.recvpool_ctl->recvpool_id.gtmgbldir, trans_buff);
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
		memset(recvpool.upd_helper_ctl, 0, SIZEOF(*recvpool.upd_helper_ctl));
		SET_LATCH_GLOBAL(&recvpool.upd_helper_ctl->pre_read_lock, LOCK_AVAILABLE);
		recvpool.recvpool_ctl->initialized = TRUE;
		recvpool.recvpool_ctl->fresh_start = TRUE;
	}

	/* If startup, lock out checkhealth and receiver startup */
	if (GTMRECV == pool_user && lock_opt_sem && 0 != grab_sem(RECV, RECV_SERV_OPTIONS_SEM))
		rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
			  RTS_ERROR_LITERAL("Error with receive pool options semaphore"), REPL_SEM_ERRNO);
	/* Release receiver pool control lock out now that it is initialized */
	rel_sem(RECV, RECV_POOL_ACCESS_SEM);
	return;
}
