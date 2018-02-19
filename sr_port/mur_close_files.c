/****************************************************************
 *								*
 * Copyright (c) 2003-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtmio.h"
#include "gtm_string.h"
#include "gtm_time.h"
#include "gtm_unistd.h"
#include "gtm_signal.h"

#include <sys/shm.h>

#include "gdsroot.h"
#include "gtm_rename.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "iosp.h"
#include "tp_change_reg.h"
#include "gds_rundown.h"
#include "gtmmsg.h"
#include "file_head_read.h"
#include "file_head_write.h"
#include "have_crit.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "mu_rndwn_replpool.h"
#include "db_ipcs_reset.h"
#include "repl_instance.h"
#include "repl_sem.h"
#include "ftok_sems.h"
#include "gtmsource_srv_latch.h"
#include "anticipatory_freeze.h"
#include "ipcrmid.h"
#include "util.h"
#include "wcs_flu.h"
#include "gtm_sem.h"
#include "gtm_file_stat.h"
#include "gtm_ipc.h"
#ifdef DEBUG
#include "wbox_test_init.h"
#endif
#include "interlock.h"
#include "do_semop.h"

#define WARN_STATUS(jctl)											\
if (SS_NORMAL != jctl->status)											\
{														\
	assert(FALSE);												\
	if (SS_NORMAL != jctl->status2)										\
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT1(6) ERR_JNLWRERR, 2, jctl->jnl_fn_len, jctl->jnl_fn,	\
			jctl->status, PUT_SYS_ERRNO(jctl->status2));						\
	else													\
		gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(5) ERR_JNLWRERR,						\
			2, jctl->jnl_fn_len, jctl->jnl_fn, jctl->status);					\
	wrn_count++;												\
}														\

GBLREF	boolean_t		holds_sem[NUM_SEM_SETS][NUM_SRC_SEMS];
GBLREF	boolean_t		mupip_exit_status_displayed;
GBLREF	boolean_t		mur_close_files_done;
GBLREF	char			*jnl_state_lit[];
GBLREF	char			*repl_state_lit[];
GBLREF	gd_region		*gv_cur_region;
GBLREF	int4			forced_exit_err;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	recvpool_addrs		recvpool;
GBLREF	mur_gbls_t		murgbl;
GBLREF	mur_opt_struct		mur_options;
GBLREF	reg_ctl_list		*mur_ctl;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data		*cs_data;
GBLREF	uint4			process_id;
GBLREF	void			(*call_on_signal)();
#ifdef DEBUG
GBLREF	boolean_t		exiting_on_dev_out_error;
#endif

error_def(ERR_FILERENAME);
error_def(ERR_JNLACTINCMPLT);
error_def(ERR_JNLBADLABEL);
error_def(ERR_JNLREAD);
error_def(ERR_JNLSTATE);
error_def(ERR_JNLSUCCESS);
error_def(ERR_JNLWRERR);
error_def(ERR_MUJNLSTAT);
error_def(ERR_MUJPOOLRNDWNSUC);
error_def(ERR_MUNOACTION);
error_def(ERR_ORLBKCMPLT);
error_def(ERR_ORLBKTERMNTD);
error_def(ERR_PREMATEOF);
error_def(ERR_RENAMEFAIL);
error_def(ERR_REPLPOOLINST);
error_def(ERR_REPLSTATE);
error_def(ERR_NOTALLDBRNDWN);
error_def(ERR_REPLFTOKSEM);
error_def(ERR_RLBKSTRMSEQ);

boolean_t mur_close_files(void)
{
	char 			*head_jnl_fn, *rename_fn, fn[MAX_FN_LEN + STR_LIT_LEN(PREFIX_ROLLED_BAK) + 1];
	int			head_jnl_fn_len, wrn_count = 0, rename_fn_len;
	reg_ctl_list		*rctl, *rctl_top;
	jnl_ctl_list		jctl_temp, *jctl, *prev_jctl, *end_jctl;
	gd_region		*reg;
	sgmnt_addrs		*csa;
	sgmnt_data		*csd, csd_temp;
	uint4			ustatus, ustatus2;
	int4			status;
	int4			rundown_status = EXIT_NRM;		/* if "gds_rundown" went smoothly */
	int			idx, finish_err_code, save_errno;
	const char		*fini_str = NULL;
	const char		*termntd_str;
	unsigned char		ipcs_buff[MAX_IPCS_ID_BUF], *ipcs_ptr;
	struct shmid_ds		shm_buf;
	union semun		semarg;
	unix_db_info		*udi = NULL;
	gtmsrc_lcl		gtmsrc_lcl_array[NUM_GTMSRC_LCL];
	repl_inst_hdr		repl_instance;
	repl_inst_hdr_ptr_t	inst_hdr = NULL;
	seq_num			max_strm_seqno[MAX_SUPPL_STRMS], this_strm_seqno;
	boolean_t		incr_jnlpool_rlbk_cycle = TRUE, got_ftok, anticipatory_freeze_available, was_crit;
	boolean_t		inst_frozen = FALSE, strm_seqno_nonzero;
	gtmsource_local_ptr_t	gtmsourcelocal_ptr;
	global_latch_t		*latch;
	seq_num			max_zqgblmod_seqno = 0, last_histinfo_seqno;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jb;
	uint4			jnl_status;
#	ifdef DEBUG
	int		semval;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (mur_close_files_done)
	{
		assert(mupip_exit_status_displayed);
		return TRUE;
	}
	call_on_signal = NULL;	/* Do not recurs via call_on_signal if there is an error */
	SET_PROCESS_EXITING_TRUE;	/* In case the database is encrypted, this value is used to avoid using
					 * mur_ctl->csd in mur_fopen as it would be invalid due to the "gds_rundown" done below.
					 */
	csd = &csd_temp;
	assert(murgbl.losttn_seqno == murgbl.save_losttn_seqno);
	/* If journaling, "gds_rundown" will need to write PINI/PFIN records. The timestamp of that journal record will
	 * need to be adjusted to the current system time to reflect that it is recovery itself writing that record
	 * instead of simulating GT.M activity. Reset jgbl.dont_reset_gbl_jrec_time to allow for adjustments to gbl_jrec_time.
	 */
	jgbl.dont_reset_gbl_jrec_time = FALSE;
	if (mur_options.rollback)
	{
		memset(&max_strm_seqno[0], 0, SIZEOF(max_strm_seqno));
		strm_seqno_nonzero = FALSE;
	}
	anticipatory_freeze_available = INST_FREEZE_ON_ERROR_POLICY;
	inst_hdr = jnlpool ? jnlpool->repl_inst_filehdr : NULL;
	if (jgbl.onlnrlbk)
	{
		/* Note that murgbl.consist_jnl_seqno is maintained even if the only thing done by rollback was
		 * lost transaction processing. In this case, we shouldn't consider the instance as being rolled back.
		 * So, set murgbl.incr_db_rlbkd_cycle = FALSE;
		 */
		if (mur_options.rollback_losttnonly)
		{
			assert(!murgbl.incr_onln_rlbk_cycle);	/* should not have been set because we did not touch
								 * the database at all.
								 */
			murgbl.incr_db_rlbkd_cycle = FALSE;
		}
		/* Note that even if murgbl.incr_db_rlbkd_cycle is TRUE (i.e. the pre-rollback seqno is different
		 * from the post-rollback seqno, it is possible murgbl.incr_onln_rlbk_cycle is FALSE. For example,
		 * if we are taking the db back in time seqno wise and all those seqnos happen to be JRT_NULL type of
		 * records, it is possible undoing those seqnos would not result in any PBLKs being played in the database.
		 * But as long as at least one database has been taken back in time, we need to set murgbl.incr_onln_rlbk_cycle
		 * to TRUE as that is the only way csa->nl->onln_rlbk_cycle++ will happen below and in turn this change in seqno
		 * will be communicated to an active source server that is replicating (so it can react to the online-rollback).
		 */
		 if (murgbl.incr_db_rlbkd_cycle)
			murgbl.incr_onln_rlbk_cycle = TRUE;
	}
