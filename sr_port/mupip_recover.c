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

/* This routine processes the MUPIP JOURNAL command */

#include "mdef.h"

#if defined(VMS)
#include <descrip.h>
#endif
#include "gtm_time.h"
#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* For muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "iosp.h"
#include "copy.h"
#include "fcntl.h"
#include "cli.h"
#include "error.h"
#include "stp_parms.h"
#include "send_msg.h"
#include "tp_restart.h"
#include "tp_change_reg.h"
#include "gtmrecv.h"
#include "mupip_exit.h"
#include "dpgbldir.h"
#include "gtmmsg.h"
#include "mupip_recover.h"
#include "wbox_test_init.h"
#include "anticipatory_freeze.h"
#ifdef GTM_TRIGGER
#include "error_trap.h"
#endif
#ifdef UNIX
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_instance.h"
#include "have_crit.h"
#endif

GBLREF	int4			gv_keysize;
GBLREF	gv_namehead		*gv_target;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF  gd_addr         	*gd_header;
GBLREF	gd_binding      	*gd_map;
GBLREF	gd_binding      	*gd_map_top;
#ifdef VMS
GBLREF	struct chf$signal_array	*tp_restart_fail_sig;
GBLREF	boolean_t		tp_restart_fail_sig_used;
#endif
GBLREF	mur_opt_struct		mur_options;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF 	mur_gbls_t		murgbl;
GBLREF	reg_ctl_list		*mur_ctl;
GBLREF  jnl_process_vector	*prc_vec;
#ifdef UNIX
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	repl_conn_info_t	*this_side, *remote_side;
GBLREF	int4			strm_index;
#endif
#ifdef GTM_TRIGGER
DEBUG_ONLY(GBLREF ch_ret_type	(*ch_at_trigger_init)();)
GBLREF	dollar_ecode_type	dollar_ecode;		/* structure containing $ECODE related information */
#endif

error_def(ERR_ASSERT);
error_def(ERR_BLKCNTEDITFAIL);
error_def(ERR_DBCOLLREQ);
error_def(ERR_SETEXTRENV);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_JNLACTINCMPLT);
error_def(ERR_MEMORY);
error_def(ERR_MUINFOUINT4);
error_def(ERR_MUINFOUINT8);
error_def(ERR_MUJNLNOTCOMPL);
error_def(ERR_MUJNLSTAT);
error_def(ERR_MUNOACTION);
error_def(ERR_MUPJNLINTERRUPT);
error_def(ERR_REPEATERROR);
error_def(ERR_REPLINSTNOHIST);
error_def(ERR_RLBKJNSEQ);
error_def(ERR_RLBKLOSTTNONLY);
error_def(ERR_RSYNCSTRMSUPPLONLY);
error_def(ERR_STACKOFLOW);
error_def(ERR_TPRETRY);
error_def(ERR_VMSMEMORY);
#ifdef UNIX
error_def(ERR_REPLINSTDBMATCH);
#endif

void		gtm_ret_code();

