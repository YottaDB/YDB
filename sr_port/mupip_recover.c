/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* This routine processes the MUPIP JOURNAL command */

#include "mdef.h"

#include "gtm_time.h"
#include "gtm_string.h"

#ifdef VMS
#include <math.h>	/* for mur_rel2abstime() function */
#endif

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashdef.h"	/* For muprec.h */
#include "buddy_list.h"	/* For muprec.h */
#include "muprec.h"
#include "iosp.h"
#include "mv_stent.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "copy.h"
#include "fcntl.h"
#include "cli.h"
#include "error.h"
#include "stp_parms.h"
#include "util.h"
#include "send_msg.h"
#include "tp_restart.h"
#include "tp_change_reg.h"
#include "gtmrecv.h"
#include "targ_alloc.h"
#include "mupip_exit.h"
#include "dpgbldir.h"
#include "gtmmsg.h"
#include "mupip_recover.h"
#include "mu_gv_stack_init.h"

GBLREF	void			(*call_on_signal)();
GBLREF	int4			gv_keysize;
GBLREF	gv_namehead		*gv_target;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	bool			is_standalone;
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
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF  jnl_process_vector	*prc_vec;

void		gtm_ret_code();

CONDITION_HANDLER(mupip_recover_ch)
{
	error_def(ERR_TPRETRY);
	error_def(ERR_ASSERT);
	error_def(ERR_GTMCHECK);
	error_def(ERR_GTMASSERT);
	error_def(ERR_STACKOFLOW);

	START_CH;
	PRN_ERROR;	/* Flush out the error message that is driving us */
	if ((int)ERR_TPRETRY == SIGNAL)
	{
		assert(FALSE);
		VMS_ONLY(assert(FALSE == tp_restart_fail_sig_used);)
		tp_restart(1);			/* This SHOULD generate an error (TPFAIL or other) */
#ifdef UNIX
		if (ERR_TPRETRY == SIGNAL)		/* (signal value undisturbed) */
#elif defined VMS
		if (!tp_restart_fail_sig_used)	/* If tp_restart ran clean */
#else
#error unsupported platform
#endif
		{
			GTMASSERT;		/* It should *not* run clean */
		}
#ifdef VMS
		else
		{	/* Otherwise tp_restart had a signal that we must now deal with -- replace the TPRETRY
			   information with that saved from tp_restart. */
			/* Assert we have room for these arguments - the array malloc is in tp_restart */
			assert(TPRESTART_ARG_CNT >= tp_restart_fail_sig->chf$is_sig_args);
			memcpy(sig, tp_restart_fail_sig, (tp_restart_fail_sig->chf$l_sig_args + 1) * sizeof(int));
			tp_restart_fail_sig_used = FALSE;
		}
#endif
	}
	if (SEVERITY == SEVERE || DUMP || SEVERITY == ERROR)
	{
		NEXTCH;
	} else
	{
		assert(SEVERITY == WARNING || SEVERITY == INFO);
		CONTINUE;
	}
}

