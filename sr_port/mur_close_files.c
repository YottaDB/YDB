/****************************************************************
 *								*
 *	Copyright 2003, 2011 Fidelity Information Services, Inc	*
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
#if defined(UNIX)
#include "gtm_unistd.h"
#elif defined(VMS)
#include <rms.h>
#include <iodef.h>
#include <psldef.h>
#include <ssdef.h>
#include <efndef.h>
#include "iosb_disk.h"
#endif
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
#if defined(UNIX)
#include "mu_rndwn_replpool.h"
#include "ftok_sems.h"
#include "repl_instance.h"
#include "repl_msg.h"
#include "gtmsource.h"
#endif
#include "util.h"
#ifdef DEBUG
#include "wbox_test_init.h"
#endif

#define WARN_STATUS(jctl)									\
if (SS_NORMAL != jctl->status)									\
{												\
	assert(FALSE);										\
	if (SS_NORMAL != jctl->status2)								\
		gtm_putmsg(VARLSTCNT1(6) ERR_JNLWRERR, 2, jctl->jnl_fn_len, jctl->jnl_fn,	\
			jctl->status, PUT_SYS_ERRNO(jctl->status2));				\
	else											\
		gtm_putmsg(VARLSTCNT(5) ERR_JNLWRERR,						\
			2, jctl->jnl_fn_len, jctl->jnl_fn, jctl->status);			\
	wrn_count++;										\
}												\

GBLREF	void		(*call_on_signal)();
GBLREF	jnl_gbls_t	jgbl;
GBLREF	mur_opt_struct	mur_options;
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	mur_gbls_t	murgbl;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	gd_region	*gv_cur_region;
GBLREF	char		*jnl_state_lit[];
GBLREF	char		*repl_state_lit[];
GBLREF	boolean_t	mupip_exit_status_displayed;
GBLREF	boolean_t	mur_close_files_done;

#ifdef UNIX
GBLREF	jnlpool_addrs	jnlpool;
#endif

error_def(ERR_FILERENAME);
error_def(ERR_JNLACTINCMPLT);
error_def(ERR_JNLBADLABEL);
error_def(ERR_JNLREAD);
error_def(ERR_JNLSTATE);
error_def(ERR_JNLSTRESTFL);
error_def(ERR_JNLSUCCESS);
error_def(ERR_JNLWRERR);
error_def(ERR_MUJNLSTAT);
error_def(ERR_MUNOACTION);
error_def(ERR_PREMATEOF);
error_def(ERR_RENAMEFAIL);
error_def(ERR_REPLSTATE);
UNIX_ONLY(error_def(ERR_REPLFTOKSEM);)
VMS_ONLY(error_def(ERR_SETREG2RESYNC);)


void	mur_close_files(void)
{
	reg_ctl_list		*rctl, *rctl_top;
	jnl_ctl_list		jctl_temp, *jctl, *prev_jctl, *end_jctl;
	gd_region		*reg;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	sgmnt_data		csd_temp;
	int			head_jnl_fn_len, wrn_count = 0;
	uint4			ustatus;
	int4			status;
	char 			*head_jnl_fn, *rename_fn, fn[MAX_FN_LEN];
	int 			rename_fn_len;
	file_control		*fc;
#if defined(VMS)
	boolean_t		set_resync_to_region = FALSE;
	vms_gds_info		*gds_info;
	io_status_block_disk	iosb;
	short			channel;
#elif defined(UNIX)
	int			channel;
	int			idx;
	unix_db_info		*udi;
	gtmsrc_lcl		gtmsrc_lcl_array[NUM_GTMSRC_LCL];
#endif


	if (mur_close_files_done)
	{
		assert(mupip_exit_status_displayed);
		return;
	}
	call_on_signal = NULL;	/* Do not recurs via call_on_signal if there is an error */
	SET_PROCESS_EXITING_TRUE;	/* In case the database is encrypted, this value is used to avoid using
					 * mur_ctl->csd in mur_fopen as it would be invalid due to the gds_rundown() done below.
					 */
	csd = &csd_temp;
	/* If journaling, gds_rundown will need to write PINI/PFIN records. The timestamp of that journal record will
	 * need to be adjusted to the current system time to reflect that it is recovery itself writing that record
	 * instead of simulating GT.M activity. Reset jgbl.dont_reset_gbl_jrec_time to allow for adjustments to gbl_jrec_time.
	 */
	jgbl.dont_reset_gbl_jrec_time = FALSE;
	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_full_total; rctl < rctl_top; rctl++)
	{
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
		 * sure if a gvcst_init was done which in turn requires a gds_rundown to be done.
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
			assert((NULL != rctl->csa) && (rctl->csa == cs_addrs) || (NULL == rctl->csa) && !murgbl.clean_exit);
			csa = cs_addrs;
			assert(NULL != csa);
			if (mur_options.update && JNL_ENABLED(rctl))
				csa->jnl->pini_addr = 0; /* Stop simulation of GTM process journal record writing */
			if (NULL != rctl->jctl && murgbl.clean_exit && mur_options.rollback && !mur_options.rollback_losttnonly)
			{	/* to write proper jnl_seqno in epoch record */
				assert(murgbl.stop_rlbk_seqno >= murgbl.resync_seqno);
				assert(murgbl.stop_rlbk_seqno >= murgbl.consist_jnl_seqno);
				UNIX_ONLY(assert(murgbl.consist_jnl_seqno);)
				if (murgbl.consist_jnl_seqno) /* can be zero if this command is a no-operation in VMS */
					jgbl.mur_jrec_seqno = csa->hdr->reg_seqno = murgbl.consist_jnl_seqno;
				VMS_ONLY(
					if (rctl->jctl->jfh->crash && rctl->jctl->jfh->update_disabled)
						/* Set resync_to_region seqno for a crash and update_disable case */
						set_resync_to_region = TRUE;
				)
			}
			assert(NULL != csa->nl);
			assert((!(mur_options.update ^ csa->nl->donotflush_dbjnl)) || !murgbl.clean_exit);
			if (mur_options.update && (murgbl.clean_exit || !rctl->db_updated) && (NULL != csa->nl))
				csa->nl->donotflush_dbjnl = FALSE;	/* shared memory is now clean for dbjnl flushing */
			gds_rundown();
			if (rctl->standalone && (murgbl.clean_exit || !rctl->db_updated) &&
					!reg->read_only && file_head_read((char *)reg->dyn.addr->fname, csd,
									       SIZEOF(csd_temp)))
			{
				assert(mur_options.update);
				csd->file_corrupt = FALSE;
				if (murgbl.clean_exit)
				{
					if (mur_options.rollback)
						csd->repl_state = rctl->repl_state;
					/* After recover replication state is always closed */
					if (rctl->repl_state != csd->repl_state)
						gtm_putmsg(VARLSTCNT(8) ERR_REPLSTATE, 6, LEN_AND_LIT(FILE_STR),
							DB_LEN_STR(reg), LEN_AND_STR(repl_state_lit[csd->repl_state]));
					if (rctl->jnl_state != csd->jnl_state)
						gtm_putmsg(VARLSTCNT(8) ERR_JNLSTATE, 6, LEN_AND_LIT(FILE_STR),
							DB_LEN_STR(reg), LEN_AND_STR(jnl_state_lit[csd->jnl_state]));
					UNIX_ONLY(
						if (NULL != rctl->jctl && mur_options.rollback && !mur_options.rollback_losttnonly)
						{
							assert(murgbl.consist_jnl_seqno);
							csd->reg_seqno = murgbl.consist_jnl_seqno;
							/* Ensure zqgblmod_seqno never goes above the current reg_seqno. Also
							 * ensure it gets set to non-zero value if instance was former root
							 * primary and this is a fetchresync rollback.
							 */
							if ((csd->zqgblmod_seqno > murgbl.consist_jnl_seqno)
								|| (!csd->zqgblmod_seqno
									&& mur_options.fetchresync_port && murgbl.was_rootprimary))
							{
								csd->zqgblmod_seqno = murgbl.consist_jnl_seqno;
								csd->zqgblmod_tn = csd->trans_hist.curr_tn;
							}
							if (REPL_PROTO_VER_DUALSITE == murgbl.remote_proto_ver)
							{	/* Primary is Dualsite. Update "dualsite_resync_seqno" if needed */
								if (csd->dualsite_resync_seqno > murgbl.consist_jnl_seqno)
									csd->dualsite_resync_seqno = murgbl.consist_jnl_seqno;
							}
						}
					)
					VMS_ONLY(
						if ((NULL != rctl->jctl)
							&& mur_options.rollback
							&& !mur_options.rollback_losttnonly
							&& murgbl.consist_jnl_seqno)
						{
							if (set_resync_to_region)
							{
								csd->resync_seqno = csd->reg_seqno;
								if (mur_options.verbose)
									gtm_putmsg(VARLSTCNT(6) ERR_SETREG2RESYNC, 4,
									&csd->resync_seqno, &csd->reg_seqno, DB_LEN_STR(reg));
							}
							csd->reg_seqno = murgbl.consist_jnl_seqno;
							if (csd->resync_seqno > murgbl.consist_jnl_seqno)
								csd->resync_seqno = murgbl.consist_jnl_seqno;
						}
					)
					csd->intrpt_recov_resync_seqno = 0;
					csd->intrpt_recov_tp_resolve_time = 0;
					csd->intrpt_recov_jnl_state = jnl_notallowed;
					csd->intrpt_recov_repl_state = repl_closed;
					csd->recov_interrupted = FALSE;
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
			} /* else do not restore state */
			if (rctl->standalone && !mur_options.forward && !mur_options.rollback_losttnonly
				&& murgbl.clean_exit && (NULL != rctl->jctl_turn_around))
			{	/* some backward processing and possibly forward processing was done. do some cleanup */
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
					if (!mur_fopen(jctl))
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
						DO_FILE_WRITE(jctl->channel, 0, jctl->jfh, REAL_JNL_HDR_LEN,
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
				jctl->jfh->crash = 0;
				jctl->jfh->end_of_data = jctl->turn_around_offset;
				jctl->jfh->eov_timestamp = jctl->turn_around_time;
				jctl->jfh->eov_tn = jctl->turn_around_tn;
				if (mur_options.rollback)
					jctl->jfh->end_seqno = jctl->turn_around_seqno;
				assert(0 == jctl->jfh->prev_recov_end_of_data ||
					jctl->jfh->prev_recov_end_of_data >= jctl->lvrec_off);
				if (0 == jctl->jfh->prev_recov_end_of_data)
					jctl->jfh->prev_recov_end_of_data = jctl->lvrec_off;
				assert(jctl->jfh->prev_recov_blks_to_upgrd_adjust <= rctl->blks_to_upgrd_adjust);
				jctl->jfh->prev_recov_blks_to_upgrd_adjust = rctl->blks_to_upgrd_adjust;
				jctl->jfh->next_jnl_file_name_length = 0;
				DO_FILE_WRITE(jctl->channel, 0, jctl->jfh, REAL_JNL_HDR_LEN, jctl->status, jctl->status2);
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
					DO_FILE_WRITE(end_jctl->channel, 0, end_jctl->jfh, REAL_JNL_HDR_LEN,
						end_jctl->status, end_jctl->status2);
					WARN_STATUS(end_jctl);
					/* Rename journals whose entire contents have been undone with
					 * the rolled_bak prefix. user can decide to delete these */
					rename_fn = fn;
					prepare_unique_name((char *)end_jctl->jnl_fn, end_jctl->jnl_fn_len,
						PREFIX_ROLLED_BAK, "", rename_fn, &rename_fn_len, &ustatus);
					if (SS_NORMAL == gtm_rename((char *)end_jctl->jnl_fn, end_jctl->jnl_fn_len,
									rename_fn, rename_fn_len, &ustatus))
					{
						gtm_putmsg(VARLSTCNT (6) ERR_FILERENAME, 4, end_jctl->jnl_fn_len,
							end_jctl->jnl_fn, rename_fn_len, rename_fn);
					} else
					{
						gtm_putmsg(VARLSTCNT(6) ERR_RENAMEFAIL, 4,
							end_jctl->jnl_fn_len, end_jctl->jnl_fn, rename_fn_len, rename_fn);
						wrn_count++;
					}
				} /* end for */
			}
		} /* end if (reg->open) */
		rctl->csa = NULL;
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
		UNIX_ONLY(
			if (rctl->standalone && !db_ipcs_reset(reg))
				wrn_count++;
			rctl->standalone = FALSE;
		)
		rctl->gd = NULL;
		if (NULL != rctl->mur_desc)	  /* mur_desc buffers were allocated at mur_open_files time for this region */
			mur_rctl_desc_free(rctl); /* free them up now */
		assert(NULL == rctl->mur_desc);
	}
	mur_close_file_extfmt();
	mur_free();	/* free up whatever was allocated by "mur_init" */