CONDITION_HANDLER(mupip_recover_ch)
{
	int	rc;

	START_CH;
	if ((int)ERR_TPRETRY == SIGNAL)
	{
		assert(gtm_white_box_test_case_enabled && (WBTEST_TP_HIST_CDB_SC_BLKMOD == gtm_white_box_test_case_number));
		VMS_ONLY(assert(FALSE == tp_restart_fail_sig_used);)
		rc = tp_restart(1, TP_RESTART_HANDLES_ERRORS);	/* This SHOULD generate an error (TPFAIL or other) */
		GTMTRIG_ONLY(assert(ERR_TPRETRY != rc));
#		ifdef UNIX
		if (ERR_TPRETRY == SIGNAL)		/* (signal value undisturbed) */
#		elif defined VMS
		if (!tp_restart_fail_sig_used)	/* If tp_restart ran clean */
#		else
#		error unsupported platform
#		endif
		{
			GTMASSERT;		/* It should *not* run clean */
		}
#		ifdef VMS
		else
		{	/* Otherwise tp_restart had a signal that we must now deal with -- replace the TPRETRY
			   information with that saved from tp_restart. */
			/* Assert we have room for these arguments - the array malloc is in tp_restart */
			assert(TPRESTART_ARG_CNT >= tp_restart_fail_sig->chf$is_sig_args);
			memcpy(sig, tp_restart_fail_sig, (tp_restart_fail_sig->chf$l_sig_args + 1) * SIZEOF(int));
			tp_restart_fail_sig_used = FALSE;
		}
#		endif
		/* At this point SIGNAL would correspond to TPFAIL (not a TPRETRY) error */
	}
#	ifdef GTM_TRIGGER
	else if (ERR_REPEATERROR == SIGNAL)
		SIGNAL = dollar_ecode.error_last_ecode;	/* Error rethrown from a trigger */
#	endif
	if (SEVERITY == SEVERE || DUMP || SEVERITY == ERROR)
	{
		/* Dont do a PRN_ERROR here as NEXTCH will transfer control to util_base_ch() which does the PRN_ERROR for us */
		NEXTCH;
	} else
	{
		assert(SEVERITY == WARNING || SEVERITY == INFO);
		PRN_ERROR;	/* flush the message that is driving us before resuming execution flow */
		CONTINUE;
	}
}

#ifdef UNIX
/* This function computes the maximum strm_reg_seqno[] across all databases open by rollback for a given "strm_num".
 * Note: If a fetchresync rollback crashes before it completes, it would have updated csd->strm_reg_seqno as it
 * processes updates in the forward phase. In this case, a reissue of rollback should not look at the point where
 * the rollback crashed but at the seqno of the database BEFORE when the first rollback was issued. We identify this
 * by a non-zero value of "save_strm_reg_seqno" and if this is the case use that instead of "strm_reg_seqno".
 */
seq_num	mur_get_max_strm_reg_seqno(int strm_num)
{
	int		regno;
	reg_ctl_list	*rctl;
	seq_num		strm_jnl_seqno, strm_reg_seqno, strm_save_seqno;

	strm_jnl_seqno = 0;
	for (regno = 0; regno < murgbl.reg_total; regno++)
	{
		rctl = &mur_ctl[regno];
		assert(NULL != rctl->csd);
		strm_reg_seqno = rctl->csd->strm_reg_seqno[strm_num];
		strm_save_seqno = rctl->csd->save_strm_reg_seqno[strm_num];
		if (strm_save_seqno)
		{
			if (strm_save_seqno > strm_jnl_seqno)
				strm_jnl_seqno = strm_save_seqno;
		} else if (strm_reg_seqno > strm_jnl_seqno)
			strm_jnl_seqno = strm_reg_seqno;
	}
	return strm_jnl_seqno;
}
#endif