#ifdef VMS
static	int4	mur_rel2abstime(jnl_proc_time deltatime, jnl_proc_time basetime, boolean_t roundup)
{
	/* In VMS, time in journal records is not stored in units of seconds. Instead it is stored in units of epoch-seconds.
	 * (see vvms/jnlsp.h for comment on epoch-seconds). Since an epoch-second is approximately .8388th of a second, it is
	 * possible that two consecutive journal records with timestamps of say t1 and t1+1 might map to the same time in seconds.
	 * A mapping between epoch-seconds and seconds is given below (using the EPOCH_SECOND2SECOND macro)
	 * 	epoch_second = 91 : second = 77
	 * 	epoch_second = 92 : second = 78
	 * 	epoch_second = 93 : second = 79
	 * 	epoch_second = 94 : second = 79
	 * 	epoch_second = 95 : second = 80
	 * 	epoch_second = 96 : second = 81
	 * 	epoch_second = 97 : second = 82
	 * 	epoch_second = 98 : second = 83
	 * 	epoch_second = 99 : second = 84
	 * say basetime  is  83 seconds (this translates to 98 epoch-seconds)
	 * say deltatime is   4 seconds (this translates to  4/.8388 = 4.76 = 4 (rounded down) epoch-seconds)
	 * Now if we do 98 - 4 we get 94 epoch-seconds which maps to 79 seconds which is indeed 4 seconds below 83 seconds.
	 * But notice that even 93 epoch-seconds maps to 79 seconds.
	 * If this time is to be used as since-time or after-time or lookback-time it is 93 epoch-seconds that needs to be taken
	 * 	(instead of 94) as otherwise we might miss out on few journal records that have 93 epoch-second timestamp).
	 * If this time is to be used as before-time, it is 94 epoch-seconds that needs to be considered (instead of 93) as
	 * 	otherwise we will stop at 93 timestamp journal records and miss out on including 94 timestamp journal records
	 * 	although they correspond to the same second which the user sees in the journal extract.
	 * Therefore, it is necessary that a relative to absolute delta-time conversion routine takes care of this.
	 * It is taken care of in the below function mur_rel2abstime.
	 * "roundup" is TRUE in case this function is called for mur_options.before_time and FALSE otherwise.
	 */
	uint4		baseseconds;
	int4		diffseconds, deltaseconds;

	/* because of the way the final journal extract time comes out in seconds, the EPOCH_SECOND2SECOND macro needs to be
	 * passed one more than the input epoch-seconds in order for us to get the exact corresponding seconds unit. wherever
	 * the macro is used below, subtract one to find out the actual epoch-second that is being considered.
	 */
	deltaseconds = EPOCH_SECOND2SECOND(-deltatime);
	baseseconds = EPOCH_SECOND2SECOND(basetime + 1);
	deltatime += basetime;
	diffseconds = baseseconds - EPOCH_SECOND2SECOND(deltatime + 1);
	if (diffseconds < deltaseconds)
	{
		while ((baseseconds - EPOCH_SECOND2SECOND(deltatime + 1)) < deltaseconds)
			deltatime--;
		DEBUG_ONLY(diffseconds = baseseconds - EPOCH_SECOND2SECOND(deltatime + 1);)
		assert(diffseconds == deltaseconds);
	} else if (diffseconds > deltaseconds)
	{
		while ((baseseconds - EPOCH_SECOND2SECOND(deltatime + 1)) > deltaseconds)
			deltatime++;
		DEBUG_ONLY(diffseconds = baseseconds - EPOCH_SECOND2SECOND(deltatime + 1);)
		assert(diffseconds == deltaseconds);
	}
	if (roundup)
	{
		if (EPOCH_SECOND2SECOND(deltatime + 2) == EPOCH_SECOND2SECOND(deltatime + 1))
			deltatime++;
		assert(EPOCH_SECOND2SECOND(deltatime + 1) < EPOCH_SECOND2SECOND(deltatime + 2));
	} else
	{
		if (EPOCH_SECOND2SECOND(deltatime) == EPOCH_SECOND2SECOND(deltatime + 1))
			deltatime--;
		assert(EPOCH_SECOND2SECOND(deltatime) < EPOCH_SECOND2SECOND(deltatime + 1));
	}
	return deltatime;
}
#endif