#if defined(UNIX)
	/* If rollback, we better have the standalone lock. The only exception is if we could not get standalone access
	 * (due to some other process still accessing the instance file and/or db/jnl). In that case "clean_exit" should be FALSE.
	 */
	assert(!mur_options.rollback || murgbl.repl_standalone || !murgbl.clean_exit);
	if (mur_options.rollback && murgbl.repl_standalone)
	{
		if (murgbl.clean_exit && !mur_options.rollback_losttnonly && murgbl.consist_jnl_seqno)
		{	/* The database has been successfully rolled back by the MUPIP JOURNAL ROLLBACK command.
			 * Virtually truncate the triple history in the replication instance file if necessary.
			 * Before that we need to get the ftok lock on the instance file as the truncate function requires that.
			 */
			repl_inst_ftok_sem_lock();
			repl_inst_triple_truncate(murgbl.consist_jnl_seqno);
				/* The above also updates "repl_inst_filehdr->jnl_seqno" and "repl_inst_filehdr->crash" */
			udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
			/* Reset seqnos in "gtmsrc_lcl" in case it is greater than seqno that the db is being rolled back to */
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
			repl_inst_ftok_sem_release();
		}
		mu_replpool_remove_sem(FALSE);
		murgbl.repl_standalone = FALSE;
	}
