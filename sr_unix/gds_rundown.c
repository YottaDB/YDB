/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_ipc.h"
#include "gtm_inet.h"
#include "gtm_fcntl.h"
#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_time.h"
#include "gtm_signal.h"	/* for VSIG_ATOMIC_T type */

#include <sys/sem.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <errno.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblk.h"
#include "gt_timer.h"
#include "jnl.h"
#include "error.h"
#include "iosp.h"
#include "gdsbgtr.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "aswp.h"
#include "gtm_c_stack_trace.h"
#include "eintr_wrapper_semop.h"
#include "util.h"
#include "send_msg.h"
#include "change_reg.h"
#include "compswap.h"
#include "mutex.h"
#include "gds_rundown.h"
#include "gvusr.h"
#include "do_semop.h"
#include "mmseg.h"
#include "ipcrmid.h"
#include "gtmmsg.h"
#include "wcs_recover.h"
#include "wcs_mm_recover.h"
#include "tp_change_reg.h"
#include "wcs_flu.h"
#include "add_inter.h"
#include "io.h"
#include "gtmsecshr.h"
#include "secshr_client.h"
#include "ftok_sems.h"
#include "gtmimagename.h"
#include "gtmio.h"
#include "have_crit.h"
#include "is_proc_alive.h"
#include "shmpool.h"
#include "db_snapshot.h"
#include "ss_lock_facility.h"
#include "anticipatory_freeze.h"
#include "wcs_clean_dbsync.h"
#include "interlock.h"
#include "gds_rundown_err_cleanup.h"
#include "wcs_wt.h"
#include "wcs_sleep.h"
#include "aio_shim.h"
#include "gvcst_protos.h"
#include "targ_alloc.h"
#include "gdskill.h"		/* needed for tp.h */
#include "gdscc.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"

GBLREF	VSIG_ATOMIC_T		forced_exit;
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	boolean_t		is_src_server, is_updproc;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	uint4			process_id;
GBLREF	ipcs_mesg		db_ipcs;
GBLREF	jnl_process_vector	*prc_vec;
GBLREF	jnl_process_vector	*originator_prc_vec;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	boolean_t		dse_running;
GBLREF	int			num_additional_processors;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	int			process_exiting;
GBLREF	boolean_t		ok_to_UNWIND_in_exit_handling;
GBLREF	gv_namehead		*gv_target_list;

LITREF  char                    ydb_release_name[];
LITREF  int4                    ydb_release_name_len;
LITREF gtmImageName		gtmImageNames[];

error_def(ERR_AIOCANCELTIMEOUT);
error_def(ERR_ASSERT);
error_def(ERR_CRITSEMFAIL);
error_def(ERR_DBFILERR);
error_def(ERR_DBRNDWN);
error_def(ERR_DBRNDWNWRN);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_IPCNOTDEL);
error_def(ERR_JNLFLUSH);
error_def(ERR_LASTWRITERBYPAS);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_RESRCINTRLCKBYPAS);
error_def(ERR_RNDWNSEMFAIL);
error_def(ERR_RNDWNSKIPCNT);
error_def(ERR_STACKOFLOW);
error_def(ERR_STACKOFLOW);
error_def(ERR_TEXT);
error_def(ERR_WCBLOCKED);

