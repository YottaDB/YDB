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

#include "gtm_inet.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"

#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <errno.h>

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "repl_msg.h"
#include "repl_instance.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "iosp.h"
#include "gtm_stdio.h"
#include "gtmio.h"
#include "gtm_string.h"
#include "gtm_logicals.h"
#include "repl_sem.h"
#include "mu_rndwn_replpool.h"
#include "mu_rndwn_repl_instance.h"
#include "mu_gv_cur_reg_init.h"
#include "gtm_sem.h"
#include "gtmmsg.h"
#include "gtm_ipc.h"
#include "eintr_wrappers.h"
#include "ftok_sems.h"
#include "mu_rndwn_all.h"
#include "util.h"
#include "ipcrmid.h"	/* for sem_rmid() prototype */
#include "buddy_list.h"		/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "error.h"
#include "anticipatory_freeze.h"
#include "heartbeat_timer.h"
#include "mutex.h"

GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	recvpool_addrs		recvpool;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gd_region		*ftok_sem_reg;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	mur_opt_struct		mur_options;
GBLREF	mur_gbls_t		murgbl;
GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];

error_def(ERR_MUJPOOLRNDWNFL);
error_def(ERR_MUJPOOLRNDWNSUC);
error_def(ERR_MURPOOLRNDWNFL);
error_def(ERR_MURPOOLRNDWNSUC);
error_def(ERR_REPLACCSEM);
error_def(ERR_SEMREMOVED);
error_def(ERR_SYSCALL);

/*
 * This will rundown a replication instance journal (and receiver) pool.
 *	Input Parameter:
 *		replpool_id of the instance. Instance file name must be null terminated in replpool_id.
 * Returns :
 *	TRUE,  if successful.
 *	FALSE, otherwise.
 */
