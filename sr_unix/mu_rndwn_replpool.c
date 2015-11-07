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

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	uint4			mutex_per_process_init_pid;
GBLREF	uint4			process_id;
GBLREF	mur_gbls_t		murgbl;
GBLREF	boolean_t		argumentless_rundown;

LITREF char             	gtm_release_name[];
LITREF int4             	gtm_release_name_len;

error_def(ERR_REPLACCSEM);
error_def(ERR_REPLINSTOPEN);
error_def(ERR_REPLPOOLINST);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

#define ISSUE_REPLPOOLINST_AND_RETURN(SAVE_ERRNO, SHM_ID, INSTFILENAME, FAILED_OP)				\
{														\
	ISSUE_REPLPOOLINST(SAVE_ERRNO, SHM_ID, INSTFILENAME, FAILED_OP)						\
	return -1;												\
}

#define DETACH_AND_RETURN(START_ADDR, SHM_ID, INSTFILENAME)							\
{														\
	int		lcl_save_errno;										\
														\
	if (-1 == shmdt((void *)START_ADDR))									\
	{													\
		lcl_save_errno = errno; 									\
		ISSUE_REPLPOOLINST_AND_RETURN(lcl_save_errno, SHM_ID, INSTFILENAME, "shmdt()");			\
	}													\
	return -1;												\
}

int 	mu_rndwn_replpool(replpool_identifier *replpool_id, repl_inst_hdr_ptr_t repl_inst_filehdr, int shm_id, boolean_t *ipc_rmvd)
{
	int			semval, status, save_errno, nattch;
	char			*instfilename, pool_type;
	sm_uc_ptr_t		start_addr;
	struct shmid_ds		shm_buf;
	unix_db_info		*udi;
	sgmnt_addrs		*csa;
	boolean_t		anticipatory_freeze_available, force_attach;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(INVALID_SHMID != shm_id);
	instfilename = replpool_id->instfilename;
	pool_type = replpool_id->pool_type;
	assert((JNLPOOL_SEGMENT == pool_type) || (RECVPOOL_SEGMENT == pool_type));
	anticipatory_freeze_available = ANTICIPATORY_FREEZE_AVAILABLE;
	force_attach = (jgbl.onlnrlbk || (!jgbl.mur_rollback && !argumentless_rundown && anticipatory_freeze_available));
	if (-1 == shmctl(shm_id, IPC_STAT, &shm_buf))
	{
		save_errno = errno;
		ISSUE_REPLPOOLINST_AND_RETURN(save_errno, shm_id, instfilename, "shmctl()");
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
		ISSUE_REPLPOOLINST_AND_RETURN(save_errno, shm_id, instfilename, "shmat()");
	}
	ESTABLISH_RET(mu_rndwn_replpool_ch, -1);
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
		DETACH_AND_RETURN(start_addr, shm_id, instfilename);
	}
	if (memcmp(replpool_id->now_running, gtm_release_name, gtm_release_name_len + 1))
	{
		util_out_print("Attempt to access with version !AD, while already using !AD for replpool segment (id = !UL)"
				" belonging to replication instance !AD.", TRUE, gtm_release_name_len, gtm_release_name,
				LEN_AND_STR(replpool_id->now_running), shm_id, LEN_AND_STR(instfilename));
		DETACH_AND_RETURN(start_addr, shm_id, instfilename);
	}
	/* Assert that if we haven't yet attached to the journal pool yet, jnlpool_ctl better be NULL */
	assert((JNLPOOL_SEGMENT != pool_type) || (NULL == jnlpool.jnlpool_ctl));
	if (JNLPOOL_SEGMENT == pool_type)
	{	/* Initialize variables to simulate a "jnlpool_init". This is required by "repl_inst_flush_jnlpool" called below */
		jnlpool_ctl = jnlpool.jnlpool_ctl = (jnlpool_ctl_ptr_t)start_addr;
		assert(NULL != jnlpool.jnlpool_dummy_reg);
		udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
		csa = &udi->s_addrs;
		csa->critical = (mutex_struct_ptr_t)((sm_uc_ptr_t)jnlpool.jnlpool_ctl + JNLPOOL_CTL_SIZE);
		csa->nl = (node_local_ptr_t)((sm_uc_ptr_t)csa->critical + JNLPOOL_CRIT_SPACE + SIZEOF(mutex_spin_parms_struct));
		/* secshr_db_clnup uses this relationship */
		assert(jnlpool.jnlpool_ctl->filehdr_off);
		assert(jnlpool.jnlpool_ctl->srclcl_array_off > jnlpool.jnlpool_ctl->filehdr_off);
		assert(jnlpool.jnlpool_ctl->sourcelocal_array_off > jnlpool.jnlpool_ctl->srclcl_array_off);
		/* Initialize "jnlpool.repl_inst_filehdr" and related fields as "repl_inst_flush_jnlpool" relies on that */
		jnlpool.repl_inst_filehdr = (repl_inst_hdr_ptr_t)((sm_uc_ptr_t)jnlpool.jnlpool_ctl
									+ jnlpool.jnlpool_ctl->filehdr_off);
		jnlpool.gtmsrc_lcl_array = (gtmsrc_lcl_ptr_t)((sm_uc_ptr_t)jnlpool.jnlpool_ctl
									+ jnlpool.jnlpool_ctl->srclcl_array_off);
		jnlpool.gtmsource_local_array = (gtmsource_local_ptr_t)((sm_uc_ptr_t)jnlpool.jnlpool_ctl
										+ jnlpool.jnlpool_ctl->sourcelocal_array_off);
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
			jnlpool.repl_inst_filehdr->jnlpool_semid = repl_inst_filehdr->jnlpool_semid;
			jnlpool.repl_inst_filehdr->jnlpool_semid_ctime = repl_inst_filehdr->jnlpool_semid_ctime;
			repl_inst_flush_jnlpool(FALSE, !anticipatory_freeze_available);
			assert(!jnlpool.repl_inst_filehdr->crash || anticipatory_freeze_available);
			/* Refresh local copy (repl_inst_filehdr) with the copy that was just flushed (jnlpool.repl_inst_filehdr) */
			memcpy(repl_inst_filehdr, jnlpool.repl_inst_filehdr, SIZEOF(repl_inst_hdr));
			if (!anticipatory_freeze_available || argumentless_rundown)
			{ 	/* Now that jnlpool has been flushed and there is going to be no journal pool, reset
				 * "jnlpool.repl_inst_filehdr" as otherwise other routines (e.g. "repl_inst_recvpool_reset") are
				 * affected by whether this is NULL or not.
				 */
				jnlpool.jnlpool_ctl = NULL;
				jnlpool_ctl = NULL;
				jnlpool.gtmsrc_lcl_array = NULL;
				jnlpool.gtmsource_local_array = NULL;
				jnlpool.jnldata_base = NULL;
				jnlpool.repl_inst_filehdr = NULL;
			}
		} /* else we are ONLINE ROLLBACK. repl_inst_flush_jnlpool will be done later after gvcst_init in mur_open_files */
	}
	if ((0 == nattch) && (!anticipatory_freeze_available || argumentless_rundown || (RECVPOOL_SEGMENT == pool_type)))
	{
		if (-1 == shmdt((caddr_t)start_addr))
		{
			save_errno = errno;
			ISSUE_REPLPOOLINST_AND_RETURN(save_errno, shm_id, instfilename, "shmdt()");
		}
		if (0 != shm_rmid(shm_id))
		{
			save_errno = errno;
			ISSUE_REPLPOOLINST_AND_RETURN(save_errno, shm_id, instfilename, "shm_rmid()");
		}
		if (JNLPOOL_SEGMENT == pool_type)
		{
			repl_inst_filehdr->jnlpool_shmid = INVALID_SHMID;
			repl_inst_filehdr->jnlpool_shmid_ctime = 0;
			assert((NULL == jnlpool.jnlpool_ctl) && (NULL == jnlpool_ctl));
			*ipc_rmvd = TRUE;
		} else
		{
			repl_inst_filehdr->recvpool_shmid = INVALID_SHMID;
			repl_inst_filehdr->recvpool_shmid_ctime = 0;
			if (NULL != jnlpool.repl_inst_filehdr)
			{
				jnlpool.repl_inst_filehdr->recvpool_shmid = INVALID_SHMID;
				jnlpool.repl_inst_filehdr->recvpool_shmid_ctime = 0;
			}
			*ipc_rmvd = TRUE;
		}
	} else
	{	/* Else we are ONLINE ROLLBACK or anticipatory freeze is in effect and so we want to keep the journal pool available
		 * for the duration of the rollback. Do not remove and/or reset the fields in the file header
	   	 */
		assert((JNLPOOL_SEGMENT != pool_type) || ((NULL != jnlpool.jnlpool_ctl) && (NULL != jnlpool_ctl)));
		if (JNLPOOL_SEGMENT == pool_type)
			*ipc_rmvd = FALSE;
		if (RECVPOOL_SEGMENT == pool_type)
			*ipc_rmvd = FALSE;
	}
	REVERT;
	return 0;
}