#endif
	if (wrn_count)
		gtm_putmsg(VARLSTCNT (1) ERR_JNLACTINCMPLT);
	else if (!mupip_exit_status_displayed)
	{	/* This exit path is not coming through "mupip_exit". Print an error message indicating incomplete recovery.
		 * The || in the assert below is to take care of a white-box test that primarily tests the
		 * WBTEST_TP_HIST_CDB_SC_BLKMOD scenario but also induces a secondary WBTEST_MUR_ABNORMAL_EXIT_EXPECTED scenario.
		 * WBTEST_JNL_FILE_OPEN_FAIL and WBTEST_JNL_CREATE_FAIL are also accepted since the impossibility to create a
		 * journal file will induce a recovery failure.
		 */
		assert(gtm_white_box_test_case_enabled
			&& ((WBTEST_MUR_ABNORMAL_EXIT_EXPECTED == gtm_white_box_test_case_number)
				|| (WBTEST_TP_HIST_CDB_SC_BLKMOD == gtm_white_box_test_case_number)
				|| (WBTEST_JNL_FILE_OPEN_FAIL == gtm_white_box_test_case_number)
				|| (WBTEST_JNL_CREATE_FAIL == gtm_white_box_test_case_number)));
		assert(!murgbl.clean_exit);
		if (murgbl.wrn_count)
			gtm_putmsg(VARLSTCNT (1) ERR_JNLACTINCMPLT);
		else
			gtm_putmsg(VARLSTCNT (1) ERR_MUNOACTION);
	} else if (murgbl.clean_exit && !murgbl.wrn_count)
		JNL_SUCCESS_MSG(mur_options);
 	JNL_PUT_MSG_PROGRESS("End processing");
	mupip_exit_status_displayed = TRUE;
	mur_close_files_done = TRUE;
}