void	mupip_recover(void)
{
	bool			mur_open_files_status;
	boolean_t		all_gen_properly_closed, apply_pblk, ztp_broken, intrrupted_recov_processing;
	char			histdetail[256];
	enum jnl_record_type	rectype;
	int			cur_time_len, regno, reg_total, status;
	jnl_ctl_list		*jctl;
	jnl_tm_t		min_broken_time;
	reg_ctl_list		*rctl;
	seq_num 		losttn_seqno, min_broken_seqno;
#ifdef UNIX
	repl_histinfo		local_histinfo;
	seq_num			max_reg_seqno, replinst_seqno;
	unix_db_info		*udi;
	boolean_t		db_absent = FALSE;
	reg_ctl_list		*db_absent_rctl;
#endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH(mupip_recover_ch);
	GTMTRIG_DBG_ONLY(ch_at_trigger_init = &mupip_recover_ch);
	/* PHASE 1: Process user input, open journal files, create rctl for phase 2 */
	JNL_PUT_MSG_PROGRESS("Initial processing started");
	mur_init();
	mur_get_options();
	/*DEFER_INTERRUPTS(INTRPT_IN_MUR_OPEN_FILES); */
	mur_open_files_status = mur_open_files();
	jgbl.mur_extract = mur_options.extr[GOOD_TN]; /* journal extract process */
	/*ENABLE_INTERRUPTS(INTRPT_IN_MUR_OPEN_FILES);*/
	if (!mur_open_files_status) /* mur_open_files already issued error */
		mupip_exit(ERR_MUNOACTION);
	VMS_ONLY(assert(!mur_options.rollback_losttnonly);)
	UNIX_ONLY(assert(!mur_options.rollback_losttnonly || mur_options.rollback);)
	murgbl.prc_vec = prc_vec;
	reg_total = murgbl.reg_total;
	if (mur_options.show_head_only)
	{
		mur_output_show();
		murgbl.clean_exit = TRUE;
		mupip_exit(SS_NORMAL);
	}
	all_gen_properly_closed = TRUE;
	intrrupted_recov_processing = murgbl.intrpt_recovery = FALSE;
	for (regno = 0; regno < reg_total; regno++)
	{
		rctl = &mur_ctl[regno];
		jctl = rctl->jctl;
		assert(NULL == jctl->next_gen);
#		ifdef UNIX
		if (!rctl->db_present)
		{
			db_absent = TRUE;
			db_absent_rctl = rctl;
		}
#		endif
		if (!jctl->properly_closed)
			all_gen_properly_closed = FALSE;
		if (jctl->jfh->recover_interrupted)
		{
			/* These journal files were created by recover so they should be BEFORE_IMAGE */
			assertpro(jctl->jfh->before_images);
			assertpro(!mur_options.rollback_losttnonly);	/* dont know how one can end up with NOBEFORE_IMAGE jnl
									 * files in intrpt recovery */
			rctl->jfh_recov_interrupted = TRUE;
			intrrupted_recov_processing = murgbl.intrpt_recovery = TRUE;
		} else if (rctl->recov_interrupted) /* it is not necessary to do interrupted recover processing */
		{
			murgbl.intrpt_recovery = TRUE; /* Recovery was interrupted at some point */
			rctl->csa->hdr->turn_around_point = FALSE; /*Reset turn around point field*/
		}
	}
#	ifdef UNIX
	if (!mur_options.update && jgbl.mur_extract && (db_absent || IS_REPL_INST_FROZEN))
	{
		if (TREF(jnl_extract_nocol))
		{
			TREF(jnl_extract_nocol) = !mur_options.update && jgbl.mur_extract && TREF(jnl_extract_nocol);
			if (db_absent)
				gtm_putmsg(VARLSTCNT(6) ERR_DBCOLLREQ, 4, LEN_AND_LIT("Mising Database file"),
						DB_LEN_STR(db_absent_rctl->gd));
			else
				gtm_putmsg(VARLSTCNT(6) ERR_DBCOLLREQ, 4, LEN_AND_LIT("Instance is frozen."),
						LEN_AND_LIT(""));
		} else
		{
			gtm_putmsg(VARLSTCNT(1) ERR_SETEXTRENV);
			mupip_exit(ERR_MUNOACTION);
		}
	} else
		TREF(jnl_extract_nocol) = 0;
	max_reg_seqno = 0;
	for (regno = 0; regno < reg_total; regno++)
	{
		rctl = &mur_ctl[regno];
		jctl = rctl->jctl;
		assert(NULL == jctl->next_gen);
		assert(!mur_options.update || (NULL != rctl->csd));
		assert(!mur_options.rollback || mur_options.update);
		if (mur_options.rollback && (rctl->csd->reg_seqno > max_reg_seqno))
			max_reg_seqno = rctl->csd->reg_seqno;
	}
	assert(!mur_options.fetchresync_port || mur_options.rollback);
	/* If rollback, check if jnl seqno in db and instance file match. Do that only if this is not interrupted
	 * rollback AND if the replication instance file AND all the journal files were cleanly shutdown.
	 */
	if (mur_options.rollback && !intrrupted_recov_processing && all_gen_properly_closed)
	{
		assert(NULL != jnlpool.repl_inst_filehdr);
		replinst_seqno = jnlpool.repl_inst_filehdr->jnl_seqno;
		if (!jnlpool.repl_inst_filehdr->crash && (0 != replinst_seqno) && (max_reg_seqno != replinst_seqno))
		{
			udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
			gtm_putmsg(VARLSTCNT(6) ERR_REPLINSTDBMATCH, 4,
				LEN_AND_STR(udi->fn), &replinst_seqno, &max_reg_seqno);
			mupip_exit(ERR_MUNOACTION);
		}
	}
#	endif
	if (all_gen_properly_closed && !murgbl.intrpt_recovery && !mur_options.forward
	    && ((!mur_options.rollback && !mur_options.since_time_specified &&
		 !mur_options.lookback_time_specified && !mur_options.lookback_opers_specified)
		|| (mur_options.rollback && !mur_options.resync_specified && 0 == mur_options.fetchresync_port)))
	{ 	/* We do not need to do unnecessary processing */
		assert(!mur_options.rollback_losttnonly);
		if (mur_options.show)
			mur_output_show();
		murgbl.clean_exit = TRUE;		 /* "mur_close_files" (invoked from "mupip_exit_handler") relies on this */
#		ifdef UNIX
		if (mur_options.rollback)
		{
			assert(max_reg_seqno);
			murgbl.consist_jnl_seqno = max_reg_seqno;/* "mur_close_files" relies on this */
			DEBUG_ONLY(murgbl.losttn_seqno = MAXUINT8;) /* or else an assert in mur_close_files will fail */
			DEBUG_ONLY(murgbl.save_losttn_seqno = murgbl.losttn_seqno;) /* keep save_losttn_seqno in sync for assert */
			/* Determine if any of the databases are taken back in time. If so we have to increment cycles
			 * in mur_close_files
			 */
			assert(NULL != jnlpool.repl_inst_filehdr);
			murgbl.incr_db_rlbkd_cycle = jgbl.onlnrlbk ? jnlpool.repl_inst_filehdr->jnl_seqno -
									murgbl.consist_jnl_seqno : FALSE;
		}
#		endif
		mupip_exit(SS_NORMAL);
	}
	if (murgbl.intrpt_recovery && mur_options.update && mur_options.forward)
	{
		gtm_putmsg(VARLSTCNT(4) ERR_MUPJNLINTERRUPT, 2, DB_LEN_STR(rctl->gd));
		mupip_exit(ERR_MUNOACTION);
	}
	if (mur_options.update && intrrupted_recov_processing)
	{
		JNL_PUT_MSG_PROGRESS("Interrupted recovery processing started");
		/* Additional steps because recover was interrupted earlier */
		assert(!mur_options.rollback_losttnonly);
		murgbl.ok_to_update_db = TRUE;	/* Allow db to be updated by the PBLKs */
		if (SS_NORMAL != mur_apply_pblk(TRUE))
			mupip_exit(ERR_MUNOACTION);
		murgbl.ok_to_update_db = FALSE;	/* Reset flag until it is safe to allow updates to the db */
		if (!mur_jctl_from_next_gen())
			mupip_exit(ERR_MUNOACTION);
	}
	assert(FALSE == murgbl.ok_to_update_db);
	if (mur_options.rollback_losttnonly)
		gtm_putmsg(VARLSTCNT(1) ERR_RLBKLOSTTNONLY);
	/* The current resync_seqno of this replication instance needs to be calculated before the call to "gtmrecv_fetchresync" */
	VMS_ONLY(jgbl.max_resync_seqno = 0;)
	for (regno = 0; regno < reg_total; regno++)
	{
		rctl = &mur_ctl[regno];
		jctl = rctl->jctl;
		assert(NULL == jctl->next_gen);
		VMS_ONLY(
			if (mur_options.fetchresync_port && rctl->csd->resync_seqno > jgbl.max_resync_seqno)
				jgbl.max_resync_seqno = rctl->csd->resync_seqno;
		)
	}
	UNIX_ONLY(
		if (mur_options.fetchresync_port || mur_options.resync_specified)
		{
			this_side = NULL;
			remote_side = &murgbl.remote_side; /* fields in this structure will be initialized in gtmrecv_fetchresync */
		}
	)
	if (mur_options.fetchresync_port)
	{
		JNL_PUT_MSG_PROGRESS("FETCHRESYNC processing started");
		VMS_ONLY(
			if (SS_NORMAL != gtmrecv_fetchresync(mur_options.fetchresync_port, &murgbl.resync_seqno))
				mupip_exit(ERR_MUNOACTION);
		)
		UNIX_ONLY(
			if (SS_NORMAL != gtmrecv_fetchresync(mur_options.fetchresync_port, &murgbl.resync_seqno, max_reg_seqno))
				mupip_exit(ERR_MUNOACTION);
			if (INVALID_SUPPL_STRM != strm_index)	/* set by "gtmrecv_fetchresync" */
				murgbl.resync_strm_index = strm_index; /* Keep murgbl.resync_seqno in sync with resync_strm_index */
		)
		if (mur_options.verbose)
		{
			UNIX_ONLY(
				if ((INVALID_SUPPL_STRM != murgbl.resync_strm_index) && !remote_side->is_supplementary)
					gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4,
						LEN_AND_LIT("Gtmrecv_fetchresync returned strm_index"),
							murgbl.resync_strm_index, murgbl.resync_strm_index);
			)
			gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("Gtmrecv_fetchresync returned resync_seqno"),
				&murgbl.resync_seqno, &murgbl.resync_seqno);
		}
		VMS_ONLY(
			if (jgbl.max_resync_seqno < murgbl.resync_seqno)
			{
				murgbl.resync_seqno = jgbl.max_resync_seqno;
				if (mur_options.verbose)
					gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT8, 4,
						LEN_AND_LIT("Resync_seqno is reset to max_resync_seqno"),
						&murgbl.resync_seqno, &murgbl.resync_seqno);
			}
		)
	}
