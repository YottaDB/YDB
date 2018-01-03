/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_unistd.h"
#include "gtm_inet.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_fcntl.h"

#include "gtm_ipc.h"
#include <sys/shm.h>
#include <errno.h>
#include <stddef.h>
#include <sys/sem.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "gtmio.h"
#include "repl_instance.h"
#include "mutex.h"
#include "jnl.h"
#include "repl_sem.h"
#include "eintr_wrappers.h"
#include "mu_rndwn_file.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "mu_rndwn_replpool.h"
#include "ipcrmid.h"
#include "do_semop.h"
#include "util.h"
#include "gtmmsg.h"
#include "gtm_sem.h"
#include "do_shmat.h"	/* for do_shmat() prototype */
#include "buddy_list.h"		/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "error.h"
#include "anticipatory_freeze.h"

GBLREF	boolean_t		argumentless_rundown;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	recvpool_addrs		recvpool;
GBLREF	mur_gbls_t		murgbl;
GBLREF	mur_opt_struct		mur_options;
GBLREF	uint4			mutex_per_process_init_pid;
GBLREF	uint4			process_id;

LITREF char             	gtm_release_name[];
LITREF int4             	gtm_release_name_len;

error_def(ERR_REPLACCSEM);
error_def(ERR_REPLINSTOPEN);
error_def(ERR_REPLPOOLINST);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

#define DETACH(START_ADDR, SHM_ID, INSTFILENAME)					\
{											\
	int		lcl_save_errno;							\
											\
	if (-1 == shmdt((void *)START_ADDR))						\
	{										\
		lcl_save_errno = errno;							\
		ISSUE_REPLPOOLINST(lcl_save_errno, SHM_ID, INSTFILENAME, "shmdt()");	\
	}										\
}