#	if 0
	/* disable assertion until we make jnlpool_init conditional on anticipatory freeze available */
	assert(jgbl.onlnrlbk || (NULL == jnlpool_ctl));
#	endif
	assert(((NULL == jnlpool) || (NULL == jnlpool->jnlpool_ctl)) || (TRUE == inst_hdr->crash));
	assert(((NULL == jnlpool) || (NULL == jnlpool->jnlpool_ctl)) || jgbl.onlnrlbk || anticipatory_freeze_available);
	if ((NULL != jnlpool) && (NULL != jnlpool->jnlpool_ctl))
	{
		csa = &FILE_INFO(jnlpool->jnlpool_dummy_reg)->s_addrs;
		ASSERT_VALID_JNLPOOL(csa);
	}
	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_full_total; rctl < rctl_top; rctl++)
	{
		/* If online rollback, external signals are blocked the first time we touch the database with a PBLK. So, if ever
		 * we come here with online rollback and the database was updated, then we better have a clean exit state.
		 */
		assert(!rctl->db_updated || murgbl.clean_exit || !jgbl.onlnrlbk);
		reg = rctl->gd;
		/* reg could be NULL at this point in some rare cases (e.g. if we come to mur_close_files through
		 * deferred_signal_handler as part of call_on_signal invocation and run down this region but encounter
		 * an error while running down another region, we could re-enter mur_close_files as part of exit handling.
		 * In this case, since this region has already been rundown, skip running this down.
		 */
		if (NULL == reg)
			continue;
		/* Note that even if reg->open is FALSE, rctl->csa could be non-NULL in case mur_open_files went through
		 * "mupfndfil" path (e.g. journal file names were explicitly specified for a journal extract command) so it
		 * would not have done gvcst_init in that case. But we would have set rctl->csa to a non-NULL value in order
		 * to be able to switch amongst regions in mur_forward. So we should check for gd->open below to know for
		 * sure if a gvcst_init was done which in turn requires a "gds_rundown" to be done.
		 */
		if (reg->open)
		{ 	/* gvcst_init was called */
			gv_cur_region = reg;
			tp_change_reg();
			/* Even though reg->open is TRUE, rctl->csa could be NULL in cases where we are in
			 * mur_open_files, doing a mu_rndwn_file (as part of the STANDALONE macro) and get a
			 * signal (say MUPIP STOP) that causes exit handling processing which invokes this function.
			 * In this case, cs_addrs would still be set to a non-NULL value so use that instead of rctl->csa.
			 */
			assert(((NULL != rctl->csa) && (rctl->csa == cs_addrs)) || ((NULL == rctl->csa) && !murgbl.clean_exit));
			csa = cs_addrs;
			csd = mur_options.forward ? &csd_temp : csa->hdr;
			assert(NULL != csa);
			if (mur_options.update && JNL_ENABLED(rctl))
				csa->jnl->pini_addr = 0; /* Stop simulation of GTM process journal record writing */
			if (NULL != rctl->jctl && murgbl.clean_exit && mur_options.rollback && !mur_options.rollback_losttnonly)
			{	/* to write proper jnl_seqno in epoch record */
				assert(murgbl.losttn_seqno >= murgbl.consist_jnl_seqno);
				assert(murgbl.consist_jnl_seqno);
				jgbl.mur_jrec_seqno = csa->hdr->reg_seqno = murgbl.consist_jnl_seqno;
			}
			assert(NULL != csa->nl);
			assert((!(mur_options.update ^ csa->nl->donotflush_dbjnl)) || !murgbl.clean_exit);
			if (mur_options.update && (murgbl.clean_exit || !rctl->db_updated) && (NULL != csa->nl))
				csa->nl->donotflush_dbjnl = FALSE;	/* shared memory is now clean for dbjnl flushing */
			/* Note: udi/csa is used a little later after the "gds_rundown" call (e.g. by "jnl_set_cur_prior")
			 * so pass CLEANUP_UDI_FALSE as the parameter.
			 */
			if (mur_options.forward)
				rundown_status = gds_rundown(CLEANUP_UDI_FALSE);
			if (EXIT_NRM != rundown_status)
			{
				wrn_count++;
				continue;
			}
			assert(!jgbl.onlnrlbk || (csa->now_crit && csa->hold_onto_crit)
					|| (!murgbl.clean_exit && !rctl->db_updated));
			if (jgbl.onlnrlbk && (repl_open == rctl->repl_state))
			{
				assert(!IS_STATSDB_CSA(csa));
				if (murgbl.incr_onln_rlbk_cycle)
				{
					csa->nl->root_search_cycle++;
					csa->nl->onln_rlbk_cycle++;
				}
				if (murgbl.incr_db_rlbkd_cycle)
				{
					assert(murgbl.incr_db_rlbkd_cycle);
					csa->nl->db_onln_rlbkd_cycle++;
				}
				csa->root_search_cycle = csa->nl->root_search_cycle;
				csa->onln_rlbk_cycle = csa->nl->onln_rlbk_cycle;
				csa->db_onln_rlbkd_cycle = csa->nl->db_onln_rlbkd_cycle;
				if (incr_jnlpool_rlbk_cycle && (jnlpool && jnlpool->jnlpool_ctl) && murgbl.incr_onln_rlbk_cycle)
				{
					jnlpool->jnlpool_ctl->onln_rlbk_cycle++;
					incr_jnlpool_rlbk_cycle = FALSE;
				}
			}
			if (rctl->standalone && (murgbl.clean_exit || !rctl->db_updated) && !reg->read_only)
			{
				if (mur_options.forward)
				{
					status = file_head_read((char *)reg->dyn.addr->fname, csd, SIZEOF(csd_temp));
					/* A TRUE "status" implies successful return from "file_head_read".
					 * A FALSE "status" implies "file_head_read" would have done the needed "gtm_putmsg" */
					if (!status)
						wrn_count++;	/* signal mupip journal action is incomplete */
				} else
					status = TRUE;
				if (status)
				{
					assert(mur_options.update);
					/* For MUPIP JOURNAL -RECOVER -FORWARD or MUPIP JOURNAL -ROLLBACK -FORWARD, we are
					 * done "gds_rundown" at this point and so have a clean database state at this point.
					 * For RECOVER/ROLLBACK -BACKWARD, even though we haven't done the "gds_rundown" yet,
					 * we still hold the standalone access and so no new process can attach to the database.
					 * For the -ONLINE version of RECOVER/ROLLBACK -BACKWARD, we haven't released the access
					 * control lock as well as the critical section lock, so no new processes can attach to
					 * the database and no existing process can continue from their hung state(waiting for
					 * crit). So, in all cases, it should be okay to safely set csd->file_corrupt to FALSE.
					 * The only issue is if we get crashed AFTER setting csd->file_corrupt to FALSE, but
					 * before doing the "gds_rundown" in which case, all the processes starting up will see
					 * it just like any other system crash warranting a ROLLBACK/RECOVER.
					 */
					csd->file_corrupt = FALSE;
					if (murgbl.clean_exit)
					{
						if (mur_options.rollback)
							csa->repl_state = csd->repl_state = rctl->repl_state;
						/* After recover replication state is always closed */
						if (rctl->repl_state != csd->repl_state)
							gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_REPLSTATE, 6,
								LEN_AND_LIT(FILE_STR), DB_LEN_STR(reg),
								LEN_AND_STR(repl_state_lit[csd->repl_state]));
						if (rctl->jnl_state != csd->jnl_state)
							gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_JNLSTATE, 6,
								LEN_AND_LIT(FILE_STR), DB_LEN_STR(reg),
								LEN_AND_STR(jnl_state_lit[csd->jnl_state]));
						if ((NULL != rctl->jctl) && !mur_options.rollback_losttnonly)
						{
							if (mur_options.rollback)
							{
								assert(murgbl.consist_jnl_seqno);
								csd->reg_seqno = murgbl.consist_jnl_seqno;
								/* Ensure zqgblmod_seqno never goes above the current reg_seqno.
								 * Also ensure it gets set to non-zero value if instance was former
								 * root primary and this is a fetchresync rollback (needed not
								 * for $zqgblmod processing but instead to store the fact that a
								 * losttn_complete is pending in this instance and until that is
								 * done, this instance cannot become a tertiary).
								 */
								if ((csd->zqgblmod_seqno > murgbl.consist_jnl_seqno)
									|| (!csd->zqgblmod_seqno
										&& mur_options.fetchresync_port
										&& murgbl.was_rootprimary))
								{
									csd->zqgblmod_seqno = murgbl.consist_jnl_seqno;
									csd->zqgblmod_tn = csd->trans_hist.curr_tn;
								}
								if (max_zqgblmod_seqno < csd->zqgblmod_seqno)
									max_zqgblmod_seqno = csd->zqgblmod_seqno;
								/* At this point, csd->strm_reg_seqno[] should already be set
								 * correctly. Compute max_strm_seqno across all regions (needed
								 * later)
								 */
								for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
								{
									if (csd->strm_reg_seqno[idx] > max_strm_seqno[idx])
									{
										max_strm_seqno[idx] = csd->strm_reg_seqno[idx];
										strm_seqno_nonzero = TRUE;
									}
								}
							}
						}
						/* If we just did forward recovery, and the current journal is available,
						 * then we are leaving journaling disabled, so mark the journal as switched.
						 */
						if (mur_options.forward && (jnl_open == csd->intrpt_recov_jnl_state))
							jnl_set_cur_prior(reg, csa, csd);
						/* Reset save_strm_reg_seqno[]. Do it even for -recover (not just for -rollback)
						 * so a successful -recover after an interrupted -rollback clears these fields.
						 * Take this opportunity to reset intrpt_recov_resync_strm_seqno[] as well.
						 */
						for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
						{
							csd->save_strm_reg_seqno[idx] = 0;
							csd->intrpt_recov_resync_strm_seqno[idx] = 0;
						}
						csd->intrpt_recov_resync_seqno = 0;
						csd->intrpt_recov_tp_resolve_time = 0;
						csd->intrpt_recov_jnl_state = jnl_notallowed;
						csd->intrpt_recov_repl_state = repl_closed;
						csd->recov_interrupted = FALSE;
						/* If any of the last_{com,inc,rec}_backup TN fields have values greater than
						 * the new transaction number of the database, then set them to 1. This will cause
						 * the next BACKUP (incremental/comprehensive) to treat the request as a full
						 * comprehensive BACKUP.
						 */
						if (csd->last_com_backup > csd->trans_hist.curr_tn)
						{
							csd->last_com_backup = 1;
							csd->last_com_bkup_last_blk = 1;
						}
						if (csd->last_inc_backup > csd->trans_hist.curr_tn)
						{
							csd->last_inc_backup = 1;
							csd->last_inc_bkup_last_blk = 1;
						}
						if (csd->last_rec_backup > csd->trans_hist.curr_tn)
						{
							csd->last_rec_backup = 1;
							csd->last_rec_bkup_last_blk = 1;
						}
					} else
					{	/* Restore states. Otherwise, reissuing the command might fail.
						 * However, before using rctl make sure it was properly initialized.
						 * If not, skip the restore. This is okay because in this case an interrupt
						 * occurred in mur_open_files before rctl->initialized was set which means
						 * journaling and/or replication state of csd (updated AFTER rctl->initialized
						 * is set to TRUE) would not yet have been touched either. */
						if (rctl->initialized)
						{
							csd->repl_state = rctl->repl_state;
							csd->jnl_state = rctl->jnl_state;
							csd->jnl_before_image = rctl->before_image;
							csd->recov_interrupted = rctl->recov_interrupted;
						}
					}
					if (!file_head_write((char *)reg->dyn.addr->fname, csd, SIZEOF(csd_temp)))
						wrn_count++;
				}
			} /* else do not restore state */
			if (rctl->standalone && !mur_options.forward && !mur_options.rollback_losttnonly
				&& murgbl.clean_exit && (NULL != rctl->jctl_turn_around))
			{	/* Some backward processing and possibly forward processing was done. do some cleanup */
				/* It is possible forward processing did updates and wrote to newly created journal files.
				 * If so, make sure those are hardened to disk BEFORE destroying information maintained in
				 * the journal files to help with interrupted-recovery/rollback (next_jnl_file_name field etc.)
				 * just in case this rollback/recover gets killed before it completes.
				 */
				was_crit = csa->now_crit;
				if (!was_crit)
					grab_crit(reg);
				assert(JNL_ENABLED(csd));
				jnl_status = jnl_ensure_open(reg, csa);
				assert(0 == jnl_status);
				if (0 == jnl_status)
				{
					jpc = csa->jnl;
					assert(NOJNL != jpc->channel);
					jb = jpc->jnl_buff;
					jnl_flush(reg);
					assert(jb->freeaddr == jb->dskaddr);
					assert(jb->rsrv_freeaddr == jb->freeaddr);
					jnl_fsync(reg, jb->dskaddr);
					assert(jb->fsync_dskaddr == jb->dskaddr);
				}
				if (!was_crit)
					rel_crit(reg);
				assert(NULL == rctl->jctl_turn_around || NULL != rctl->jctl_head);
				jctl = rctl->jctl_turn_around;
				head_jnl_fn_len = jctl->jnl_fn_len;
				head_jnl_fn = fn;
				memcpy(head_jnl_fn, jctl->jnl_fn, head_jnl_fn_len);
				/* reset jctl->jfh->recover_interrupted field in all recover created jnl files to signal
				 * that a future recover should not consider this recover as an interrupted recover.
				 */
				jctl = &jctl_temp;
				memset(&jctl_temp, 0, SIZEOF(jctl_temp));
				jctl->jnl_fn_len = csd->jnl_file_len;
				memcpy(jctl->jnl_fn, csd->jnl_file_name, jctl->jnl_fn_len);
				jctl->jnl_fn[jctl->jnl_fn_len] = 0;
				while (0 != jctl->jnl_fn_len)
				{
					if ((jctl->jnl_fn_len == head_jnl_fn_len)
							&& !memcmp(jctl->jnl_fn, head_jnl_fn, jctl->jnl_fn_len))
						break;
					if (SS_NORMAL != mur_fopen(jctl, rctl))
					{	/* if opening the journal file failed, we cannot do anything here */
						wrn_count++;
						/* mur_fopen() would have done the appropriate gtm_putmsg() */
						break;
					}
					/* at this point jctl->jfh->recover_interrupted is expected to be TRUE
					 * except in a few cases like mur_back_process() encountered an error in
					 * "mur_insert_prev" because of missing journal.
					 * in that case we would not have gone through mur_process_intrpt_recov()
					 * so we would not have created new journal files.
					 */
					if (jctl->jfh->recover_interrupted)
					{
						jctl->jfh->recover_interrupted = FALSE;
						/* Since overwriting the journal file header (an already allocated block
						 * in the file) should not cause ENOSPC, we don't take the trouble of
						 * passing csa or jnl_fn (first two parameters). Instead we pass NULL.
						 */
						JNL_DO_FILE_WRITE(NULL, NULL, jctl->channel, 0, jctl->jfh, REAL_JNL_HDR_LEN,
							jctl->status, jctl->status2);
						WARN_STATUS(jctl);
					}
					jctl->jnl_fn_len = jctl->jfh->prev_jnl_file_name_length;
					memcpy(jctl->jnl_fn, jctl->jfh->prev_jnl_file_name, jctl->jnl_fn_len);
					jctl->jnl_fn[jctl->jnl_fn_len] = 0;
					if (!mur_fclose(jctl))
						wrn_count++;	/* mur_fclose() would have done the appropriate gtm_putmsg() */
				}
				jctl = rctl->jctl_turn_around;
				assert(!jctl->jfh->recover_interrupted);
				/* reset fields in turn-around-point journal file header to
				 * reflect new virtually truncated journal file */
				assert(jctl->turn_around_offset);
				jctl->jfh->turn_around_offset = 0;
				jctl->jfh->turn_around_time = 0;
				jctl->jfh->is_not_latest_jnl = TRUE;
				jctl->jfh->crash = FALSE;
				jctl->jfh->end_of_data = jctl->turn_around_offset;
				jctl->jfh->eov_timestamp = jctl->turn_around_time;
				jctl->jfh->eov_tn = jctl->turn_around_tn;
				if (mur_options.rollback)
				{
					jctl->jfh->end_seqno = jctl->turn_around_seqno;
					/* jctl->jfh->strm_end_seqno has already been updated in mur_process_intrpt_recov */
				}
				assert(0 == jctl->jfh->prev_recov_end_of_data ||
					jctl->jfh->prev_recov_end_of_data >= jctl->lvrec_off);
				if (0 == jctl->jfh->prev_recov_end_of_data)
					jctl->jfh->prev_recov_end_of_data = jctl->lvrec_off;
				assert(jctl->jfh->prev_recov_blks_to_upgrd_adjust <= rctl->blks_to_upgrd_adjust);
				jctl->jfh->prev_recov_blks_to_upgrd_adjust = rctl->blks_to_upgrd_adjust;
				jctl->jfh->next_jnl_file_name_length = 0;
				/* Since overwriting the journal file header (an already allocated block
				 * in the file) should not cause ENOSPC, we don't take the trouble of
				 * passing csa or jnl_fn (first two parameters). Instead we pass NULL.
				 */
				JNL_DO_FILE_WRITE(NULL, NULL, jctl->channel, 0,
					jctl->jfh, REAL_JNL_HDR_LEN, jctl->status, jctl->status2);
				WARN_STATUS(jctl);
				/* we have to clear next_jnl_file_name fields in the post-turn-around-point journal files.
				 * but if we get killed in this process, a future recover should be able to resume
				 * the cleanup.  since a future recover can only start from the turn-around-point
				 * journal file and follow the next chains, it is important that we remove the next
				 * chain from the end rather than from the beginning.
				 */
				for (end_jctl = jctl; NULL != end_jctl->next_gen; )	/* find the latest gener */
					end_jctl = end_jctl->next_gen;
				for ( ; end_jctl != jctl; end_jctl = end_jctl->prev_gen)
				{	/* Clear next_jnl_file_name fields in the post-turn-around-point journal files */
					assert(0 == end_jctl->turn_around_offset);
					end_jctl->jfh->next_jnl_file_name_length = 0;
					/* Since overwriting the journal file header (an already allocated block
					 * in the file) should not cause ENOSPC, we don't take the trouble of
					 * passing csa or jnl_fn (first two parameters). Instead we pass NULL.
					 */
					JNL_DO_FILE_WRITE(NULL, NULL, end_jctl->channel, 0, end_jctl->jfh, REAL_JNL_HDR_LEN,
						end_jctl->status, end_jctl->status2);
					WARN_STATUS(end_jctl);
					/* Rename journals whose entire contents have been undone with the rolled_bak prefix.
					 * User can decide to delete these.
					 */
					rename_fn = fn;
					rename_fn_len = ARRAYSIZE(fn);
					ustatus = prepare_unique_name((char *)end_jctl->jnl_fn, end_jctl->jnl_fn_len,
								PREFIX_ROLLED_BAK, "", rename_fn, &rename_fn_len, 0, &ustatus2);
					/* We have allocated enough space in rename_fn/fn array to store PREFIX_ROLLED_BAK
					 * prefix. So no way the above "prepare_unique_name" call can fail. Hence the assert.
					 */
					assert(SS_NORMAL == ustatus);
					WAIT_FOR_REPL_INST_UNFREEZE_SAFE(csa);	/* wait for instance freeze
										 * before journal file renames.
										 */
					if (SS_NORMAL == gtm_rename((char *)end_jctl->jnl_fn, end_jctl->jnl_fn_len,
											rename_fn, rename_fn_len, &ustatus2))
					{
						gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT (6) ERR_FILERENAME, 4,
							end_jctl->jnl_fn_len, end_jctl->jnl_fn, rename_fn_len, rename_fn);
					} else
					{
						gtm_putmsg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_RENAMEFAIL, 4,
							end_jctl->jnl_fn_len, end_jctl->jnl_fn, rename_fn_len, rename_fn);
						wrn_count++;
					}
				} /* end for */
			}
		} /* end if (reg->open) */
		for (jctl = rctl->jctl_head; NULL != jctl; )
		{	/* NULL value of jctl_head possible if we errored out in mur_open_files() before constructing jctl list.
			 * Similarly jctl->reg_ctl could be NULL in such cases. We use murgbl.clean_exit to check for that.
			 */
			assert((jctl->reg_ctl == rctl) || (!murgbl.clean_exit && (NULL == jctl->reg_ctl)));
			prev_jctl = jctl;
			jctl = jctl->next_gen;
			if (!mur_fclose(prev_jctl))
				wrn_count++;	/* mur_fclose() would have done the appropriate gtm_putmsg() */
		}
		rctl->jctl_head = NULL;	/* So that we do not come to above loop again */
	}
	/* If -ROLLBACK -BACKWARD, we better have the standalone lock. The only exception is if we could not get standalone access
	 * (due to some other process still accessing the instance file and/or db/jnl). In that case "clean_exit" should be FALSE.
	 */
	assert(!mur_options.rollback || mur_options.forward || murgbl.repl_standalone || !murgbl.clean_exit);
	if (mur_options.rollback)
	{
		if (mur_options.forward && murgbl.clean_exit && murgbl.consist_jnl_seqno && strm_seqno_nonzero)
		{	/* For cleanly exiting forward rollback, issue RLBKSTRMSEQ message now.
			 * The "strm_seqno_nonzero" check above accomplishes the equivalent of "if (inst_hdr->is_supplementary)"
			 * (used in backward rollback to determine if RLBKSTRMSEQ message needs to be printed or not)
			 * for forward rollback. For backward rollback we do it a little later.
			 */
			assert(!mur_options.rollback_losttnonly);
			assert(!murgbl.repl_standalone);
			for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
			{
				this_strm_seqno = max_strm_seqno[idx];
				if ((0 == idx) && !this_strm_seqno)
					this_strm_seqno = 1;
				if (this_strm_seqno)
				{
					assert(0 == GET_STRM_INDEX(this_strm_seqno));
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_RLBKSTRMSEQ, 3, idx,
								&this_strm_seqno, &this_strm_seqno);
				}
			}
			/* Note: The above RLBKSTRMSEQ printing code is copied from similar code for backward rollback below */
		} else if (murgbl.repl_standalone)
		{	/* In case of -ROLLBACK -BACKWARD, do some replication instance file related cleanup.
			 * For -ROLLBACK -FORWARD, we do not touch the instance file.
			 */
			assert(!mur_options.forward);	/* or else murgbl.repl_standalone won't be TRUE
							 * (needed to get in this "if" block)
							 */
			assert(NULL != jnlpool);
			reg = jnlpool->jnlpool_dummy_reg;
			if (NULL == reg)
				reg = recvpool.recvpool_dummy_reg;	/* in case jnlpool not fully setup */
			assert(NULL != reg);
			udi = FILE_INFO(reg);
			csa = &udi->s_addrs;
			ASSERT_HOLD_REPLPOOL_SEMS;
			if (murgbl.clean_exit && !mur_options.rollback_losttnonly && murgbl.consist_jnl_seqno)
			{	/* The database has been successfully rolled back by the MUPIP JOURNAL ROLLBACK command */
				if (inst_hdr->is_supplementary)
				{	/* For supplementary instance, set strm_seqno[] appropriately in the instance file header.
					 * History record truncating function (invoked below) relies on this.
					 */
					for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
					{
						this_strm_seqno = max_strm_seqno[idx];
						/* Since this is a supplementary instance, the 0th stream should have seqno of
						 * at least 1. In case of a rollback, it is possible that this seqno is being
						 * reset to 0 (as we get this value from the EPOCH record in backward processing
						 * which could be 0 even though the next expected seqno is 1) so reset it to 1
						 * instead in that case. See repl_inst_create.c & gtmsource_seqno_init.c for
						 * more such 0 -> 1 seqno adjustments.
						 * For stream #s 1 thru 15, check if there is a non-zero uuid information in the
						 * instance file header. To avoid REPLINSTNOHIST errors the next time some
						 * communication happens on this stream #, reset it to 1. See the function
						 * "repl_inst_histinfo_truncate" for similar adjustments.
						 */
						if (!this_strm_seqno
							&& ((0 == idx)
								|| IS_REPL_INST_UUID_NON_NULL(inst_hdr->strm_group_info[idx - 1])))
							this_strm_seqno = 1;
						inst_hdr->strm_seqno[idx] = this_strm_seqno;
						if (this_strm_seqno)
						{
							assert(0 == GET_STRM_INDEX(this_strm_seqno));
							gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_RLBKSTRMSEQ, 3, idx,
										&this_strm_seqno, &this_strm_seqno);
						}
					}
				}
				if (!jgbl.onlnrlbk || murgbl.incr_db_rlbkd_cycle)
				{	/* Virtually truncate the history in the replication instance file if necessary. For online
					 * rollback, we should truncate the history records ONLY if the instance was actually rolled
					 * back (indicated by incr_db_rlbkd_cycle). Also, the repl_inst_histinfo_truncate function
					 * expects the caller to hold journal pool crit if jnlpool exists. We can come here with
					 * journal pool if:
					 * (a) ONLINE ROLLBACK
					 * (b) Regular ROLLBACK with Anticipatory Freeze scheme
					 */
					if (jnlpool->jnlpool_ctl && !(was_crit = csa->now_crit))	/* note: assignment */
					{
						assert(!jgbl.onlnrlbk);
						assert(NULL != jnlpool);
						assert(NULL != jnlpool->jnlpool_dummy_reg);
						assert(anticipatory_freeze_available);
						assert(!csa->hold_onto_crit);
						grab_lock(jnlpool->jnlpool_dummy_reg, TRUE, ASSERT_NO_ONLINE_ROLLBACK);
					}
					last_histinfo_seqno = repl_inst_histinfo_truncate(murgbl.consist_jnl_seqno);
					if ((NULL != jnlpool->jnlpool_ctl) && !was_crit)
						rel_lock(jnlpool->jnlpool_dummy_reg);
					/* The above also updates "repl_inst_filehdr->jnl_seqno". If regular rollback, it also
					 * updates "repl_inst_filehdr->crash" to FALSE. For online rollback, we have to update
					 * the crash field ONLY if there is NO journal pool and that is done below.
					 */
					if ((NULL != jnlpool->jnlpool_ctl) && jgbl.onlnrlbk)
					{	/* journal pool still exists and some backward and forward processing happened. More
						 * importantly, the database was taken to a prior logical state. Refresh the journal
						 * pool fields to reflect the new state.
						 */
						assert(csa->now_crit && csa->hold_onto_crit);
						jnlpool->jnlpool_ctl->last_histinfo_seqno = last_histinfo_seqno;
						jnlpool->jnlpool_ctl->jnl_seqno = murgbl.consist_jnl_seqno;
						jnlpool->jnlpool_ctl->start_jnl_seqno = murgbl.consist_jnl_seqno;
						jnlpool->jnlpool_ctl->rsrv_write_addr = jnlpool->jnlpool_ctl->write_addr = 0;
						jnlpool->jnlpool_ctl->rsrv_write_addr = 0;
						assert(jnlpool->jnlpool_ctl->phase2_commit_index1
							== jnlpool->jnlpool_ctl->phase2_commit_index2);
						jnlpool->jnlpool_ctl->lastwrite_len = 0;
						jnlpool->jnlpool_ctl->max_zqgblmod_seqno = max_zqgblmod_seqno;
						/* Keep strm_seqno in journal pool in sync with the one in instance file header */
						assert(SIZEOF(jnlpool->jnlpool_ctl->strm_seqno) == SIZEOF(inst_hdr->strm_seqno));
						memcpy(jnlpool->jnlpool_ctl->strm_seqno, inst_hdr->strm_seqno,
										MAX_SUPPL_STRMS * SIZEOF(seq_num));
						if (!jnlpool->jnlpool_ctl->upd_disabled)
						{	/* Simulate a fresh instance startup by writing a new history record with
							 * the rollback'ed sequence number. This is required as otherwise the source
							 * server startup will NOT realize that receiver server needs to rollback or
							 * will incorrectly conclude a wrong resync sequence number to be passed on
							 * to the receiver server.
							 */
							gtmsource_rootprimary_init(murgbl.consist_jnl_seqno);
						}
					}
				}
#				ifdef DEBUG
				else if (murgbl.incr_onln_rlbk_cycle)
				{	/* database was updated, but the logical state is unchanged. We need to make sure
					 * the jnlpool structures have sane and expected values
					 */
					if (NULL != jnlpool->jnlpool_ctl)
					{	/* journal pool exists */
						assert(jnlpool->jnlpool_ctl->jnl_seqno == murgbl.consist_jnl_seqno);
						assert(jnlpool->jnlpool_ctl->start_jnl_seqno <= murgbl.consist_jnl_seqno);
						assert(jnlpool->jnlpool_ctl->max_zqgblmod_seqno == max_zqgblmod_seqno);
						if (inst_hdr->is_supplementary)
						{
							for (idx = 0; MAX_SUPPL_STRMS > idx; idx++)
								assert((NULL != jnlpool->jnlpool_ctl)
									|| (jnlpool->jnlpool_ctl->strm_seqno[idx]
										== inst_hdr->strm_seqno[idx]));
						}
					}
				}
#				endif
				inst_hdr->file_corrupt = FALSE;
				/* Reset seqnos in "gtmsrc_lcl" in case it is greater than seqno the db is being rolled back to */
				repl_inst_read(udi->fn, (off_t)REPL_INST_HDR_SIZE, (sm_uc_ptr_t)gtmsrc_lcl_array, GTMSRC_LCL_SIZE);
				for (idx = 0; idx < NUM_GTMSRC_LCL; idx++)
				{	/* Check if the slot is being used and only then check the resync_seqno */
					if ('\0' != gtmsrc_lcl_array[idx].secondary_instname[0])
					{
						if (gtmsrc_lcl_array[idx].resync_seqno > murgbl.consist_jnl_seqno)
							gtmsrc_lcl_array[idx].resync_seqno = murgbl.consist_jnl_seqno;
						if (gtmsrc_lcl_array[idx].connect_jnl_seqno > murgbl.consist_jnl_seqno)
							gtmsrc_lcl_array[idx].connect_jnl_seqno = murgbl.consist_jnl_seqno;
					}
				}
				repl_inst_write(udi->fn, (off_t)REPL_INST_HDR_SIZE, (sm_uc_ptr_t)gtmsrc_lcl_array, GTMSRC_LCL_SIZE);
			}
			if (((NULL != jnlpool) && (NULL != jnlpool->jnlpool_ctl)) && jgbl.onlnrlbk)
			{	/* Remove any locks that we acquired in mur_open_files.
				 * Needs to be done even if this is NOT a clean exit.
				 */
				assert(0 != jnlpool->jnlpool_ctl->onln_rlbk_pid || !murgbl.clean_exit);
				assert((csa->now_crit && csa->hold_onto_crit) || !murgbl.clean_exit);
				jnlpool->jnlpool_ctl->onln_rlbk_pid = 0;
				if (csa->now_crit)
					rel_lock(jnlpool->jnlpool_dummy_reg);
				gtmsourcelocal_ptr = &jnlpool->gtmsource_local_array[0];
				for (idx = 0; NUM_GTMSRC_LCL > idx; idx++, gtmsourcelocal_ptr++)
				{
					latch = &gtmsourcelocal_ptr->gtmsource_srv_latch;
					assert((latch->u.parts.latch_pid == process_id) || !murgbl.clean_exit);
					if (latch->u.parts.latch_pid == process_id)
					{	/* need to release the latch */
						rel_gtmsource_srv_latch(latch);
					}
				}
				csa->hold_onto_crit = FALSE;
			}
		}
	}
	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_full_total; rctl < rctl_top; rctl++)
	{
		reg = rctl->gd;
		if (NULL == reg)
			continue;
		udi = (NULL != FILE_CNTL(reg)) ? FILE_INFO(reg) : NULL;
		if (reg->open)
		{
			assert(!mur_options.forward); /* for forward recovery, "gds_rundown" should have been done before */
			gv_cur_region = reg;
			TP_CHANGE_REG(reg);
			assert(!jgbl.onlnrlbk || (cs_addrs->now_crit && cs_addrs->hold_onto_crit) || !murgbl.clean_exit);
			assert(!rctl->standalone || (1 == (semval = semctl(udi->semid, 0, GETVAL))));
			if (jgbl.onlnrlbk)
			{	/* This is an online rollback. If "gtm_mupjnl_parallel" is not 1, multiple child processes were
				 * started to operate on different regions in the forward phase. Any updates they made would not
				 * have been flushed since the children did not go through "gds_rundown". If multiple child
				 * processes were not started, it is possible some GT.M processes (which were active before the
				 * online rollback started) have taken over the flush timers so the rollback process could not get
				 * any timer slots. In either case, it is better to flush the jnl records to disk right away as the
				 * source server could be waiting for these and the sooner it gets them, the better. In the
				 * multiple-child processes case, not doing the flush here can actually cause the source server to
				 * timeout with a SEQNUMSEARCHTIMEOUT error (if no GT.M processes have any flush timers active and
				 * if online rollback does not do the flush either) so it is actually necessary.
				 */
				assert(!FROZEN_CHILLED(cs_addrs));
				wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH | WCSFLU_SYNC_EPOCH);
			}
			/* Note: udi/csa is used a little later after the "gds_rundown" call to determine if "db_ipcs_reset"
			 * can be called so pass CLEANUP_UDI_FALSE as the parameter.
			 */
			rundown_status = gds_rundown(CLEANUP_UDI_FALSE); /* does the final rel_crit */
			if (EXIT_NRM != rundown_status)
				wrn_count++;
			assert((EXIT_NRM != rundown_status) || !rctl->standalone
			       || (1 == (semval = semctl(udi->semid, 0, GETVAL))));
			assert(!cs_addrs->now_crit && !cs_addrs->hold_onto_crit);
		}
		/* If this was a RECOVER/ROLLBACK and rctl->standalone is FALSE, then gvcst_init/mu_rndwn_file did not happen in
		 * successfully for this region. Increment wrn_count in this case
		 */
		assert(!mur_options.update || rctl->standalone || !murgbl.clean_exit);
		if (rctl->standalone && (EXIT_NRM == rundown_status))
			/* Avoid db_ipcs_reset if "gds_rundown" did not remove shared memory */
			if ((NULL != udi) && udi->shm_deleted && !db_ipcs_reset(reg))
				wrn_count++;
		rctl->standalone = FALSE;
		rctl->gd = NULL;
		rctl->csa = NULL;
		if (NULL != rctl->mur_desc)	  /* mur_desc buffers were allocated at mur_open_files time for this region */
			mur_rctl_desc_free(rctl); /* free them up now */
		assert(NULL == rctl->mur_desc);
	}
	if (mur_options.rollback && murgbl.repl_standalone)
	{
		assert(!mur_options.forward); /* or else murgbl.repl_standalone won't be TRUE (needed to get in this "if" block) */
		assert((NULL != jnlpool) && (jnlpool->jnlpool_dummy_reg));
		udi = FILE_INFO(jnlpool->jnlpool_dummy_reg);
		ASSERT_HOLD_REPLPOOL_SEMS;
		/* repl_inst_read and mu_replpool_release_sem expects that the caller holds the ftok semaphore as it is about to
		 * read the replication instance file and assumes there are no concurrent writers. However, ROLLBACK grabs all the
		 * access control semaphores of both jnlpool and receiver pool as well as the replication locks in mur_open_files.
		 * This means -
		 * (a) No replication servers can startup as they cannot go beyond obtaining ftok lock in jnlpool_init or
		 *     recvpool_init as they will be hung waiting for the access control semaphores to be released by ROLLBACK
		 * (b) The already running replication servers will also be hung waiting for critical section to be released
		 *     by rollback.
		 * Attempting to obtain the ftok lock at this point will only increase the possibility of a deadlock if a concurrent
		 * replication server tries to start up as it will hold the ftok lock and wait for access control lock while we
		 * hold the access control and wait for the ftok lock. But, the deadlock only exists if we "wait" for the ftok.
		 * Instead, we can call ftok_sem_lock with immediate=TRUE. If we get it, we can go ahead and remove the semaphores
		 * we created. If we couldn't get it, we just "release" them and whoever is waiting on it will take care of doing
		 * the cleanup.
		 */
		repl_inst_read(udi->fn, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
		repl_instance.file_corrupt = inst_hdr->file_corrupt;
		if ((NULL == jnlpool) || (NULL == jnlpool->jnlpool_ctl))
			repl_instance.crash = inst_hdr->crash = FALSE;
		else
		{	/* Online Rollback OR Anticipatory Freeze is in effect. Detach from the journal pool as all the database
			 * writes are now over. Since the attach count is 1 (we are the only one attached) go ahead and remove the
			 * journal pool. We've already flushed all the contents to the instance file at the beginning of rollback
			 */
			assert(anticipatory_freeze_available || jgbl.onlnrlbk);
			assert(INVALID_SHMID != repl_instance.jnlpool_shmid);
			/* Receive pool is typically rundown by almost all callers of mu_rndwn_repl_instance except for ONLINE
			 * ROLLBACK which can keep it up and running if no one is attached to it (by design). Note: This is just
			 * an assert to validate our understanding and has no implications in PRO
			 */
			assert(INVALID_SHMID == repl_instance.recvpool_shmid || jgbl.onlnrlbk);
			/* Check frozen state before detaching the journal pool */
			inst_frozen = IS_REPL_INST_FROZEN;
			/* Ensure that no new processes have attached to the journal pool */
			if (-1 == shmctl(repl_instance.jnlpool_shmid, IPC_STAT, &shm_buf))
			{
				save_errno = errno;
				assert(FALSE);
				ISSUE_REPLPOOLINST(save_errno, repl_instance.jnlpool_shmid, repl_instance.inst_info.this_instname,
							"shmctl()");
			}
			JNLPOOL_SHMDT(jnlpool, status, save_errno);
			if (-1 == status)
			{
				ISSUE_REPLPOOLINST(save_errno, repl_instance.jnlpool_shmid, repl_instance.inst_info.this_instname,
							"shmdt()");
				assert(FALSE);
			}
			if ((1 == shm_buf.shm_nattch) && !inst_frozen)
			{	/* We are the only one attached. Go ahead and remove the shared memory ID and invalidate it in the
				 * instance file header as well.
				 */
				if (-1 == shm_rmid(repl_instance.jnlpool_shmid))
				{
					save_errno = errno;
					assert(FALSE);
					ISSUE_REPLPOOLINST(save_errno, repl_instance.jnlpool_shmid, repl_instance.inst_info.
								this_instname, "shm_rmid()");
				}
				ipcs_ptr = i2asc((uchar_ptr_t)ipcs_buff, repl_instance.jnlpool_shmid);
				*ipcs_ptr = '\0';
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUJPOOLRNDWNSUC, 4, LEN_AND_STR(ipcs_buff),
						LEN_AND_STR(udi->fn));
				/* Now that the journal pool shared memory is removed, go ahead and invalidate it in the file
				 * header. Note that we cannot reset the halted fields (ftok_counter_halted) because the
				 * processes that are attached to the journal pool OR the receive pool bump the ftok counter.
				 * All we know is that there is no one attached to the journal pool now. We don't know anything
				 * about the receive pool. Resetting ftok_counter_halted implies the ftok counter is back to
				 * being reliable and that is not correct. We leave it unchanged instead of attaching to the
				 * receive pool shmid and determining the # of pids attached to it.
				 */
				repl_instance.jnlpool_shmid = INVALID_SHMID;
				repl_instance.jnlpool_shmid_ctime = 0;
				repl_instance.crash = FALSE;	/* reset crash bit as the journal pool no longer exists */
			}
			inst_hdr = &repl_instance; /* now that shared memory is gone, re-point inst_hdr to the one read from file */
		}
		/* flush the instance file header to the disk before releasing the semaphores. This way, any process waiting on
		 * the semaphores will not notice stale values for "crash" and "file_corrupt" flags leading to incorrect
		 * REPLREQROLLBACK errors. This means writing the instance file header twice (first here and the second one a little
		 * later (for flushing the sem-id fields). But, these should have negligible penalty as the file system caches the
		 * disk reads/writes.
		 */
		repl_inst_write(udi->fn, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
		assert((NULL != jnlpool) && (NULL != jnlpool->jnlpool_dummy_reg));
		got_ftok = ftok_sem_lock(jnlpool->jnlpool_dummy_reg, TRUE); /* immediate=TRUE */
		/* Note: The decision to remove the Journal Pool Access Control Semaphores should be based on two things:
		 * 1. If we have the ftok on the instance file
		 * 			AND
		 * 2. If the instance is NOT crashed indicating that Journal Pool is rundown as well. This condition ensures that
		 *    we don't cause a situation where the Journal Pool is left-around but the semaphores are removed which is an
		 *    out-of-design situation.
		 */
		mu_replpool_release_sem(&repl_instance, JNLPOOL_SEGMENT, got_ftok && !repl_instance.crash);
		mu_replpool_release_sem(&repl_instance, RECVPOOL_SEGMENT, got_ftok);
		if (got_ftok)
			ftok_sem_release(jnlpool->jnlpool_dummy_reg, FALSE, TRUE); /* immediate=TRUE */
		ASSERT_DONOT_HOLD_REPLPOOL_SEMS;
		assert(jgbl.onlnrlbk || inst_frozen ||
			((INVALID_SEMID == repl_instance.jnlpool_semid) && (0 == repl_instance.jnlpool_semid_ctime)));
		assert(jgbl.onlnrlbk || inst_frozen ||
			((INVALID_SEMID == repl_instance.recvpool_semid) && (0 == repl_instance.recvpool_semid_ctime)));
		repl_inst_write(udi->fn, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
		/* Now that the standalone access is released, we should decrement the counter in the ftok semaphore obtained in
		 * mu_rndwn_repl_instance(). If the counter is zero, ftok_sem_release will automatically remove it from the system
		 * Since we should be holding the ftok lock to release it, grab the ftok lock first. We don't expect ftok_sem_lock
		 * to error out because the semaphore should still exist in the system
		 */
		assert(udi->counter_ftok_incremented || jgbl.onlnrlbk || anticipatory_freeze_available);
		if (!ftok_sem_lock(jnlpool->jnlpool_dummy_reg, FALSE)
				|| !ftok_sem_release(jnlpool->jnlpool_dummy_reg, udi->counter_ftok_incremented, FALSE))
			wrn_count++;
	}
	if (jgbl.onlnrlbk)
	{	/* Signal completion */
		assert(NULL != jnlpool);
		assert(((NULL != inst_hdr) && (udi == FILE_INFO(jnlpool->jnlpool_dummy_reg))) || !murgbl.clean_exit);
		finish_err_code = murgbl.clean_exit ? ERR_ORLBKCMPLT : ERR_ORLBKTERMNTD;
		assert(!murgbl.repl_standalone || ((NULL != inst_hdr) && (NULL != udi)));
		if (murgbl.repl_standalone)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) finish_err_code, 4,
					LEN_AND_STR(inst_hdr->inst_info.this_instname), LEN_AND_STR(udi->fn));
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) finish_err_code, 4,
					LEN_AND_STR(inst_hdr->inst_info.this_instname), LEN_AND_STR(udi->fn));
		}
	}
	mur_close_file_extfmt(IN_MUR_CLOSE_FILES_TRUE);
	mur_free();	/* free up whatever was allocated by "mur_init" */
	if (wrn_count)
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (1) ERR_JNLACTINCMPLT);
	else if (!mupip_exit_status_displayed)
	{	/* This exit path is not coming through "mupip_exit". Print an error message indicating incomplete recovery. */
		/* Since this exit is not through "mupip_exit" we expect murgbl.clean_exit to be set to FALSE. There is one
		 * exception and that is if after setting murgbl.clean_exit to TRUE in mupip_recover, we got interrupted by an
		 * external signal that in turn took us to exit handling. In that case forced_exit_err would be set so assert that.
		 */
		assert(!murgbl.clean_exit || forced_exit_err);
		if (murgbl.wrn_count)
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (1) ERR_JNLACTINCMPLT);
		else
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (1) ERR_MUNOACTION);
	} else if (murgbl.clean_exit && !murgbl.wrn_count)
		JNL_SUCCESS_MSG(mur_options);
 	JNL_PUT_MSG_PROGRESS("End processing");
	mupip_exit_status_displayed = TRUE;
	mur_close_files_done = TRUE;
#	if defined(DEBUG)
	if (WBTEST_ENABLED(WBTEST_RECOVER_ENOSPC) && (0 == gtm_white_box_test_case_count))
		util_out_print("Total number of writes !UL",TRUE, gtm_wbox_input_test_case_count);
#	endif
	return (0 == wrn_count);
}