boolean_t mu_rndwn_repl_instance(replpool_identifier *replpool_id, boolean_t immediate, boolean_t rndwn_both_pools,
					boolean_t *jnlpool_sem_created)
{
	boolean_t		jnlpool_stat = SS_NORMAL, recvpool_stat = SS_NORMAL, decr_cnt, sem_created = FALSE, ipc_rmvd;
	char			*instfilename;
	unsigned char		ipcs_buff[MAX_IPCS_ID_BUF], *ipcs_ptr;
	gd_region		*r_save;
	repl_inst_hdr		repl_instance;
	static	gd_region	*reg = NULL;
	struct semid_ds		semstat;
	struct shmid_ds		shmstat;
	unix_db_info		*udi;
	int			save_errno, sem_id, shm_id, status;
	sgmnt_addrs		*repl_csa;
	boolean_t		was_crit;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (NULL == reg)
	{
		r_save = gv_cur_region;
		mu_gv_cur_reg_init();
		reg = gv_cur_region;
		gv_cur_region = r_save;
	}
	*jnlpool_sem_created = FALSE;
	/* Assert that the layout of replpool_identifier is identical for all versions going forward as the function
	 * "validate_replpool_shm_entry" (used by the argumentless mupip rundown aka "mupip rundown") relies on this.
	 * This assert is placed here (instead of there) because the automated tests exercise this logic much more
	 * than the argumentless code. If any of these asserts fail, "validate_replpool_shm_entry" needs to change
	 * to handle the old and new layouts.
	 *
	 *	Structure ----> replpool_identifier <----    size 312 [0x0138]
	 *
	 *		offset = 0000 [0x0000]      size = 0012 [0x000c]    ----> replpool_identifier.label
	 *		offset = 0012 [0x000c]      size = 0001 [0x0001]    ----> replpool_identifier.pool_type
	 *		offset = 0013 [0x000d]      size = 0036 [0x0024]    ----> replpool_identifier.now_running
	 *		offset = 0052 [0x0034]      size = 0004 [0x0004]    ----> replpool_identifier.repl_pool_key_filler
	 *		offset = 0056 [0x0038]      size = 0256 [0x0100]    ----> replpool_identifier.instfilename
	 */
	assert(0 == OFFSETOF(replpool_identifier, label[0]));
	assert(12 == SIZEOF(((replpool_identifier *)NULL)->label));
	assert(12 == OFFSETOF(replpool_identifier, pool_type));
	assert(1 == SIZEOF(((replpool_identifier *)NULL)->pool_type));
	assert(13 == OFFSETOF(replpool_identifier, now_running[0]));
	assert(36 == SIZEOF(((replpool_identifier *)NULL)->now_running));
	assert(56 == OFFSETOF(replpool_identifier, instfilename[0]));
	assert(256 == SIZEOF(((replpool_identifier *)NULL)->instfilename));
	/* End asserts */
	jnlpool.jnlpool_dummy_reg = reg;
	recvpool.recvpool_dummy_reg = reg;
	instfilename = replpool_id->instfilename;
	reg->dyn.addr->fname_len = strlen(instfilename);
	assert(0 == instfilename[reg->dyn.addr->fname_len]);
	memcpy((char *)reg->dyn.addr->fname, instfilename, reg->dyn.addr->fname_len + 1);
	udi = FILE_INFO(reg);
	udi->fn = (char *)reg->dyn.addr->fname;
	/* Lock replication instance using ftok semaphore so that no other replication process can startup until we are done with
	 * rundown
	 */
	if (!ftok_sem_get(reg, TRUE, REPLPOOL_ID, immediate))
		return FALSE;
	ESTABLISH_RET(mu_rndwn_repl_instance_ch, FALSE);
	repl_inst_read(instfilename, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
	assert(rndwn_both_pools || JNLPOOL_SEGMENT == replpool_id->pool_type || RECVPOOL_SEGMENT == replpool_id->pool_type);
	if (rndwn_both_pools || (JNLPOOL_SEGMENT == replpool_id->pool_type))
	{	/* --------------------------
		 * First rundown Journal pool
		 * --------------------------
		 */
		shm_id = repl_instance.jnlpool_shmid;
		if (SS_NORMAL == (jnlpool_stat = mu_replpool_grab_sem(&repl_instance, JNLPOOL_SEGMENT, &sem_created, immediate)))
		{
			/* Got JNL_POOL_ACCESS_SEM and incremented SRC_SRV_COUNT_SEM */
			assert(holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
			assert(holds_sem[SOURCE][SRC_SERV_COUNT_SEM]);
			sem_id = repl_instance.jnlpool_semid;
			if ((INVALID_SHMID == shm_id) || (-1 == shmctl(shm_id, IPC_STAT, &shmstat))
				|| (shmstat.shm_ctime != repl_instance.jnlpool_shmid_ctime))
			{
				repl_instance.jnlpool_shmid = shm_id = INVALID_SHMID;
				repl_instance.jnlpool_shmid_ctime = 0;
			}
			assert((INVALID_SHMID != shm_id) || ((NULL == jnlpool.jnlpool_ctl) && (NULL == jnlpool_ctl)));
			ipc_rmvd = TRUE;
			if (INVALID_SHMID != shm_id)
			{
				replpool_id->pool_type = JNLPOOL_SEGMENT;
				jnlpool_stat = mu_rndwn_replpool(replpool_id, &repl_instance, shm_id, &ipc_rmvd);
				ipcs_ptr = i2asc((uchar_ptr_t)ipcs_buff, shm_id);
				*ipcs_ptr = '\0';
				if (rndwn_both_pools && ((SS_NORMAL != jnlpool_stat) || ipc_rmvd))
					gtm_putmsg(VARLSTCNT(6) (jnlpool_stat ? ERR_MUJPOOLRNDWNFL : ERR_MUJPOOLRNDWNSUC),
						4, LEN_AND_STR(ipcs_buff), LEN_AND_STR(instfilename));
			}
			assert(ipc_rmvd || (NULL != jnlpool_ctl));
			assert((NULL == jnlpool.jnlpool_ctl) || (SS_NORMAL == jnlpool_stat) || jgbl.onlnrlbk);
			assert((INVALID_SHMID != repl_instance.jnlpool_shmid) || (0 == repl_instance.jnlpool_shmid_ctime));
			assert((INVALID_SHMID == repl_instance.jnlpool_shmid) || (0 != repl_instance.jnlpool_shmid_ctime));
			assert(INVALID_SEMID != sem_id);
			if (!mur_options.rollback)
			{	/* Invoked by MUPIP RUNDOWN in which case the semaphores needs to be removed. But, remove the
				 * semaphore ONLY if we created it here OR the journal pool was successfully removed.
				 */
				if (NULL == jnlpool_ctl)
				{
					if (((sem_created || (SS_NORMAL == jnlpool_stat))
						&& (SS_NORMAL == mu_replpool_release_sem(&repl_instance, JNLPOOL_SEGMENT, TRUE))))
					{	/* Now that semaphores are removed, reset fields in file header */
						if (!sem_created)
						{	/* If sem_id was created by mu_replpool_grab_sem then do NOT report the
							 * MURPOOLRNDWNSUC message as it indicates that the semaphore was orphaned
							 * and we removed it when in fact there was no orphaned semaphore and we
							 * created it as part of mu_replpool_grab_sem to get standalone access to
							 * rundown the receiver pool (which may or may not exist)
							 */
							ipcs_ptr = i2asc((uchar_ptr_t)ipcs_buff, sem_id);
							*ipcs_ptr = '\0';
							gtm_putmsg(VARLSTCNT(9) ERR_MUJPOOLRNDWNSUC, 4, LEN_AND_STR(ipcs_buff),
								LEN_AND_STR(instfilename), ERR_SEMREMOVED, 1, sem_id);
						}
						repl_inst_jnlpool_reset();
					}
				} else
				{	/* Anticipatory Freeze scheme is turned ON. So, release just the JNL_POOL_ACCESS_SEM. The
					 * semaphore will be released/removed in the caller (mupip_rundown)
					 */
					assert(ANTICIPATORY_FREEZE_AVAILABLE);
					assertpro(SS_NORMAL == (status = rel_sem(SOURCE, JNL_POOL_ACCESS_SEM)));
					assert(!holds_sem[SOURCE][JNL_POOL_ACCESS_SEM]);
					/* Since we are not resetting the semaphore IDs in the file header, we need to write out
					 * the semaphore IDs in the instance file (if we created them).
					 */
					if (sem_created)
						repl_inst_write(instfilename, (off_t)0, (sm_uc_ptr_t)&repl_instance,
									SIZEOF(repl_inst_hdr));
				}
				/* If semaphore is not created and the journal pool rundown failed (due to attached processes),
				 * rundown process continues to holds the journal pool access control semaphore. This way, we hold
				 * the semaphore on behalf of the source server (now no longer alive) to prevent mu_rndwn_sem_all
				 * (invoked later) from cleaning up this orphaned semaphore (which causes REPLREQROLLBACK if the
				 * source server is restarted). But, since the semaphore is not released (until the rundown process
				 * dies), holds_sem[SOURCE][JNL_POOL_ACCESS_SEM] continues to remain TRUE. This causes asserts in
				 * ftok_sem_get if mu_rndwn_repl_instance is invoked for a different journal/receive pool. To
				 * workaround it, set holds_sem[SOURCE][JNL_POOL_ACCESS_SEM] to FALSE. This is an interim solution
				 * until we record such semaphores in an ignore-list (or some such) and change mu_rndwn_sem_all to
				 * skip the ones that are present in the ignore list.
				 */
				holds_sem[SOURCE][JNL_POOL_ACCESS_SEM] = FALSE;
			}
		} else if (rndwn_both_pools && (INVALID_SHMID != shm_id))
		{
			ipcs_ptr = i2asc((uchar_ptr_t)ipcs_buff, shm_id);
			*ipcs_ptr = '\0';
			if (rndwn_both_pools)
				gtm_putmsg(VARLSTCNT(6) ERR_MUJPOOLRNDWNFL, 4, LEN_AND_STR(ipcs_buff),
					LEN_AND_STR(instfilename));
		}
		*jnlpool_sem_created = sem_created;
	}
	if (((SS_NORMAL == jnlpool_stat) || !jgbl.mur_rollback) &&
		(rndwn_both_pools || (RECVPOOL_SEGMENT == replpool_id->pool_type)))
	{	/* --------------------------
		 * Now rundown Receivpool
		 * --------------------------
		 * Note: RECVPOOL is rundown ONLY if the JNLPOOL rundown was successful. This way, we don't end up
		 * creating new semaphores for the RECVPOOL if ROLLBACK is not going to start anyways because of the failed
		 * JNLPOOL rundown. The only exception is MUPIP RUNDOWN command in which case we try running down the
		 * RECVPOOL even if the JNLPOOL rundown failed.
		 */
		shm_id = repl_instance.recvpool_shmid;
		if (SS_NORMAL == (recvpool_stat = mu_replpool_grab_sem(&repl_instance, RECVPOOL_SEGMENT, &sem_created, immediate)))
		{
			sem_id = repl_instance.recvpool_semid;
			if ((INVALID_SHMID == shm_id) || (-1 == shmctl(shm_id, IPC_STAT, &shmstat))
				|| (shmstat.shm_ctime != repl_instance.recvpool_shmid_ctime))
			{
				repl_instance.recvpool_shmid = shm_id = INVALID_SHMID;
				repl_instance.recvpool_shmid_ctime = 0;
			}
			ipc_rmvd = TRUE;
			if (INVALID_SHMID != shm_id)
			{
				replpool_id->pool_type = RECVPOOL_SEGMENT;
				recvpool_stat = mu_rndwn_replpool(replpool_id, &repl_instance, shm_id, &ipc_rmvd);
				ipcs_ptr = i2asc((uchar_ptr_t)ipcs_buff, shm_id);
				*ipcs_ptr = '\0';
				if (rndwn_both_pools && ((SS_NORMAL != recvpool_stat) || ipc_rmvd))
					gtm_putmsg(VARLSTCNT(6) (recvpool_stat ? ERR_MURPOOLRNDWNFL : ERR_MURPOOLRNDWNSUC),
						4, LEN_AND_STR(ipcs_buff), LEN_AND_STR(instfilename));
			}
			assert((TRUE == ipc_rmvd) || (SS_NORMAL != recvpool_stat) || jgbl.onlnrlbk);
			assert((INVALID_SHMID != repl_instance.recvpool_shmid) || (0 == repl_instance.recvpool_shmid_ctime));
			assert((INVALID_SHMID == repl_instance.recvpool_shmid) || (0 != repl_instance.recvpool_shmid_ctime));
			assert(INVALID_SEMID != sem_id);
			if (!mur_options.rollback)
			{	/* Invoked by MUPIP RUNDOWN in which case the semaphores needs to be removed. But, remove the
				 * semaphore ONLY if we created it here OR the receive pool was successfully removed.
				 */
				if ((sem_created || (SS_NORMAL == recvpool_stat))
					&& (SS_NORMAL == mu_replpool_release_sem(&repl_instance, RECVPOOL_SEGMENT, TRUE)))
				{	/* Now that semaphores are removed, reset fields in file header */
					if (!sem_created)
					{	/* if sem_id was "created" by mu_replpool_grab_sem then do NOT report the
						 * MURPOOLRNDWNSUC message as it indicates that the semaphore was orphaned and we
						 * removed it when in fact there was no orphaned semaphore and we "created" it as
						 * part of mu_replpool_grab_sem to get standalone access to rundown the receiver
						 * pool (which may or may not exist)
						 */
						ipcs_ptr = i2asc((uchar_ptr_t)ipcs_buff, sem_id);
						*ipcs_ptr = '\0';
						gtm_putmsg(VARLSTCNT(9) ERR_MURPOOLRNDWNSUC, 4, LEN_AND_STR(ipcs_buff),
							LEN_AND_STR(instfilename), ERR_SEMREMOVED, 1, sem_id);
					}
					if (NULL != jnlpool_ctl)
					{	/* Journal pool is not yet removed. So, grab lock before resetting semid/shmid
						 * fields in the file header as the function expects the caller to hold crit
						 * if the journal pool is available
						 */
						repl_csa = &FILE_INFO(jnlpool.jnlpool_dummy_reg)->s_addrs;
						assert(!repl_csa->now_crit);
						assert(!repl_csa->hold_onto_crit);
						was_crit = repl_csa->now_crit;
						/* Since we do grab_lock, below, we need to do a per-process initialization. Also,
						 * start heartbeat so that grab_lock can issue MUTEXLCKALERT and get C-stacks if
						 * waiting for crit
						 */
						START_HEARTBEAT_IF_NEEDED;
						mutex_per_process_init();
						if (!was_crit)
							grab_lock(jnlpool.jnlpool_dummy_reg, TRUE, GRAB_LOCK_ONLY);
					}
					repl_inst_recvpool_reset();
					if ((NULL != jnlpool_ctl) && !was_crit)
						rel_lock(jnlpool.jnlpool_dummy_reg);
				}
				/* If semaphore is not created and the receive pool rundown failed (due to attached processes),
				 * rundown process continues to holds the receive pool access control semaphore. This way, we hold
				 * the semaphore on behalf of the receiver server (now no longer alive) to prevent mu_rndwn_sem_all
				 * (invoked later) from cleaning up this orphaned semaphore (which causes REPLREQROLLBACK if the
				 * receiver is restarted). But, since the semaphore is not released (until the rundown process
				 * dies), holds_sem[RECV][RECV_POOL_ACCESS_SEM] continues to remain TRUE. This causes asserts in
				 * ftok_sem_get if mu_rndwn_repl_instance is invoked for a different journal/receive pool. To
				 * workaround it, set holds_sem[SOURCE][RECV_POOL_ACCESS_SEM] to FALSE. This is an interim solution
				 * until we record such semaphores in an ignore-list (or some such) and change mu_rndwn_sem_all to
				 * skip the ones that are present in the ignore list.
				 */
				assert((sem_created || (SS_NORMAL == recvpool_stat)) || holds_sem[RECV][RECV_POOL_ACCESS_SEM]);
				DEBUG_ONLY(set_sem_set_recvr(sem_id));
			}
		} else if (rndwn_both_pools && (INVALID_SHMID != shm_id))
		{
			ipcs_ptr = i2asc((uchar_ptr_t)ipcs_buff, shm_id);
			*ipcs_ptr = '\0';
			if (rndwn_both_pools)
				gtm_putmsg(VARLSTCNT(6) ERR_MURPOOLRNDWNFL, 4, LEN_AND_STR(ipcs_buff),
					LEN_AND_STR(instfilename));
		}
	}
	assert(jgbl.onlnrlbk || ANTICIPATORY_FREEZE_AVAILABLE || (NULL == jnlpool.repl_inst_filehdr));
	if (mur_options.rollback && (SS_NORMAL == jnlpool_stat) && (SS_NORMAL == recvpool_stat))
	{
		assert(jgbl.onlnrlbk || ANTICIPATORY_FREEZE_AVAILABLE || ((INVALID_SHMID == repl_instance.jnlpool_shmid)
			&& (INVALID_SHMID == repl_instance.recvpool_shmid)));
		/* Initialize jnlpool.repl_inst_filehdr as it is used later by gtmrecv_fetchresync() */
		decr_cnt = FALSE;
		if (NULL == jnlpool.repl_inst_filehdr)
		{	/* Possible if there is NO journal pool in the first place. In this case, malloc the structure here and
			 * copy the file header from repl_instance structure.
			 */
			jnlpool.repl_inst_filehdr = (repl_inst_hdr_ptr_t)malloc(SIZEOF(repl_inst_hdr));
			memcpy(jnlpool.repl_inst_filehdr, &repl_instance, SIZEOF(repl_inst_hdr));
		} else
		{
			assert(repl_instance.jnlpool_semid == jnlpool.repl_inst_filehdr->jnlpool_semid);
			assert(repl_instance.jnlpool_semid_ctime == jnlpool.repl_inst_filehdr->jnlpool_semid_ctime);
			assert(repl_instance.jnlpool_shmid == jnlpool.repl_inst_filehdr->jnlpool_shmid);
			assert(repl_instance.jnlpool_shmid_ctime == jnlpool.repl_inst_filehdr->jnlpool_shmid_ctime);
			/* If the ONLINE ROLLBACK command is run on the primary when the source server is up and running,
			 * jnlpool.repl_inst_filehdr->recvpool_semid will be INVALID because there is NO receiver server
			 * running. However, ROLLBACK creates semaphores for both journal pool and receive pool and writes
			 * it to the instance file header. Copy this information to the file header copy in the jnlpool
			 * as well
			 */
			jnlpool.repl_inst_filehdr->recvpool_semid = repl_instance.recvpool_semid;
			jnlpool.repl_inst_filehdr->recvpool_semid_ctime = repl_instance.recvpool_semid_ctime;
		}
		/* Flush changes to the replication instance file header to disk */
		repl_inst_write(instfilename, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
	} else /* for MUPIP RUNDOWN, semid fields in the file header are reset and is written in mu_replpool_release_sem() above */
		decr_cnt = (NULL == jnlpool_ctl); /* for anticipatory freeze, mupip_rundown releases the semaphore */
	REVERT;
	/* Release replication instance ftok semaphore lock */
	if (!ftok_sem_release(reg, decr_cnt, immediate)) /* Do not decrement the counter if ROLLBACK */
		return FALSE;
	return ((SS_NORMAL == jnlpool_stat) && (SS_NORMAL == recvpool_stat));
}

CONDITION_HANDLER(mu_rndwn_repl_instance_ch)
{
	unix_db_info	*udi;
	sgmnt_addrs	*csa;
	gd_region	*reg;

	START_CH;
	reg = jnlpool.jnlpool_dummy_reg;
	assert(NULL != reg);
	if (NULL != reg)
	{
		udi = FILE_INFO(reg);
		csa = &udi->s_addrs;
		if (udi->grabbed_ftok_sem)
			ftok_sem_release(reg, FALSE, TRUE);
	}
	NEXTCH;
}