void	mupip_recover(void)
{
	boolean_t		latest_gen_properly_closed, apply_pblk, ztp_broken, intrrupted_recov_processing;
	enum jnl_record_type	rectype;
	int			cur_time_len, regno, reg_total;
	jnl_tm_t		max_lvrec_time, min_bov_time, min_broken_time;
	seq_num 		losttn_seqno, min_broken_seqno;
	unsigned char		*mstack_ptr;
	reg_ctl_list		*rctl;
	jnl_ctl_list		*jctl;
	char			time_str1[LENGTH_OF_TIME + 1], time_str2[LENGTH_OF_TIME + 1];
	error_def		(ERR_MUNOACTION);
	error_def		(ERR_BLKCNTEDITFAIL);
	error_def		(ERR_JNLTMQUAL1);
	error_def		(ERR_JNLTMQUAL2);
	error_def		(ERR_JNLTMQUAL3);
	error_def		(ERR_JNLTMQUAL4);
	error_def		(ERR_MUJNLSTAT);
	error_def		(ERR_MUJNLNOTCOMPL);
	error_def		(ERR_RLBKJNSEQ);
	error_def		(ERR_JNLACTINCMPLT);
	error_def		(ERR_MUPJNLINTERRUPT);

	ESTABLISH(mupip_recover_ch);
	jgbl.mupip_journal = TRUE;	/* this is a MUPIP JOURNAL command */
	murgbl.db_updated = FALSE;
	call_on_signal = mur_close_files;
	is_standalone = TRUE;
	DEBUG_ONLY(assert_jrec_member_offsets();)
	/* PHASE 1: Process user input, open journal files, create rctl for phase 2 */
	JNL_PUT_MSG_PROGRESS("Initial processing started");
	mur_init();
	murgbl.resync_seqno = 0; 		/* interrupted rollback set this to non-zero value later */
	murgbl.stop_rlbk_seqno = MAXUINT8;	/* allow default rollback to continue forward processing till last valid record */
	mur_get_options();
	mupip_jnl_recover = mur_options.update;
	if (!mur_open_files())
		/* mur_open_files already issued error */
		mupip_exit(ERR_MUNOACTION);
	murgbl.prc_vec = prc_vec;
	reg_total = murgbl.reg_total;
	if (mur_options.show_head_only)
	{
		mur_output_show();
		murgbl.clean_exit = TRUE;
		mupip_exit(SS_NORMAL);
	}
	latest_gen_properly_closed = TRUE;
	intrrupted_recov_processing = murgbl.intrpt_recovery = FALSE;
	for (regno = 0; regno < reg_total; regno++)
	{
		rctl = &mur_ctl[regno];
		jctl = rctl->jctl;
		assert(NULL == jctl->next_gen);
		if (!jctl->properly_closed)
			latest_gen_properly_closed = FALSE;
		if (jctl->jfh->recover_interrupted)
		{
			/* These journal files were created by recover */
			if (!jctl->jfh->before_images)
				GTMASSERT;
			rctl->jfh_recov_interrupted = TRUE;
			intrrupted_recov_processing = murgbl.intrpt_recovery = TRUE;
		} else if (rctl->recov_interrupted) /* it is not necessary to do interrupted recover processing */
			murgbl.intrpt_recovery = TRUE; /* Recovery was interrupted at some point */
	}
	if (latest_gen_properly_closed && !murgbl.intrpt_recovery && !mur_options.forward
		&& ((!mur_options.rollback && !mur_options.since_time_specified &&
			!mur_options.lookback_time_specified && !mur_options.lookback_opers_specified)
		  || (mur_options.rollback && !mur_options.resync_specified && 0 == mur_options.fetchresync_port)))
	{ 	/* We do not need to do unnecessary processing */
		if (mur_options.show)
			mur_output_show();
		murgbl.clean_exit = TRUE;
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
		if (SS_NORMAL != mur_apply_pblk(TRUE))
			mupip_exit(ERR_MUNOACTION);
		if (!mur_jctl_from_next_gen())
			mupip_exit(ERR_MUNOACTION);
	}
	jgbl.max_resync_seqno = 0; 		/* jgbl.max_resync_seqno must be calculated before gtmrecv_fetchresync() call */
	max_lvrec_time = 0;			/* For delta format time qualifiers */
	min_bov_time = MAXUINT4;		/* For forward qualifier we can find minimum of bov_timestamps */
	for (regno = 0; regno < reg_total; regno++)
	{
		rctl = &mur_ctl[regno];
		jctl = rctl->jctl;
		assert(NULL == jctl->next_gen);
		if (mur_options.fetchresync_port && rctl->csd->resync_seqno > jgbl.max_resync_seqno)
			jgbl.max_resync_seqno = rctl->csd->resync_seqno;
		/* copy lvrec_time into region structure */
		rctl->lvrec_time = jctl->lvrec_time;
		if (rctl->lvrec_time > max_lvrec_time)
			max_lvrec_time = rctl->lvrec_time;
		if (mur_options.forward && (jnl_tm_t)rctl->jctl_head->jfh->bov_timestamp < min_bov_time)
			min_bov_time = (jnl_tm_t)rctl->jctl_head->jfh->bov_timestamp;
	}
	/* Following processing of time qualifiers cannot be done in mur_get_options() as it does not have max_lvrec_time
	 * Also this should be done after interrupted recovery processing.
	 * Otherwise delta time of previous command and delta time of this recover may not be same.
	 * All time qualifiers are specified in delta time or absolute time.
	 *	mur_options.since_time == 0 means no /SINCE time was specified
	 *	mur_options.since_time < 0 means it is the delta since time.
	 *	mur_options.since_time > 0 means it is absolute since time in seconds
	 */
	assert(!mur_options.forward || 0 == mur_options.since_time);
	assert(!mur_options.forward || 0 == mur_options.lookback_time);
	if (mur_options.since_time <= 0)
		REL2ABSTIME(mur_options.since_time, max_lvrec_time, FALSE); /* make it absolute time */
	if (!mur_options.before_time_specified)
		mur_options.before_time = MAXUINT4;
	else if (mur_options.before_time <= 0)
		REL2ABSTIME(mur_options.before_time, max_lvrec_time, TRUE); /* make it absolute time */
	if ((CLI_PRESENT == cli_present("AFTER")) && (mur_options.after_time <= 0))
		REL2ABSTIME(mur_options.after_time, max_lvrec_time, FALSE); /* make it absolute time */
	if (mur_options.lookback_time <= 0)
		REL2ABSTIME(mur_options.lookback_time, mur_options.since_time, FALSE); /* make it absolute time */
	if (!mur_options.forward && (mur_options.before_time < mur_options.since_time))
	{
		GET_TIME_STR(mur_options.before_time, time_str1);
		GET_TIME_STR(mur_options.since_time, time_str2);
		gtm_putmsg(VARLSTCNT(4) ERR_JNLTMQUAL1, 2, time_str1, time_str2);
		mupip_exit(ERR_MUNOACTION);
	}
	if (!mur_options.forward && (mur_options.lookback_time > mur_options.since_time))
	{
		GET_TIME_STR(mur_options.lookback_time, time_str1);
		GET_TIME_STR(mur_options.since_time, time_str2);
		gtm_putmsg(VARLSTCNT(4) ERR_JNLTMQUAL2, 2, time_str1, time_str2);
		mupip_exit(ERR_MUNOACTION);
	}
	if (mur_options.forward && mur_options.before_time < min_bov_time)
	{
		GET_TIME_STR(mur_options.before_time, time_str1);
		GET_TIME_STR(min_bov_time, time_str2);
		gtm_putmsg(VARLSTCNT(4) ERR_JNLTMQUAL3, 2, time_str1, time_str2);
		mupip_exit(ERR_MUNOACTION);
	}
	if (mur_options.forward && (mur_options.before_time < mur_options.after_time))
	{
		GET_TIME_STR(mur_options.before_time, time_str1);
		GET_TIME_STR(mur_options.after_time, time_str2);
		gtm_putmsg(VARLSTCNT(4) ERR_JNLTMQUAL4, 2, time_str1, time_str2);
		mupip_exit(ERR_MUNOACTION);
	}
	if (mur_options.fetchresync_port)
	{
		JNL_PUT_MSG_PROGRESS("FETCHRESYNC processing started");
		if (SS_NORMAL != gtmrecv_fetchresync(mur_options.fetchresync_port, &murgbl.resync_seqno))
			mupip_exit(ERR_MUNOACTION);
		if (jgbl.max_resync_seqno < murgbl.resync_seqno)
			murgbl.resync_seqno = jgbl.max_resync_seqno;
		murgbl.stop_rlbk_seqno = murgbl.resync_seqno;
	} else if (mur_options.resync_specified)
		murgbl.stop_rlbk_seqno = murgbl.resync_seqno;

	/* PHASE 2: Create list of broken transactions for both forward and backward recovery
	 *          In addition apply PBLK for backward recover with noverify */
	JNL_PUT_MSG_PROGRESS("Backward processing started");
	apply_pblk = (mur_options.update && !mur_options.forward && !mur_options.verify);
	if (!mur_back_process(apply_pblk))
		mupip_exit(ERR_MUNOACTION);
	if (!mur_options.rollback)
	{
		/* mur_process_token_table returns followings:
		 * 	min_broken_time = token with minimum time stamp of broken entries
		 * 	ztp_broken = TRUE, if any ztp entry is broken */
		min_broken_time = mur_process_token_table(&ztp_broken);
		losttn_seqno = MAXUINT8;
	}
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
		 *          If ZTP is present and !mur_options.verify and lookback processing changed resolve_time Then
		 *          	This will do additional PBLK undoing
                 * 	    For mur_options.verify == true following will do complete
		 *	    	PBLK processing (from lvrec_off to turn_around point)
		 */
		JNL_PUT_MSG_PROGRESS("Before image applying started");
                if (mur_options.update)
		{
			if (SS_NORMAL != mur_apply_pblk(FALSE))
				mupip_exit(ERR_MUNOACTION);

			/* PHASE 5 : Update journal file header with current state of recover, so that if this process
			 *           is interrupted, we can recover from it. We already synched updates */
			if (SS_NORMAL != mur_process_intrpt_recov())
				mupip_exit(ERR_MUNOACTION);
		}
	}

	/* PHASE 6 : Forward processing phase */
	JNL_PUT_MSG_PROGRESS("Forward processing started");
	if (mur_options.rollback)
	{
		mur_process_seqno_table(&min_broken_seqno, &losttn_seqno);
		min_broken_time = MAXUINT4;
	}
	if (SS_NORMAL != mur_forward(min_broken_time, min_broken_seqno, losttn_seqno))
		mupip_exit(ERR_MUNOACTION);
	prc_vec = murgbl.prc_vec;
	if (mur_options.show)
		mur_output_show();

	/* PHASE 7 : Close all files, rundown and exit */
	murgbl.clean_exit = TRUE;
	if (mur_options.rollback)
		gtm_putmsg(VARLSTCNT(4) ERR_RLBKJNSEQ, 2, &murgbl.consist_jnl_seqno, &murgbl.consist_jnl_seqno);
	if (murgbl.wrn_count)
		mupip_exit(ERR_JNLACTINCMPLT);
	else
		mupip_exit(SS_NORMAL);
}