int     mu_rndwn_replpool2(replpool_identifier *replpool_id, repl_inst_hdr_ptr_t repl_inst_filehdr, int shm_id, boolean_t *ipc_rmvd,
			   char *instfilename, sm_uc_ptr_t start_addr, int nattch)
{
	int			save_errno, status;
	char			pool_type;
	unix_db_info		*udi;
	sgmnt_addrs		*csa;
	gd_region		*reg;
	boolean_t		anticipatory_freeze_available, reset_crash;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	pool_type = replpool_id->pool_type;
	anticipatory_freeze_available = INST_FREEZE_ON_ERROR_POLICY;
	/* assert that the identifiers are at the top of replpool control structure */
	assert(0 == offsetof(jnlpool_ctl_struct, jnlpool_id));
	assert(0 == offsetof(recvpool_ctl_struct, recvpool_id));
	memcpy((void *)replpool_id, (void *)start_addr, SIZEOF(replpool_identifier));
	if (memcmp(replpool_id->label, GDS_RPL_LABEL, GDS_LABEL_SZ - 1))
	{
		if (!memcmp(replpool_id->label, GDS_RPL_LABEL, GDS_LABEL_SZ - 3))
			util_out_print(
				"Incorrect version for the replpool segment (id = !UL) belonging to replication instance !AD",
					TRUE, shm_id, LEN_AND_STR(instfilename));
		else
			util_out_print("Incorrect replpool format for the segment (id = !UL) belonging to replication instance !AD",
					TRUE, shm_id, LEN_AND_STR(instfilename));
		DETACH(start_addr, shm_id, instfilename);
		return -1;
	}
	if (memcmp(replpool_id->now_running, gtm_release_name, gtm_release_name_len + 1))
	{
		util_out_print("Attempt to access with version !AD, while already using !AD for replpool segment (id = !UL)"
				" belonging to replication instance !AD.", TRUE, gtm_release_name_len, gtm_release_name,
				LEN_AND_STR(replpool_id->now_running), shm_id, LEN_AND_STR(instfilename));
		DETACH(start_addr, shm_id, instfilename);
		return -1;
	}
	reset_crash = (!anticipatory_freeze_available || argumentless_rundown);
	/* Assert that if we haven't yet attached to the journal pool yet, jnlpool_ctl better be NULL */
	assert((JNLPOOL_SEGMENT != pool_type) || ((NULL == jnlpool) || (NULL == jnlpool->jnlpool_ctl)));
	if (JNLPOOL_SEGMENT == pool_type)
	{	/* Initialize variables to simulate a "jnlpool_init". This is required by "repl_inst_flush_jnlpool" called below */
		SET_JNLPOOL_FROM_RECVPOOL_P(jnlpool);
		jnlpool->jnlpool_ctl = (jnlpool_ctl_ptr_t)start_addr;
		assert(NULL != jnlpool->jnlpool_dummy_reg);
		udi = FILE_INFO(jnlpool->jnlpool_dummy_reg);
		csa = &udi->s_addrs;
		csa->critical = (mutex_struct_ptr_t)((sm_uc_ptr_t)jnlpool->jnlpool_ctl + JNLPOOL_CTL_SIZE);
		csa->nl = (node_local_ptr_t)((sm_uc_ptr_t)csa->critical + JNLPOOL_CRIT_SPACE + SIZEOF(mutex_spin_parms_struct));
		/* secshr_db_clnup uses this relationship */
		assert(jnlpool->jnlpool_ctl->filehdr_off);
		assert(jnlpool->jnlpool_ctl->srclcl_array_off > jnlpool->jnlpool_ctl->filehdr_off);
		assert(jnlpool->jnlpool_ctl->sourcelocal_array_off > jnlpool->jnlpool_ctl->srclcl_array_off);
		/* Initialize "jnlpool->repl_inst_filehdr" and related fields as "repl_inst_flush_jnlpool" relies on that */
		jnlpool->repl_inst_filehdr = (repl_inst_hdr_ptr_t)((sm_uc_ptr_t)jnlpool->jnlpool_ctl
									+ jnlpool->jnlpool_ctl->filehdr_off);
		jnlpool->gtmsrc_lcl_array = (gtmsrc_lcl_ptr_t)((sm_uc_ptr_t)jnlpool->jnlpool_ctl
									+ jnlpool->jnlpool_ctl->srclcl_array_off);
		jnlpool->gtmsource_local_array = (gtmsource_local_ptr_t)((sm_uc_ptr_t)jnlpool->jnlpool_ctl
										+ jnlpool->jnlpool_ctl->sourcelocal_array_off);
		if (0 == nattch)
		{	/* No one attached. So, we can safely flush the journal pool so that the gtmsrc_lcl structures in the
			 * jnlpool and disk are in sync with each other. More importantly we are about to remove the jnlpool
			 * so we better get things in sync before that. If anticipatory freeze scheme is in effect, then we
			 * need to keep the journal pool up and running. So, don't reset the crash field in the instance file
			 * header (dictated by the second parameter to repl_inst_flush_jnlpool below).
			 * Note:
			 * If mu_rndwn_repl_instance created new semaphores (in mu_replpool_remove_sem), we need to flush those
			 * to the instance file as well. So, override the jnlpool_semid and jnlpool_semid_ctime with the new
			 * values.
			 */
			assert((INVALID_SEMID != repl_inst_filehdr->jnlpool_semid)
					&& (0 != repl_inst_filehdr->jnlpool_semid_ctime));
			jnlpool->repl_inst_filehdr->jnlpool_semid = repl_inst_filehdr->jnlpool_semid;
			jnlpool->repl_inst_filehdr->jnlpool_semid_ctime = repl_inst_filehdr->jnlpool_semid_ctime;
			repl_inst_flush_jnlpool(FALSE, reset_crash);
			assert(!jnlpool->repl_inst_filehdr->crash || !reset_crash);
			/* Refresh local copy (repl_inst_filehdr) with the copy that was just
				flushed (jnlpool->repl_inst_filehdr) */
			memcpy(repl_inst_filehdr, jnlpool->repl_inst_filehdr, SIZEOF(repl_inst_hdr));
			if (reset_crash)
			{ 	/* Now that jnlpool has been flushed and there is going to be no journal pool, reset
				 * "jnlpool->repl_inst_filehdr" as otherwise other routines (e.g. "repl_inst_recvpool_reset") are
				 * affected by whether this is NULL or not.
				 */
				JNLPOOL_CLEAR_FIELDS(jnlpool);
			}
		} /* else we are ONLINE ROLLBACK. repl_inst_flush_jnlpool will be done later after gvcst_init in mur_open_files */
	}
	if ((0 == nattch) && (reset_crash || (RECVPOOL_SEGMENT == pool_type)))
	{
		if (-1 == shmdt((caddr_t)start_addr))
		{
			save_errno = errno;
			ISSUE_REPLPOOLINST(save_errno, shm_id, instfilename, "shmdt()");
			return -1;
		}
		if (0 != shm_rmid(shm_id))
		{
			save_errno = errno;
			ISSUE_REPLPOOLINST(save_errno, shm_id, instfilename, "shm_rmid()");
			return -1;
		}
		if (JNLPOOL_SEGMENT == pool_type)
		{
			repl_inst_filehdr->jnlpool_shmid = INVALID_SHMID;
			repl_inst_filehdr->jnlpool_shmid_ctime = 0;
			assert((NULL == jnlpool) || ((NULL == jnlpool->jnlpool_ctl)));
			*ipc_rmvd = TRUE;
		} else
		{
			repl_inst_filehdr->recvpool_shmid = INVALID_SHMID;
			repl_inst_filehdr->recvpool_shmid_ctime = 0;
			if ((NULL != jnlpool) && (NULL != jnlpool->repl_inst_filehdr))
			{
				jnlpool->repl_inst_filehdr->recvpool_shmid = INVALID_SHMID;
				jnlpool->repl_inst_filehdr->recvpool_shmid_ctime = 0;
			}
			*ipc_rmvd = TRUE;
		}
	} else
	{
		assert((JNLPOOL_SEGMENT != pool_type) || ((NULL != jnlpool) && (NULL != jnlpool->jnlpool_ctl)));
		if (JNLPOOL_SEGMENT == pool_type)
		{
			*ipc_rmvd = FALSE;
			/* Caller can be MUPIP RUNDOWN (-reg * OR argumentless) or MUPIP ROLLBACK (online or standalone flavor).
			 * In all cases except the argumentless MUPIP RUNDOWN case return with jnlpool_ctl as is. Here is why.
			 * In the rollback case, we are ONLINE ROLLBACK OR anticipatory freeze is in effect and so we want to
			 * keep the journal pool available for the duration of the rollback (to record errors and trigger instance
			 * freeze). Do not detach from the journal pool in that case.
			 * In the rundown -reg * case, anticipatory freeze is in effect and we want to keep the journal pool
			 * available for the duration of the rundown (to record errors and trigger instance freeze). The actual
			 * jnlpool detach will happen in the caller ("mupip_rundown") once all regions have been rundown.
			 * In the argumentless rundown case, detach from the jnlpool. This does not honor the custom errors scheme
			 * and we do not want to be attached to a LOT of journal pools as the argumentless rundown proceeds
			 * (virtual memory bloat etc.).
			 */
			if (argumentless_rundown)
			{
				JNLPOOL_SHMDT(jnlpool, status, save_errno);
				assert(0 == status);	/* even if shmdt fails, there is not much we can do so move on in pro */
			}
		}
		if (RECVPOOL_SEGMENT == pool_type)
			*ipc_rmvd = FALSE;
	}
	return 0;
}