#	ifdef UNIX
	else if (mur_options.resync_specified && (INVALID_SUPPL_STRM != murgbl.resync_strm_index))
	{	/* -RESYNC=<strm_seqno> AND -RSYNC_STRM=<strm_num> were specified. Verify input stream is a valid #
		 * and input seqno is a valid strm_seqno in the current replication instance file. If not, issue errors.
		 */
		if (!jnlpool.repl_inst_filehdr->is_supplementary)
		{
			gtm_putmsg(VARLSTCNT(1) ERR_RSYNCSTRMSUPPLONLY);
			mupip_exit(ERR_MUNOACTION);
		}
		if (0 < murgbl.resync_strm_index)
		{	/* For streams 1 thru 15, it is possible that we might have had prior -updateresync or -noresync
			 * invocations which might have hidden some part of the history records in the instance file.
			 * So before doing the rollback, make sure the input resync_seqno has a corresponding history
			 * record. If not, issue a REPLINSTNOHIST error just like a -fetchresync rollback would have done.
			 * For stream 0, no such hiding of history records is possible so we dont have confusion between
			 * the instance file and journal file contents (no possibility of two journal records at different
			 * points in time having the exact same strm_seqno) so we skip this check in that case. Besides,
			 * "repl_inst_histinfo_find_seqno" processes strm_index=0 differently and we dont want that behavior.
			 */
			assert(murgbl.resync_seqno);
			status = repl_inst_histinfo_find_seqno(murgbl.resync_seqno + 1, murgbl.resync_strm_index, &local_histinfo);
			if (0 != status)
			{
				assert(ERR_REPLINSTNOHIST == status);
				SPRINTF(histdetail, "Stream Seqno "INT8_FMT" "INT8_FMTX" (Stream # %2d) ",
					murgbl.resync_seqno - 1, murgbl.resync_seqno - 1, murgbl.resync_strm_index);
				udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
				gtm_putmsg(VARLSTCNT(6) ERR_REPLINSTNOHIST, 4, LEN_AND_STR(histdetail), LEN_AND_STR(udi->fn));
				mupip_exit(ERR_MUNOACTION);
			}
		}
	}