CONDITION_HANDLER(mu_rndwn_replpool_ch)
{
	unix_db_info		*udi;
	int			status, save_errno;
	repl_inst_hdr_ptr_t	inst_hdr;

	START_CH;
	PRN_ERROR; /* flush the error string */
	if (SEVERITY == SEVERE)
		NEXTCH;
	if (NULL != jnlpool.jnlpool_ctl)
	{
		JNLPOOL_SHMDT(status, save_errno);
		if (0 > status)
		{
			inst_hdr = jnlpool.repl_inst_filehdr;
			assert(NULL != inst_hdr);
			assert(NULL != jnlpool.jnlpool_dummy_reg);
			udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
			assert(ERR_REPLINSTOPEN == SIGNAL); /* only reason we know why mu_rndwn_replpool can fail */
			assert(NULL != jnlpool.jnlpool_ctl);
			assert(INVALID_SHMID != inst_hdr->jnlpool_shmid);
			ISSUE_REPLPOOLINST(save_errno, inst_hdr->jnlpool_shmid, udi->fn, "shmdt()");
		}
		jnlpool.gtmsrc_lcl_array = NULL;
		jnlpool.gtmsource_local_array = NULL;
		jnlpool.jnldata_base = NULL;
		jnlpool.repl_inst_filehdr = NULL;
	}
	UNWIND(NULL, NULL);
}