int4 gds_rundown(boolean_t cleanup_udi)
{
	boolean_t		canceled_dbsync_timer, do_jnlwait, ok_to_write_pfin, wrote_pfin;
	boolean_t		have_standalone_access, ipc_deleted, err_caught, aiocancel_timedout;
	boolean_t		is_cur_process_ss_initiator, remove_shm, vermismatch, we_are_last_user, we_are_last_writer, is_mm;
	boolean_t		unsafe_last_writer;
	boolean_t		db_needs_flushing, csd_read_only;
	char			time_str[CTIME_BEFORE_NL + 2]; /* for GET_CUR_TIME macro */
	gd_region		*reg, *statsDBreg;
	gd_segment		*seg;
	int			save_errno, status, rc;
	int4			semval, ftok_semval, sopcnt, ftok_sopcnt;
	short			crash_count;
	sm_long_t		munmap_len;
	void			*munmap_start;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	struct shmid_ds		shm_buf;
	struct sembuf		sop[2], ftok_sop[2];
	uint4           	jnl_status;
	unix_db_info		*udi;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jbp;
	shm_snapshot_t		*ss_shm_ptr;
	uint4			ss_pid, onln_rlbk_pid, holder_pid;
	boolean_t		is_statsDB, was_crit;
	boolean_t		safe_mode; /* Do not flush or take down shared memory. */
	boolean_t		bypassed_ftok = FALSE, bypassed_access = FALSE, may_bypass_ftok, inst_is_frozen;
	boolean_t		ftok_counter_halted, access_counter_halted;
	int			secshrstat;
	intrpt_state_t		prev_intrpt_state;
	gv_namehead		*currgvt;
	gd_region		*baseDBreg;
	sgmnt_addrs		*baseDBcsa;
	node_local_ptr_t	baseDBnl;
	sgm_info		*si;
	DEBUG_ONLY(boolean_t	orig_we_are_last_writer = FALSE);

	jnl_status = 0;
	reg = gv_cur_region;			/* Local copy */
	/* Early out for cluster regions
	 * to avoid tripping the assert below.
	 * Note:
	 *	This early out is consistent with VMS.  It has been
	 *	noted that all of the gtcm assignments
	 *      to gv_cur_region should use the TP_CHANGE_REG
	 *	macro.  This would also avoid the assert problem
	 *	and should be done eventually.
	 */
	seg = reg->dyn.addr;
	if (dba_cm == seg->acc_meth)
		return EXIT_NRM;
	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	csd = csa->hdr;
	assert((csa == cs_addrs) && (csd == cs_data));	/* relied upon by "jnl_ensure_open" calls below */
	if ((reg->open) && (dba_usr == csd->acc_meth))
	{
		change_reg();
		gvusr_rundown();
		return EXIT_NRM;
	}
	/* If this region has a corresponding statsdb region that is open, close that first. This is needed to ensure
	 * that the statsdb can safely be deleted at basedb rundown time if we happen to be the last one to rundown the basedb.
	 */
	is_statsDB = IS_STATSDB_REG(reg);
	if (!is_statsDB)
	{	/* Note that even if the baseDB has RDBF_NOSTATS set, we could have opened the statsDB region
		 * (for example, if statsDB has read-only permissions, we would have opened it and found it is
		 * read-only when we tried to add the ^%YGS node and would have disabled stats in the baseDB
		 * all the while leaving the statsDB open. So we need to check if it is open and if so run it down
		 * without checking the RDBF_NOSTATS bit in the baseDB.
		 */
		BASEDBREG_TO_STATSDBREG(reg, statsDBreg);
		if (statsDBreg->open)
		{
			gv_cur_region = statsDBreg;	/* Switch "gv_cur_region" to do rundown of statsDB */
			tp_change_reg();
			gds_rundown(cleanup_udi); /* Ignore errors in statsdb rundown. Continue with baseDB rundown. */
			gv_cur_region = reg;	/* Restore "gv_cur_region" back to continue rundown of baseDB */
			tp_change_reg();
			/* Now that statsdb has been rundown, reset basedb stats back to private memory in case it was
			 * pointing to statsdb shared/mapped memory. Note that the following reset of the stats
			 * pointer back to the internal stats buffer located is sgmnt_data is normally taken care
			 * of by the statsdb unlink processing in gvcst_remove_statsDB_linkage() but we keep this
			 * reset here also to be sure it gets done in case of a statsDB rundown issue.
			 */
			csa->gvstats_rec_p = &csa->gvstats_rec;
		}
	}
	csa->regcnt--;
	if (csa->regcnt)
	{	/* There is at least one more region pointing to the same db file as this region.
		 * Defer rundown of this "csa" until the last region corresponding to this csa is called for rundown.
		 */
		reg->open = FALSE;
		return EXIT_NRM;
	}
	if (is_statsDB)
	{	/* This is a statsdb. Fix reg->read_only & csa->read_write based on csa->orig_read_write.
		 * This is so it reflects real permissions this process has on the statsdb.
		 */
		reg->read_only = !csa->orig_read_write;
		csa->read_write = csa->orig_read_write;		/* Maintain read_only/read_write in parallel */
	}
	/* If the process has standalone access, it has udi->grabbed_access_sem set to TRUE at this point. Note that down in a local
	 * variable as the udi->grabbed_access_sem is set to TRUE even for non-standalone access below and hence we can't rely on
	 * that later to determine if the process had standalone access or not when it entered this function.  We need to guarantee
	 * that none else access database file header when semid/shmid fields are reset.  We already have created ftok semaphore in
	 * db_init or, mu_rndwn_file and did not remove it.  So just lock it. We do it in blocking mode.
	 */
	have_standalone_access = udi->grabbed_access_sem; /* process holds standalone access */
	DEFER_INTERRUPTS(INTRPT_IN_GDS_RUNDOWN, prev_intrpt_state);
	ESTABLISH_NORET(gds_rundown_ch, err_caught);
	if (err_caught)
	{
		REVERT;
		WITH_CH(gds_rundown_ch, gds_rundown_err_cleanup(have_standalone_access), 0);
		ENABLE_INTERRUPTS(INTRPT_IN_GDS_RUNDOWN, prev_intrpt_state);
		DEBUG_ONLY(ok_to_UNWIND_in_exit_handling = FALSE);
		return EXIT_ERR;
	}
	assert(reg->open);			/* if we failed to open, dbinit_ch should have taken care of proper clean up */
	assert(!reg->opening);			/* see comment above */
	assert(IS_CSD_BG_OR_MM(csd));
	is_mm = (dba_bg != csd->acc_meth);
	assert(!csa->hold_onto_crit || (csa->now_crit && jgbl.onlnrlbk));
	/* If we are online rollback, we should already be holding crit and should release it only at the end of this module. This
	 * is usually done by noting down csa->now_crit in a local variable (was_crit) and using it whenever we are about to
	 * grab_crit. But, there are instances (like mupip_set_journal.c) where we grab_crit but invoke "gds_rundown" without any
	 * preceeding rel_crit. Such code relies on the fact that "gds_rundown" does rel_crit unconditionally (to get locks to a
	 * known state). So, augment csa->now_crit with jgbl.onlnrlbk to track if we can rel_crit unconditionally or not in
	 * "gds_rundown".
	 */
	was_crit = (csa->now_crit && jgbl.onlnrlbk);
	/* Cancel any pending flush timer for this region by this process */
	canceled_dbsync_timer = FALSE;
	CANCEL_DB_TIMERS(reg, csa, canceled_dbsync_timer);
	we_are_last_user = FALSE;
	inst_is_frozen = IS_REPL_INST_FROZEN && REPL_ALLOWED(csa->hdr);
	if (!csa->persistent_freeze)
		region_freeze(reg, FALSE, FALSE, FALSE, FALSE, FALSE);
	if (!was_crit)
	{
		rel_crit(reg);		/* get locks to known state */
		mutex_cleanup(reg);
	}
	/* The only process that can invoke "gds_rundown" while holding access control semaphore is RECOVER/ROLLBACK. All the
	* others (like MUPIP SET -FILE/MUPIP EXTEND would have invoked db_ipcs_reset() before invoking "gds_rundown" (from
	 * mupip_exit_handler). The only exception is when these processes encounter a terminate signal and they reach
	 * mupip_exit_handler while holding access control semaphore. Assert accordingly.
	 */
	assert(!have_standalone_access || mupip_jnl_recover || process_exiting);
	/* If we have standalone access, then ensure that a concurrent online rollback cannot be running at the same time as it
	 * needs the access control lock as well. The only expection is we are online rollback and currently running down.
	 */
	cnl = csa->nl;
	onln_rlbk_pid = cnl->onln_rlbk_pid;
	csd_read_only = csd->read_only;
	assert(!have_standalone_access || mupip_jnl_recover || !onln_rlbk_pid || !is_proc_alive(onln_rlbk_pid, 0));
	/* If csd_read_only is TRUE, then do not attempt to get the ftok lock. This is because it is possible the ftok semaphore
	 * has been deleted by another process (example scenario below). But we do not need to get the ftok lock in that case
	 * since we are guaranteed to not have updated the db since db open time in "db_init". So skip the "ftok_sem_lock" part.
	 * In this case, it is also okay to skip getting an exclusive lock on the db access control semaphore since it is anyways
	 * a process-only semaphore for the read_only db flag case (no other process that has the same db open knows this semid).
	 *
	 * Example scenario
	 * -----------------
	 *	1) P1 opens db with ftok_semval = 16K.
	 *	2) P2 opens db with ftok_semval = 32K (i.e. overflow) so P2 keeps ftok_semval at 16K.
	 *	3) P1 runs down db and deletes ftok sem. Since P1's cnl->ftok_counter_halted is private memory
	 *		(due to read_only db flag, even if P2 sets cnl->ftok_counter_halted to TRUE, P1 cannot see that).
	 *	4) P2 comes to gds_rundown. It will notice udi->ftok_semid is gone.
	 */
	if (!have_standalone_access && !csd_read_only)
	{
		if (-1 == (ftok_semval = semctl(udi->ftok_semid, DB_COUNTER_SEM, GETVAL))) /* Check # of procs counted on FTOK */
		{
			save_errno = errno;
			assert(FALSE);
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,
				      RTS_ERROR_TEXT("gds_rundown SEMCTL failed to get ftok_semval"), CALLFROM, errno);
		}
		may_bypass_ftok = CAN_BYPASS(ftok_semval, csa, inst_is_frozen); /* Do we need a blocking wait? */
		/* We need to guarantee that no one else access database file header when semid/shmid fields are reset.
		 * We already have created ftok semaphore in db_init or mu_rndwn_file and did not remove it. So just lock it.
		 */
		if (!ftok_sem_lock(reg, may_bypass_ftok))
		{
			if (may_bypass_ftok)
			{	/* We did a non-blocking wait. It's ok to proceed without locking */
				bypassed_ftok = TRUE;
				holder_pid = semctl(udi->ftok_semid, DB_CONTROL_SEM, GETPID);
				if ((uint4)-1 == holder_pid)
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
						      ERR_SYSCALL, 5,
						      RTS_ERROR_TEXT("gds_rundown SEMCTL failed to get holder_pid"),
						      CALLFROM, errno);
				if (!IS_GTM_IMAGE) /* MUMPS processes should not flood syslog with bypass messages. */
				{
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_RESRCINTRLCKBYPAS, 10,
						     LEN_AND_STR(gtmImageNames[image_type].imageName), process_id,
						     LEN_AND_LIT("FTOK"), REG_LEN_STR(reg), DB_LEN_STR(reg), holder_pid);
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2,
						     LEN_AND_LIT("FTOK bypassed at rundown"));
				}
			} else
			{	/* We did a blocking wait but something bad happened. */
				FTOK_TRACE(csa, csa->ti->curr_tn, ftok_ops_lock, process_id);
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
			}
		}
		sop[0].sem_num = DB_CONTROL_SEM; sop[0].sem_op = 0;	/* Wait for 0 */
		sop[1].sem_num = DB_CONTROL_SEM; sop[1].sem_op = 1;	/* Lock */
		sopcnt = 2;
		sop[0].sem_flg = sop[1].sem_flg = SEM_UNDO | IPC_NOWAIT; /* Don't wait the first time thru */
		SEMOP(udi->semid, sop, sopcnt, status, NO_WAIT);
		if (0 != status)
		{
			save_errno = errno;
			/* Check # of processes counted on access sem. */
			if (-1 == (semval = semctl(udi->semid, DB_COUNTER_SEM, GETVAL)))
			{
				assert(FALSE);
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,
					      RTS_ERROR_TEXT("gds_rundown SEMCTL failed to get semval"), CALLFROM, errno);
			}
			bypassed_access = CAN_BYPASS(semval, csa, inst_is_frozen) || onln_rlbk_pid || csd->file_corrupt;
			/* Before attempting again in the blocking mode, see if the holding process is an online rollback.
			 * If so, it is likely we won't get the access control semaphore anytime soon. In that case, we
			 * are better off skipping rundown and continuing with sanity cleanup and exit.
			 */
			holder_pid = semctl(udi->semid, DB_CONTROL_SEM, GETPID);
			if ((uint4)-1 == holder_pid)
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,
					      RTS_ERROR_TEXT("gds_rundown SEMCTL failed to get holder_pid"), CALLFROM, errno);
			if (!bypassed_access)
			{	/* We couldn't get it in one shot-- see if we already have it */
				if (holder_pid == process_id)
				{
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) MAKE_MSG_INFO(ERR_CRITSEMFAIL), 2, DB_LEN_STR(reg),
						     ERR_RNDWNSEMFAIL);
					REVERT;
					ENABLE_INTERRUPTS(INTRPT_IN_GDS_RUNDOWN, prev_intrpt_state);
					assert(FALSE);
					return EXIT_ERR;
				}
				if (EAGAIN != save_errno)
				{
					assert(FALSE);
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
						      ERR_SYSCALL, 5,
						      RTS_ERROR_TEXT("gds_rundown SEMOP on access control semaphore"),
						      CALLFROM, save_errno);
				}
				sop[0].sem_flg = sop[1].sem_flg = SEM_UNDO;	/* Try again - blocking this time */
				SEMOP(udi->semid, sop, 2, status, FORCED_WAIT);
				if (-1 == status)			/* We couldn't get it at all.. */
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
						      ERR_SYSCALL, 5,
						      RTS_ERROR_TEXT("gds_rundown SEMOP on access control semaphore"),
						      CALLFROM, errno);
			} else if (!IS_GTM_IMAGE)
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_RESRCINTRLCKBYPAS, 10,
					     LEN_AND_STR(gtmImageNames[image_type].imageName), process_id,
					     LEN_AND_LIT("access control"), REG_LEN_STR(reg), DB_LEN_STR(reg), holder_pid);
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2,
					     LEN_AND_LIT("Access control bypassed at rundown"));
			}
			udi->grabbed_access_sem = !bypassed_access;
		}
	} /* Else we we hold the access control semaphore and therefore have standalone access. We do not release it now - we
	   * release it later in mupip_exit_handler.c. Since we already hold the access control semaphore, we don't need the
	   * ftok semaphore and trying it could cause deadlock
	   */
	/* Note that in the case of online rollback, "udi->grabbed_access_sem" (and in turn "have_standalone_access") is TRUE.
	 * But there could be other processes still having the database open so we cannot safely reset the halted fields.
	 */
	if (have_standalone_access && !jgbl.onlnrlbk)
		cnl->ftok_counter_halted = cnl->access_counter_halted = FALSE;
	ftok_counter_halted = cnl->ftok_counter_halted;
	access_counter_halted = cnl->access_counter_halted;
	/* If "csd_read_only" is TRUE, that means we created a process private semaphore for this database and no shared memory.
	 * So skip most of the processing in this function that has to do with shared memory and shared semaphore.
	 */
	if (!csd_read_only)
	{
		/* If we bypassed any of the semaphores, activate safe mode.
		 * Also, if the replication instance is frozen and this db has replication turned on (which means
		 * no flushes of dirty buffers to this db can happen while the instance is frozen) activate safe mode.
		 * Similarly, if there is an online freeze in place, we need to avoid writing to the file, so we need
		 * to keep shared memory around.
		 * Or if an online freeze has been autoreleased, we need to keep shared memory around so that it can be
		 * reported and cleaned up by a subsequent MUPIP FREEZE -OFF.
		 */
		ok_to_write_pfin = !(bypassed_access || bypassed_ftok || inst_is_frozen);
		safe_mode = !ok_to_write_pfin || ftok_counter_halted || access_counter_halted || FROZEN_CHILLED(csa)
				|| CHILLED_AUTORELEASE(csa);
		/* At this point we are guaranteed no one else is doing a db_init/rundown as we hold the access control semaphore */
		assert(csa->ref_cnt);	/* decrement private ref_cnt before shared ref_cnt decrement. */
		csa->ref_cnt--;
		assert(!csa->ref_cnt);
		/* Note that the below value is normally incremented/decremented under control of the init/rundown semaphore in
		 * "db_init" and "gds_rundown" but if QDBRUNDOWN is turned ON it could be manipulated without the semaphore in
		 * both callers. Therefore use interlocked INCR_CNT/DECR_CNT.
		 */
		DECR_CNT(&cnl->ref_cnt, &cnl->wc_var_lock);
		if (memcmp(cnl->now_running, ydb_release_name, ydb_release_name_len + 1))
		{	/* VERMISMATCH condition. Possible only if DSE */
			assert(dse_running);
			vermismatch = TRUE;
		} else
			vermismatch = FALSE;
		if (-1 == shmctl(udi->shmid, IPC_STAT, &shm_buf))
		{
			save_errno = errno;
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,
				      RTS_ERROR_TEXT("gds_rundown shmctl"), CALLFROM, save_errno);
		} else
			we_are_last_user =  (1 == shm_buf.shm_nattch) && !vermismatch && !safe_mode;
		/* recover => one user except ONLINE ROLLBACK, or standalone with frozen instance */
		assert(!have_standalone_access || we_are_last_user || jgbl.onlnrlbk || inst_is_frozen);
		if (-1 == (semval = semctl(udi->semid, DB_COUNTER_SEM, GETVAL)))
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg), ERR_SYSCALL, 5,
				      RTS_ERROR_TEXT("gds_rundown SEMCTL failed to get semval"), CALLFROM, errno);
		/* There's one writer left and I am it */
		assert(reg->read_only || semval >= 0);
		unsafe_last_writer = (DB_COUNTER_SEM_INCR == semval) && (FALSE == reg->read_only) && !vermismatch;
		we_are_last_writer = unsafe_last_writer && !safe_mode;
		assert(!we_are_last_writer || !safe_mode);
		assert(!we_are_last_user || !safe_mode);
		/* recover + R/W region => one writer except ONLINE ROLLBACK, or standalone with frozen instance,
		 * leading to safe_mode.
		 */
		assert(!(have_standalone_access && !reg->read_only) || we_are_last_writer || jgbl.onlnrlbk || inst_is_frozen);
		GTM_WHITE_BOX_TEST(WBTEST_ANTIFREEZE_JNLCLOSE, we_are_last_writer, 1);
			/* Assume we are the last writer to invoke wcs_flu */
		if (NULL != csa->ss_ctx)
		{
			ss_destroy_context(csa->ss_ctx);
			free(csa->ss_ctx);
			csa->ss_ctx = NULL;
		}
		/* SS_MULTI: If multiple snapshots are supported, then we have to run through each of the snapshots */
		assert(1 == MAX_SNAPSHOTS);
		ss_shm_ptr = (shm_snapshot_ptr_t)SS_GETSTARTPTR(csa);
		ss_pid = ss_shm_ptr->ss_info.ss_pid;
		is_cur_process_ss_initiator = (process_id == ss_pid);
		if (ss_pid && (is_cur_process_ss_initiator || we_are_last_user))
		{	/* Try getting snapshot crit latch. If we don't get latch, we won't hang for eternity and will skip
			 * doing the orphaned snapshot cleanup. It will be cleaned up eventually either by subsequent MUPIP
			 * INTEG or by a MUPIP RUNDOWN.
			 */
			if (ss_get_lock_nowait(reg) && (ss_pid == ss_shm_ptr->ss_info.ss_pid)
			    && (is_cur_process_ss_initiator || !is_proc_alive(ss_pid, 0)))
			{
				ss_release(NULL);
				ss_release_lock(reg);
			}
		}
		/* If cnl->donotflush_dbjnl is set, it means mupip recover/rollback was interrupted and therefore we need not flush
		 * shared memory contents to disk as they might be in an inconsistent state. Moreover, any more flushing will only
		 * cause future rollback to undo more journal records (PBLKs). In this case, we will go ahead and remove shared
		 * memory (without flushing the contents) in this routine. A reissue of the recover/rollback command will restore
		 * the database to a consistent state.
		 * Or if we have an Online Freeze, skip flushing, as that will be handled when the freeze is removed.
		 */
		if (!cnl->donotflush_dbjnl && !reg->read_only && !vermismatch)
		{	/* If we had an orphaned block and were interrupted, set wc_blocked so we can invoke wcs_recover. Do it ONLY
			 * if there is NO concurrent online rollback running (as we need crit to set wc_blocked)
			 */
			if (csa->wbuf_dqd && !is_mm)
			{	/* If we had an orphaned block and were interrupted, mupip_exit_handler will invoke secshr_db_clnup
				 * which will clear this field and so we should never come to "gds_rundown" with a non-zero
				 * wbuf_dqd. The only exception is if we are recover/rollback in which case "gds_rundown" (from
				 * mur_close_files) is invoked BEFORE secshr_db_clnup in mur_close_files.
				 * Note: It is NOT possible for online rollback to reach here with wbuf_dqd being non-zero. This is
				 * because the moment we apply the first PBLK, we stop all interrupts and hence can never be
				 * interrupted in wcs_wtstart or wcs_get_space. Assert accordingly.
				 */
				assert(mupip_jnl_recover && !jgbl.onlnrlbk && !safe_mode);
				if (!was_crit)
					grab_crit(reg);
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
				BG_TRACE_PRO_ANY(csa, wcb_gds_rundown1);
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_gds_rundown1"),
					     process_id, &csa->ti->curr_tn, DB_LEN_STR(reg));
				csa->wbuf_dqd = 0;
				wcs_recover(reg);
				BG_TRACE_PRO_ANY(csa, lost_block_recovery);
				if (!was_crit)
					rel_crit(reg);
			}
			if (JNL_ENABLED(csd) && IS_GTCM_GNP_SERVER_IMAGE)
				originator_prc_vec = NULL;
			/* If we are the last writing user, then everything must be flushed */
			if (we_are_last_writer)
			{	/* Time to flush out all of our buffers */
				assert(!safe_mode);
				if (is_mm)
				{
					MM_DBFILEXT_REMAP_IF_NEEDED(csa, reg);
					cnl->remove_shm = TRUE;
				}
				if (cnl->wc_blocked && jgbl.onlnrlbk)
				{	/* if the last update done by online rollback was not committed in the normal code-path but
					 * was completed by secshr_db_clnup, wc_blocked will be set to TRUE. But, since online
					 * rollback never invokes grab_crit (since csa->hold_onto_crit is set to TRUE), wcs_recover
					 * is never invoked. This could result in the last update never getting flushed to the disk
					 * and if online rollback happened to be the last writer then the shared memory will be
					 * flushed and removed and the last update will be lost. So, force wcs_recover if we find
					 * ourselves in such a situation. But, wc_blocked is possible only if phase1 or phase2
					 * errors are induced using white box test cases
					 */
					assert(WB_COMMIT_ERR_ENABLED);
					wcs_recover(reg);
				}
				/* Note WCSFLU_SYNC_EPOCH ensures the epoch is synced to the journal and indirectly
				 * also ensures that the db is fsynced. We don't want to use it in the calls to
				 * "wcs_flu" from "t_end" and "tp_tend" since we can defer it to out-of-crit there.
				 * In this case, since we are running down, we don't have any such option.
				 * Note that it is possible an online freeze is concurrently enabled on this db just before
				 * the below call to "wcs_flu" gets crit. In that case, we want to skip the "wcs_flu" but
				 * just do a "jnl_wait" to ensure the jnl file is uptodate. This is because we are holding
				 * the udi->ftok_semid and udi->semid locks at this point and can cause a deadlock with the
				 * MUPIP FREEZE -OFF process (that turns off the online freeze) if we wait indefinitely
				 * inside "wcs_flu" for the online freeze to be lifted off. Hence the WCSFLU_RET_IF_OFRZ usage.
				 */
				cnl->remove_shm = wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH
													| WCSFLU_RET_IF_OFRZ);
				if (!cnl->remove_shm)
				{	/* If "wcs_flu" call fails, then we should not remove shm or reset anything in the db
					 * fileheader. So reset "we_are_last_writer" variable itself as that makes it more safer
					 * to fall through to the cleanup code below. Before doing so, take a copy for debugging
					 * purposes.
					 */
					DEBUG_ONLY(orig_we_are_last_writer = TRUE;)
					we_are_last_writer = FALSE;
					/* Since "wcs_flu" failed, set wc_blocked to TRUE if not already set. */
					if (!cnl->wc_blocked)
					{
						SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
						BG_TRACE_PRO_ANY(csa, wcb_gds_rundown2);
						send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6,
								LEN_AND_LIT("wcb_gds_rundown2"), process_id, &csa->ti->curr_tn,
								DB_LEN_STR(reg));
					}
				}
				/* "wcs_flu" performs writes asynchronously, which might spawn up a thread. We close it here. Since
				 * the thread belongs to the global directory, we assume no one else is doing the same for this
				 * global directory.
				 */
				IF_LIBAIO(aio_shim_destroy(udi->owning_gd);)
				/* Since we_are_last_writer, we should be guaranteed that "wcs_flu" did not change csd, (in
				 * case of MM for potential file extension), even if it did a grab_crit().  Therefore, make
				 * sure that's true.
				 */
				assert(csd == csa->hdr);
				assert(0 == memcmp(csd->label, GDS_LABEL, GDS_LABEL_SZ - 1));
			} else if (((csa->canceled_flush_timer && (0 > cnl->wcs_timers)) || canceled_dbsync_timer)
					&& !inst_is_frozen)
			{	/* If we canceled a pending dbsync timer OR canceled a db flush timer in "gds_rundown"
				 * or sometime in the past (e.g. because we found a JNL_FILE_SWITCHED situation in wcs_stale etc.)
				 * AND there are no other active pending flush timers, it is possible we have unflushed buffers in
				 * the db/jnl so call wcs_flu to flush EPOCH to disk in a timely fashion.
				 * But before that, check if a wcs_flu is really necessary. If not, skip the heavyweight call.
				 */
				db_needs_flushing = (cnl->last_wcsflu_tn < csa->ti->curr_tn);
				/* See comment before prior usage for why WCSFLU_RET_IF_OFRZ is used below */
				if (db_needs_flushing)
					wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH | WCSFLU_RET_IF_OFRZ);
				/* Same as above "wcs_flu" */
				IF_LIBAIO(aio_shim_destroy(udi->owning_gd);)
				assert(is_mm || (csd == cs_data));
				csd = cs_data;	/* In case this is MM and "wcs_flu" remapped an extended database, reset csd */
			}
			/* Do rundown journal processing after buffer flushes since they require jnl to be open */
			if (JNL_ENABLED(csd))
			{
				jpc = csa->jnl;
				jbp = jpc->jnl_buff;
				if (jbp->fsync_in_prog_latch.u.parts.latch_pid == process_id)
				{
					assert(FALSE);
					COMPSWAP_UNLOCK(&jbp->fsync_in_prog_latch, process_id, LOCK_AVAILABLE);
				}
				if (jbp->io_in_prog_latch.u.parts.latch_pid == process_id)
				{
					assert(FALSE);
					COMPSWAP_UNLOCK(&jbp->io_in_prog_latch, process_id, LOCK_AVAILABLE);
				}
				/* If we are last writer, it is possible cnl->remove_shm is set to FALSE from the "wcs_flu" call
				 * above (e.g. we are source server and "wcs_flu" noticed a phase2 commit that need to be cleaned up
				 * which needs a "wcs_recover" call but that is a no-op for the source server). So check that
				 * additionally. Thankfully "we_are_last_writer" would have already factored that into account above
				 * ("we_are_last_writer && cnl->remove_shm" code block above). So no additional check needed below.
				 */
				if (ok_to_write_pfin && !FROZEN_CHILLED(csa)
						&& (((NOJNL != jpc->channel) && !JNL_FILE_SWITCHED(jpc))
								|| (we_are_last_writer && (0 != cnl->jnl_file.u.inode))))
				{	/* We need to close the journal file cleanly if we have the latest generation journal file
					 *	open or if we are the last writer and the journal file is open in shared memory
					 *	(not necessarily by ourselves e.g. the only process that opened the journal got
					 *	shot abnormally).
					 * Note: we should not infer anything from the shared memory value of cnl->jnl_file.u.inode
					 *	if we are not the last writer as it can be concurrently updated.
					 */
					do_jnlwait = FALSE;
					if (!was_crit)
						grab_crit(reg);
					if (JNL_ENABLED(csd))
					{
						SET_GBL_JREC_TIME;	/* jnl_ensure_open/jnl_write_pini/pfin/jnl_file_close
									 * all need it.
									 */
						/* Before writing to jnlfile, adjust jgbl.gbl_jrec_time if needed to maintain time
						 * order of jnl records. This needs to be done BEFORE the jnl_ensure_open as that
						 * could write journal records (if it decides to switch to a new journal file).
						 */
						ADJUST_GBL_JREC_TIME(jgbl, jbp);
						jnl_status = jnl_ensure_open(reg, csa);
						if (0 == jnl_status)
						{	/* If we_are_last_writer, we would have already done a "wcs_flu" which would
							 * have written an epoch record and we are guaranteed no further updates
							 * since we are the last writer. So, just close the journal.
							 * If the freeaddr == post_epoch_freeaddr, wcs_flu may have skipped writing
							 * a pini, so allow for that.
							 */
							assert(!jbp->before_images || is_mm
								|| !we_are_last_writer || (0 != jpc->pini_addr) || jgbl.mur_extract
								|| ((jbp->freeaddr == jbp->post_epoch_freeaddr)
									&& (jbp->freeaddr == jbp->rsrv_freeaddr)));
							/* If we haven't written a pini, let jnl_file_close write the pini/pfin. */
							if (!jgbl.mur_extract && (0 != jpc->pini_addr))
							{
								jnl_write_pfin(csa);
								wrote_pfin = TRUE;
							} else
								wrote_pfin = FALSE;
							/* If not the last writer and no pending flush timer left, do jnl flush now.
							 * But if we wrote a PFIN record, then we can instead do a "jnl_wait" which
							 * is done outside CRIT and is equivalent to "jnl_flush".
							 */
							if (!we_are_last_writer && (0 > cnl->wcs_timers))
							{
								if (wrote_pfin)
									do_jnlwait = TRUE;
								else if (SS_NORMAL == (jnl_status = jnl_flush(reg)))
								{
									assert(jbp->freeaddr == jbp->dskaddr);
									assert(jbp->freeaddr == jbp->rsrv_freeaddr);
									jnl_fsync(reg, jbp->dskaddr);
									assert(jbp->fsync_dskaddr == jbp->dskaddr);
								} else
								{
									send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_JNLFLUSH, 2,
										JNL_LEN_STR(csd), ERR_TEXT, 2,
										RTS_ERROR_TEXT("Error with journal"	\
												"flush in gds_rundown"),
										jnl_status);
									assert(NOJNL == jpc->channel);/* jnl file lost happened */
									/* In this routine, all code that follows from here on
									 * does not assume anything about the journaling
									 * characteristics of this database so it is safe to
									 * continue execution even though journaling got closed
									 * in the middle.
									 */
								}
							}
							if (!do_jnlwait)
								jnl_file_close(reg, we_are_last_writer, FALSE);
						} else
							send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd),
								     DB_LEN_STR(reg));
					}
					if (!was_crit)
						rel_crit(reg);
					if (do_jnlwait)
					{
						jnl_wait(reg);	/* Try to do db fsync outside crit */
						jnl_file_close(reg, we_are_last_writer, FALSE);
					}
				}
			}
			if (we_are_last_writer && !FROZEN_CHILLED(csa))
			{	/* Flush the fileheader last and harden the file to disk */
				if (!was_crit)
					grab_crit(reg);			/* To satisfy crit requirement in fileheader_sync() */
				memset(csd->machine_name, 0, MAX_MCNAMELEN); /* clear the machine_name field */
				if (we_are_last_user && !CHILLED_AUTORELEASE(csa))
				{
					csd->shmid = INVALID_SHMID;
					csd->gt_shm_ctime.ctime = 0;
					if (!have_standalone_access)
					{	/* "mupip_exit_handler" will delete semid later in
						 * "mur_close_file"-->"db_ipcs_reset".
						 */
						csd->semid = INVALID_SEMID;
						csd->gt_sem_ctime.ctime = 0;
					}
				}
				fileheader_sync(reg);
				if (!was_crit)
					rel_crit(reg);
				if (!is_mm)
				{
					GTM_DB_FSYNC(csa, udi->fd, rc);		/* Sync it all */
					if (-1 == rc)
					{
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
							ERR_TEXT, 2, RTS_ERROR_TEXT("Error during file sync at close"), errno);
					}
				} else
				{	/* Now do final MM file sync before exit */
					assert(csa->ti->total_blks == csa->total_blks);
#					ifdef _AIX
					GTM_DB_FSYNC(csa, udi->fd, rc);
					if (-1 == rc)
#					else
					if (-1 == MSYNC((caddr_t)csa->db_addrs[0], (caddr_t)csa->db_addrs[1]))
#					endif
					{
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
							ERR_TEXT, 2, RTS_ERROR_TEXT("Error during file sync at close"),
							errno);
					}
				}
			} else if (unsafe_last_writer && !cnl->lastwriterbypas_msg_issued)
			{
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_LASTWRITERBYPAS, 2, DB_LEN_STR(reg));
				cnl->lastwriterbypas_msg_issued = TRUE;
			}
		} /* end if (!reg->read_only && !cnl->donotflush_dbjnl) */
		/* We had canceled all db timers at start of rundown. In case as part of rundown (wcs_flu above), we had started
		 * any timers, cancel them BEFORE setting reg->open to FALSE (assert in wcs_clean_dbsync relies on this).
		 */
		CANCEL_DB_TIMERS(reg, csa, canceled_dbsync_timer);
		if (reg->read_only && we_are_last_user && cnl->remove_shm)
		{	/* mupip_exit_handler will do this after mur_close_file */
			db_ipcs.open_fd_with_o_direct = udi->fd_opened_with_o_direct;
			db_ipcs.shmid = INVALID_SHMID;
			db_ipcs.gt_shm_ctime = 0;
			if (!have_standalone_access)
			{	/* "mupip_exit_handler" will delete semid later in "mur_close_file"-->"db_ipcs_reset" */
				db_ipcs.semid = INVALID_SEMID;
				db_ipcs.gt_sem_ctime = 0;
			}
			db_ipcs.fn_len = seg->fname_len;
			memcpy(db_ipcs.fn, seg->fname, seg->fname_len);
			db_ipcs.fn[seg->fname_len] = 0;
			/* request gtmsecshr to flush. read_only cannot flush itself */
			WAIT_FOR_REPL_INST_UNFREEZE_SAFE(csa);
			if (!csa->read_only_fs)
			{
				secshrstat = send_mesg2gtmsecshr(FLUSH_DB_IPCS_INFO, 0, (char *)NULL, 0);
				if (0 != secshrstat)
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						ERR_TEXT, 2, RTS_ERROR_TEXT("gtmsecshr failed to update database file header"));
			}
		}
		if (!is_mm && csd->asyncio)
		{	/* Cancel ALL pending async ios for this region by this process. Need to do this BEFORE detaching from db
			 * shared memory OR closing the file descriptor (udi->fd) as the in-progress asyncio buffers/fd point there.
			 */
#			ifndef USE_LIBAIO
			WAIT_FOR_AIO_TO_BE_DONE(udi->fd, aiocancel_timedout);
			if (aiocancel_timedout)
				send_msg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_AIOCANCELTIMEOUT, 3, process_id, DB_LEN_STR(reg));