#	endif
	UNIX_ONLY(
		/* Do some adjustments if -resync or -fetchresync was specified and resync_strm_index is non-zero */
		if (murgbl.resync_seqno)
		{
			/* In case the current instance is supplementary and the remote source is also supplementary, and a
			 * resync seqno has been agreed upon, this is the unified seqno and not the stream-specific seqno
			 * even though "murgbl.resync_strm_index" is actually 0 in this case. In this case, do not set
			 * murgbl.resync_strm_seqno.
			 */
			assert(!remote_side->is_supplementary
				|| (jnlpool.repl_inst_filehdr->is_supplementary && (0 == murgbl.resync_strm_index)));
			if ((INVALID_SUPPL_STRM != murgbl.resync_strm_index) && !remote_side->is_supplementary)
			{
				assert((0 <= murgbl.resync_strm_index) && (MAX_SUPPL_STRMS > murgbl.resync_strm_index));
				assert(murgbl.resync_seqno);
				murgbl.resync_strm_seqno[murgbl.resync_strm_index] = murgbl.resync_seqno;
				murgbl.resync_strm_seqno_nonzero = TRUE;
				murgbl.resync_seqno = 0; /* reset resync_seqno now that stream specific seqno has been copied */
				/* From now on, use murgbl.resync_strm_index only (not the global variable "strm_index") */
			}
		}
	)
	/* PHASE 2: Create list of broken transactions for both forward and backward recovery
	 *          In addition apply PBLK for backward recover with noverify */
	apply_pblk = (mur_options.update && !mur_options.forward && !mur_options.rollback_losttnonly && !mur_options.verify);
	murgbl.ok_to_update_db = apply_pblk;	/* Allow db to be updated by the PBLKs if we chose to apply them */
	if (!mur_back_process(apply_pblk, &losttn_seqno))
		mupip_exit(ERR_MUNOACTION);
	if (!mur_options.rollback)
	{
		/* mur_process_token_table returns followings:
		 * 	min_broken_time = token with minimum time stamp of broken entries
		 * 	ztp_broken = TRUE, if any ztp entry is broken */
		min_broken_time = mur_process_token_table(&ztp_broken);
		if (mur_options.verbose)
			gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4, LEN_AND_LIT("mur_process_token_table returns min_broken_time"),
				min_broken_time, min_broken_time);
		min_broken_seqno = losttn_seqno = MAXUINT8;
	} else
		assert(0 != losttn_seqno);
       	/* Multi_region TP/ZTP resolution */
	if (!mur_options.forward)
	{
		if (!mur_options.rollback && FENCE_NONE != mur_options.fences && ztp_broken)
		{
			/* PHASE 3 : ZTP lookback processing phase (not for non-ZTP) */
			JNL_PUT_MSG_PROGRESS("Lookback processing started");
			if (!mur_ztp_lookback())
				mupip_exit(ERR_MUNOACTION);
			/* ZTP lookback processing might or might not have reset tp_resolve_time for some set of regions
			 * to reflect the earliest time when we found a ZTP journal record whose token existed in the
			 * hashtable as a broken ZTP transaction. In either case, we want to examine each ZTP record in the
			 * forward processing phase for brokenness (instead of examining only those records from min_broken_time).
			 * This is because otherwise we might miss out on recognizing broken transactions (see example below).
			 * e.g.
			 *	REGA has tp_resolve_time = t1, epochtime = "t1 - 5", FSET at "t1 + 2", no ZTCOM
			 *	REGB has tp_resolve_time = t1, epochtime = "t1 - 5", FSET at "t1 - 4", no ZTCOM
			 * While processing REGA we find a broken FSET at "t1 + 2" before reaching tp_resolve_time = "t1"
			 * But while processing REGB we do not find anything broken before reaching tp_resolve_time = "t1"
			 * We see the broken FSET at "t1 - 4" before reaching the turn-around-point for REGB but we do not
			 *	consider it as broken since we do not add any records into the hashtable before tp_resolve_time "t1"
			 * This means min_broken_time will correspond to the broken FSET in REGA which is "t1 + 2"
			 * With this value of "min_broken_time" if we process REGA in mur_forward() we will treat the FSET
			 * 	at "t1 + 2" as broken although while processing REGB in mur_forward() we will not treat the
			 * 	FSET at "t1 - 4" as broken (since its timestamp is less than min_broken_time of "t1 + 2").
			 * This is incorrect. To fix this, we instead set min_broken_time = 0. That way all journal records will
			 * 	be examined for brokenness in mur_forward. Note: this is only if there is at least one broken ZTP.
			 */
			min_broken_time = 0;
		}

		/* PHASE 4 : Apply PBLK
		 *          If no ZTP is present and !mur_options.verify, this phase will effectively do nothing.
		 *          If ZTP is present and !mur_options.verify and lookback processing changed tp_resolve_time Then
		 *          	This will do additional PBLK undoing
                 * 	    For mur_options.verify == true following will do complete
		 *	    	PBLK processing (from lvrec_off to turn_around point)
		 */
                if (mur_options.update)
		{	/* If doing a LOSTTNONLY rollback we will not be applying any before images but we want to
			 * invoke "mur_apply_pblk" as it does other things that we need.
			 */
			if (!mur_options.rollback_losttnonly)
			{
				JNL_PUT_MSG_PROGRESS("Before image applying started");
				/* At this time, murgbl.ok_to_update_db could be FALSE if mur_options.verify was TRUE.
				 * Set it to TRUE to let updates to the database now that journals have been verified to be clean.
				 */
				assert(mur_options.verify || murgbl.ok_to_update_db);
				assert(!mur_options.verify || !murgbl.ok_to_update_db);
				murgbl.ok_to_update_db = TRUE;
			}
			if (SS_NORMAL != mur_apply_pblk(FALSE))
				mupip_exit(ERR_MUNOACTION);

			/* PHASE 5 : Update journal file header with current state of recover, so that if this process
			 *	is interrupted, we can recover from it. We already synched updates. Dont do this
			 *	in case of a LOSTTNONLY rollback as we want to avoid touching the database/jnl in this case.
			 */
			if (!mur_options.rollback_losttnonly && (SS_NORMAL != mur_process_intrpt_recov()))
				mupip_exit(ERR_MUNOACTION);
		}
	}
	/* PHASE 6 : Forward processing phase */
	JNL_PUT_MSG_PROGRESS("Forward processing started");
	if (mur_options.rollback)
	{
		mur_process_seqno_table(&min_broken_seqno, &losttn_seqno);
		if (mur_options.verbose)
		{
			gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("mur_process_seqno_table returns min_broken_seqno"),
				&min_broken_seqno, &min_broken_seqno);
			gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("mur_process_seqno_table returns losttn_seqno"),
				&losttn_seqno, &losttn_seqno);
		}
		min_broken_time = MAXUINT4;
	}
	murgbl.ok_to_update_db = (mur_options.update && !mur_options.rollback_losttnonly);
	if (SS_NORMAL != mur_forward(min_broken_time, min_broken_seqno, losttn_seqno))
		mupip_exit(ERR_MUNOACTION);
	assert(prc_vec == murgbl.prc_vec);	/* should have been modified temporarily but finally reset by mur_forward */
	if (mur_options.show)
		mur_output_show();
#	ifdef UNIX
	if (jgbl.onlnrlbk)
	{
		/* Determine if any of the databases are taken back in time. If so we have to increment cycles in mur_close_files */
		assert(NULL != jnlpool.repl_inst_filehdr);
		murgbl.incr_db_rlbkd_cycle = jgbl.onlnrlbk ? jnlpool.repl_inst_filehdr->jnl_seqno -
										murgbl.consist_jnl_seqno : FALSE;
	}
#	endif
	/* PHASE 7 : Close all files, rundown and exit */
	murgbl.clean_exit = TRUE;
	if (mur_options.rollback && !mur_options.rollback_losttnonly)
	{
		assert(murgbl.consist_jnl_seqno <= losttn_seqno);
		assert(murgbl.consist_jnl_seqno <= min_broken_seqno);
		gtm_putmsg(VARLSTCNT(4) ERR_RLBKJNSEQ, 2, &murgbl.consist_jnl_seqno, &murgbl.consist_jnl_seqno);
	}
	if (murgbl.wrn_count)
		mupip_exit(ERR_JNLACTINCMPLT);
	else
		mupip_exit(SS_NORMAL);
}