int 	mu_rndwn_replpool(replpool_identifier *replpool_id, repl_inst_hdr_ptr_t repl_inst_filehdr, int shm_id, boolean_t *ipc_rmvd)
{
	int			status, save_errno, nattch;
	char			*instfilename;
	sm_uc_ptr_t		start_addr;
	struct shmid_ds		shm_buf;
	boolean_t		force_attach;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	assert(INVALID_SHMID != shm_id);
	instfilename = replpool_id->instfilename;
	assert((JNLPOOL_SEGMENT == replpool_id->pool_type) || (RECVPOOL_SEGMENT == replpool_id->pool_type));
	assert(!jgbl.mur_rollback || !jgbl.mur_options_forward); /* ROLLBACK -FORWARD should not call this function */
	force_attach = (jgbl.onlnrlbk || (!jgbl.mur_rollback && !argumentless_rundown && INST_FREEZE_ON_ERROR_POLICY));
	if (-1 == shmctl(shm_id, IPC_STAT, &shm_buf))
	{
		save_errno = errno;
		ISSUE_REPLPOOLINST(save_errno, shm_id, instfilename, "shmctl()");
		return -1;
	}
	nattch = shm_buf.shm_nattch;
	if ((0 != nattch) && !force_attach)
	{
		util_out_print("Replpool segment (id = !UL) for replication instance !AD is in use by another process.",
				TRUE, shm_id, LEN_AND_STR(instfilename));
		return -1;
	}
	if (-1 == (sm_long_t)(start_addr = (sm_uc_ptr_t) do_shmat(shm_id, 0, 0)))
	{
		save_errno = errno;
		ISSUE_REPLPOOLINST(save_errno, shm_id, instfilename, "shmat()");
		return -1;
	}
	ESTABLISH_RET(mu_rndwn_replpool_ch, -1);
	status = mu_rndwn_replpool2(replpool_id, repl_inst_filehdr, shm_id, ipc_rmvd, instfilename, start_addr, nattch);
	REVERT;
	return status;
}

CONDITION_HANDLER(mu_rndwn_replpool_ch)
{
	unix_db_info		*udi;
	int			status, save_errno;
	int			jnlpool_shmid;

	START_CH(TRUE);
	PRN_ERROR; /* flush the error string */
	if (SEVERITY == SEVERE)
		NEXTCH;
	/* If we have a journal pool, detach it to prevent leaking the shared memory.
	 * However, if IFOE is configured, we may need the journal pool attached so that we can check for instance freeze
	 * in database rundown.
	 * In that case, the detach will happen automatically when the process terminates.
	 * On the other hand, we don't respect IFOE in argumentless rundown, so go ahead and detach in that case, since otherwise
	 * we could potentially leak the shared memory of multiple journal pools.
	 */
	if (((NULL != jnlpool) && (NULL != jnlpool->jnlpool_ctl)) && (!INST_FREEZE_ON_ERROR_POLICY || argumentless_rundown))
	{
		jnlpool_shmid = jnlpool->repl_inst_filehdr->jnlpool_shmid;
		JNLPOOL_SHMDT(jnlpool, status, save_errno);
		if (0 > status)
		{
			assert(NULL != jnlpool->jnlpool_dummy_reg);
			udi = FILE_INFO(jnlpool->jnlpool_dummy_reg);
			assert(ERR_REPLINSTOPEN == SIGNAL); /* only reason we know why mu_rndwn_replpool can fail */
			ISSUE_REPLPOOLINST(save_errno, jnlpool_shmid, udi->fn, "shmdt()");
		}
	}
	UNWIND(NULL, NULL);
}