#			else
			/* Here, aio_shim_destroy() will destroy the thread and the kernel context associated with the entire global
			 * directory -- so this effectively cancels all in-progress IOs to all subsequent regions that are about to
			 * be rundown. This is necessary, however, because we need to cancel all in-progress IOs before the below
			 * CLOSEFILE_RESET().
			 *
			 * IOs canceled for subsequent regions will be reissued when we go to "gds_rundown" next and the
			 * "wcs_flu"/wcs_wtstart()/aio_shim_write() happens, which will reopen the kernel context and multiplexing
			 * thread as necessary.
			 */
			aio_shim_destroy(udi->owning_gd);
#			endif
		}
	} else
	{
		we_are_last_user = TRUE;
		safe_mode = FALSE;
	}
	/* If "reg" is a statsdb, it is possible that the basedb has asyncio turned on and has called for the statsdb to be
	 * rundown first. A statsdb never has asyncio turned on. So it is possible that thread_gdi is non-NULL and will be
	 * cleaned up as part of the "aio_shim_destroy" done in "gds_rundown" of the basedb but that will happen only after
	 * the statsdb rundown completes. So account for that in the below assert.
	 */
	assert((NULL == udi->owning_gd->thread_gdi) || is_statsDB);
	udi->owning_gd = NULL;
	/* Done with file now, close it */
	CLOSEFILE_RESET(udi->fd, rc);	/* resets "udi->fd" to FD_INVALID and does flock(LOCK_UN) if needed */
	if (-1 == rc)
	{
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg),
			      ERR_TEXT, 2, LEN_AND_LIT("Error during file close"), errno);
	}
	if (NULL != csa->db_addrs[0])
	{
		/* Unmap storage if mm mode */
		if (is_mm)
		{
			assert(csa->db_addrs[1] > csa->db_addrs[0]);
#			if !defined(_AIX)
			munmap_len = (sm_long_t)(1 + csa->db_addrs[1] - csa->db_addrs[0]);	/* Note: 1 + to compensate for
												 * -1 done in "db_init".
												 */
			assert(0 < munmap_len);
			status = munmap((caddr_t)(csa->db_addrs[0]), (size_t)(munmap_len));
			assert(0 == status);
#			endif
		}
		/* If this is a BASEDB and we are the last one out, prepare to unlink/remove the corresponding STATSDB */
		if (we_are_last_user)
			UNLINK_STATSDB_AT_BASEDB_RUNDOWN(cnl, csd);
	}
	/* Detach our shared memory while still under lock so reference counts will be correct for the next process to run down
	 * this region. In the process also get the remove_shm status from node_local before detaching.
	 * If cnl->donotflush_dbjnl is TRUE, it means we can safely remove shared memory without compromising data
	 * integrity as a reissue of recover will restore the database to a consistent state.
	 */
	remove_shm = !csd_read_only && !vermismatch && (cnl->remove_shm || cnl->donotflush_dbjnl) && !CHILLED_AUTORELEASE(csa);
	/* We are done with online rollback on this region. Indicate to other processes by setting the onln_rlbk_pid to 0.
	 * Do it before releasing crit (t_end relies on this ordering when accessing cnl->onln_rlbk_pid).
	 */
	if (jgbl.onlnrlbk)
		cnl->onln_rlbk_pid = 0;
	rel_crit(reg); /* Since we are about to detach from the shared memory, release crit and reset onln_rlbk_pid */
	/* If we had skipped flushing journal and database buffers due to a concurrent online rollback, increment the counter
	 * indicating that in the shared memory so that online rollback can report the # of such processes when it shuts down.
	 * The same thing is done for both FTOK and access control semaphores when there are too many MUMPS processes.
	 */
	if (safe_mode) /* indicates flushing was skipped */
	{
		if (bypassed_access)
			cnl->dbrndwn_access_skip++; /* Access semaphore can be bypassed during online rollback */
		if (bypassed_ftok)
			cnl->dbrndwn_ftok_skip++;
	}
	if (jgbl.onlnrlbk)
		csa->hold_onto_crit = FALSE;
	GTM_WHITE_BOX_TEST(WBTEST_HOLD_SEM_BYPASS, cnl->wbox_test_seq_num, 0);
	status = (!csd_read_only) ? SHMDT((caddr_t)cnl) : 0;
	csa->nl = NULL; /* dereferencing nl after detach is not right, so we set it to NULL so that we can test before dereference*/
	csa->hdr = NULL;	/* dereferencing hdr after detach also is not right so set it to NULL */
	/* Note that although csa->nl is NULL, we use CSA_ARG(csa) below (not CSA_ARG(NULL)) to be consistent with similar
	 * usages before csa->nl became NULL. The "is_anticipatory_freeze_needed" function (which is in turn called by the
	 * CHECK_IF_FREEZE_ON_ERROR_NEEDED macro) does a check of csa->nl before dereferencing shared memory contents so
	 * we are safe passing "csa".
	 */
	if (-1 == status)
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) ERR_DBFILERR, 2, DB_LEN_STR(reg), ERR_TEXT, 2,
			     LEN_AND_LIT("Error during shmdt"), errno);
	REMOVE_CSA_FROM_CSADDRSLIST(csa);	/* remove "csa" from list of open regions (cs_addrs_list) */
	reg->open = FALSE;
	assert(!is_statsDB || process_exiting || IS_GTCM_GNP_SERVER_IMAGE);
	/* If file is still not in good shape, die here and now before we get rid of our storage */
	assertpro(0 == csa->wbuf_dqd);
	ipc_deleted = FALSE;
	/* If we are the very last user, remove shared storage id and the semaphores */
	if (we_are_last_user)
	{	/* remove shared storage, only if last writer to rundown did a successful "wcs_flu" */
		assert(csd_read_only || !vermismatch);
		if (remove_shm)
		{
			ipc_deleted = TRUE;
			if (0 != shm_rmid(udi->shmid))
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					      ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to remove shared memory"));
			/* Note that this process deleted shared memory. Currently only used by rollback. */
			udi->shm_deleted = TRUE;
			/* mupip recover/rollback don't release the semaphore here, but do it later in "db_ipcs_reset"
			 * (invoked from "mur_close_files")
			 */
			if (!have_standalone_access)
			{
				if (0 != sem_rmid(udi->semid))
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						      ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to remove semaphore"), errno);
				udi->sem_deleted = TRUE;		/* Note that we deleted the semaphore */
				udi->grabbed_access_sem = FALSE;
				udi->counter_acc_incremented = FALSE;
			}
			if (is_statsDB)
			{
				STATSDBREG_TO_BASEDBREG(reg, baseDBreg);
				baseDBcsa = &FILE_INFO(baseDBreg)->s_addrs;
				baseDBnl = baseDBcsa->nl;
				baseDBnl->statsdb_rundown_clean = TRUE;
			}
		} else if (csd_read_only)
		{
			if (0 != sem_rmid(udi->semid))
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					      ERR_TEXT, 2, RTS_ERROR_TEXT("Unable to remove semaphore"), errno);
			ipc_deleted = TRUE;
			udi->sem_deleted = TRUE;		/* Note that we deleted the semaphore */
			udi->grabbed_access_sem = FALSE;
			udi->counter_acc_incremented = FALSE;
		} else if (is_src_server || is_updproc)
		{
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_DBRNDWNWRN, 4, DB_LEN_STR(reg), process_id, process_id);
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_DBRNDWNWRN, 4, DB_LEN_STR(reg), process_id, process_id);
		} else
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_DBRNDWNWRN, 4, DB_LEN_STR(reg), process_id, process_id);
	} else
	{
		assert(!csd_read_only);
		assert(!have_standalone_access || jgbl.onlnrlbk || safe_mode);
		if (!jgbl.onlnrlbk && !have_standalone_access)
		{ 	/* If we were writing, get rid of our writer access count semaphore */
			if (!reg->read_only)
			{
				if (!access_counter_halted)
				{
					save_errno = do_semop(udi->semid, DB_COUNTER_SEM, -DB_COUNTER_SEM_INCR, SEM_UNDO);
					if (0 != save_errno)
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
							      ERR_SYSCALL, 5,
							      RTS_ERROR_TEXT("gds_rundown access control semaphore decrement"),
							      CALLFROM, save_errno);
				}
				udi->counter_acc_incremented = FALSE;
			}
			assert(safe_mode || !bypassed_access);
			/* Now remove the rundown lock */
			if (!bypassed_access)
			{
				if (0 != (save_errno = do_semop(udi->semid, DB_CONTROL_SEM, -1, SEM_UNDO)))
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(12) ERR_CRITSEMFAIL, 2, DB_LEN_STR(reg),
						      ERR_SYSCALL, 5,
						      RTS_ERROR_TEXT("gds_rundown access control semaphore release"),
						      CALLFROM, save_errno);
				udi->grabbed_access_sem = FALSE;
			}
		} /* else access control semaphore will be released in db_ipcs_reset */
	}
	if (!have_standalone_access)
	{
		if (csd_read_only)
		{	/* If csd_read_only is TRUE and we did not increment the ftok sem counter, it means the ftok sem
			 * could be concurrently removed by another process so skip doing any semop/semctl on that semid
			 * (just like we skipped the "ftok_sem_lock" above.
			 */
			assert(udi->counter_ftok_incremented == !ftok_counter_halted);
			if (udi->counter_ftok_incremented)
			{	/* Decrement ftok semaphore counter and remove it if counter becomes zero.
				 * But before that, get the ftok lock.
				 */
				if (!ftok_sem_lock(reg, IMMEDIATE_FALSE))
				{
					FTOK_TRACE(csa, csa->ti->curr_tn, ftok_ops_lock, process_id);
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(11) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						ERR_SYSCALL, 5, RTS_ERROR_TEXT("db_read_only ftok_sem_lock"), CALLFROM);
				}
				if (!ftok_sem_release(reg, DECR_CNT_TRUE, FALSE))
				{
					FTOK_TRACE(csa, csa->ti->curr_tn, ftok_ops_release, process_id);
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(11) ERR_DBFILERR, 2, DB_LEN_STR(reg),
						ERR_SYSCALL, 5, RTS_ERROR_TEXT("db_read_only ftok_sem_release"), CALLFROM);
				}
			}
		} else if (bypassed_ftok)
		{
			if (!ftok_counter_halted)
				if (0 != (save_errno = do_semop(udi->ftok_semid, DB_COUNTER_SEM, -DB_COUNTER_SEM_INCR, SEM_UNDO)))
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
		} else if (!ftok_sem_release(reg, !ftok_counter_halted, FALSE))
		{
			FTOK_TRACE(csa, csa->ti->curr_tn, ftok_ops_release, process_id);
			rts_error_csa(CSA_ARG(csa) VARLSTCNT(4) ERR_DBFILERR, 2, DB_LEN_STR(reg));
		}
		udi->grabbed_ftok_sem = FALSE;
		udi->counter_ftok_incremented = FALSE;
	}
	ENABLE_INTERRUPTS(INTRPT_IN_GDS_RUNDOWN, prev_intrpt_state);
	if (!ipc_deleted)
	{
		GET_CUR_TIME(time_str);
		if (is_src_server)
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_IPCNOTDEL, 6, CTIME_BEFORE_NL, time_str,
				       LEN_AND_LIT("Source server"), REG_LEN_STR(reg));
		if (is_updproc)
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_IPCNOTDEL, 6, CTIME_BEFORE_NL, time_str,
				       LEN_AND_LIT("Update process"), REG_LEN_STR(reg));
		if (mupip_jnl_recover && (!jgbl.onlnrlbk || !we_are_last_user))
		{
			gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_IPCNOTDEL, 6, CTIME_BEFORE_NL, time_str,
				       LEN_AND_LIT("Mupip journal process"), REG_LEN_STR(reg));
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_IPCNOTDEL, 6, CTIME_BEFORE_NL, time_str,
				     LEN_AND_LIT("Mupip journal process"), REG_LEN_STR(reg));
		}
	}
	REVERT;
	/* Now that "gds_rundown" is done, free up the memory associated with the region as long as the caller is okay with it */
	if (cleanup_udi)
	{
		if (NULL != csa->dir_tree)
			FREE_CSA_DIR_TREE(csa);
		if (csa->sgm_info_ptr)
		{
			si = csa->sgm_info_ptr;
			/* It is possible we got interrupted before initializing all fields of "si"
			 * completely so account for NULL values while freeing/releasing those fields.
			 */
			assert((si->tp_csa == csa) || (NULL == si->tp_csa));
			if (si->jnl_tail)
			{
				PROBE_FREEUP_BUDDY_LIST(si->format_buff_list);
				PROBE_FREEUP_BUDDY_LIST(si->jnl_list);
				FREE_JBUF_RSRV_STRUCT(si->jbuf_rsrv_ptr);
			}
			PROBE_FREEUP_BUDDY_LIST(si->recompute_list);
			PROBE_FREEUP_BUDDY_LIST(si->new_buff_list);
			PROBE_FREEUP_BUDDY_LIST(si->tlvl_info_list);
			PROBE_FREEUP_BUDDY_LIST(si->tlvl_cw_set_list);
			PROBE_FREEUP_BUDDY_LIST(si->cw_set_list);
			if (NULL != si->blks_in_use)
			{
				free_hashtab_int4(si->blks_in_use);
				free(si->blks_in_use);
				si->blks_in_use = NULL;
			}
			if (si->cr_array_size)
			{
				assert(NULL != si->cr_array);
				if (NULL != si->cr_array)
					free(si->cr_array);
			}
			if (NULL != si->first_tp_hist)
				free(si->first_tp_hist);
			free(si);
		}
		if (csa->jnl)
		{
			assert(&FILE_INFO(csa->jnl->region)->s_addrs == csa);
			free(csa->jnl);
		}
		assert(seg->file_cntl->file_info);
		free(seg->file_cntl->file_info);
		free(seg->file_cntl);
		seg->file_cntl = NULL;
	}
	return EXIT_NRM;
}
