/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
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


void	mupip_recover(void)
{
	boolean_t		all_gen_properly_closed, apply_pblk, ztp_broken, intrrupted_recov_processing;
	enum jnl_record_type	rectype;
	int			cur_time_len, regno, reg_total;
	jnl_tm_t		min_broken_time;
	seq_num 		losttn_seqno, min_broken_seqno;
	reg_ctl_list		*rctl;
	jnl_ctl_list		*jctl;
	error_def		(ERR_MUNOACTION);
	error_def		(ERR_BLKCNTEDITFAIL);
	error_def		(ERR_MUJNLSTAT);
	error_def		(ERR_MUJNLNOTCOMPL);
	error_def		(ERR_RLBKJNSEQ);
	error_def		(ERR_JNLACTINCMPLT);
	error_def		(ERR_MUPJNLINTERRUPT);
	error_def		(ERR_MUINFOUINT4);
	error_def		(ERR_MUINFOUINT8);

	ESTABLISH(mupip_recover_ch);
	/* PHASE 1: Process user input, open journal files, create rctl for phase 2 */
	JNL_PUT_MSG_PROGRESS("Initial processing started");
	mur_init();
	mur_get_options();
	if (!mur_open_files()) /* mur_open_files already issued error */
		mupip_exit(ERR_MUNOACTION);
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
		if (!jctl->properly_closed)
			all_gen_properly_closed = FALSE;
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
	if (all_gen_properly_closed && !murgbl.intrpt_recovery && !mur_options.forward
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
	for (regno = 0; regno < reg_total; regno++)
	{
		rctl = &mur_ctl[regno];
		jctl = rctl->jctl;
		assert(NULL == jctl->next_gen);
		if (mur_options.fetchresync_port && rctl->csd->resync_seqno > jgbl.max_resync_seqno)
			jgbl.max_resync_seqno = rctl->csd->resync_seqno;
	}
	if (mur_options.fetchresync_port)
	{
		JNL_PUT_MSG_PROGRESS("FETCHRESYNC processing started");
		if (SS_NORMAL != gtmrecv_fetchresync(mur_options.fetchresync_port, &murgbl.resync_seqno))
			mupip_exit(ERR_MUNOACTION);
		if (mur_options.verbose)
			gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("Gtmrecv_fetchresync returned resync_seqno"),
				&murgbl.resync_seqno, &murgbl.resync_seqno);

		if (jgbl.max_resync_seqno < murgbl.resync_seqno)
		{
			murgbl.resync_seqno = jgbl.max_resync_seqno;
			if (mur_options.verbose)
				gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT8, 4,
					LEN_AND_LIT("Resync_seqno is reset to max_resync_seqno"),
					&murgbl.resync_seqno, &murgbl.resync_seqno);
		}
		murgbl.stop_rlbk_seqno = murgbl.resync_seqno;
	} else if (mur_options.resync_specified)
		murgbl.stop_rlbk_seqno = murgbl.resync_seqno;

	/* PHASE 2: Create list of broken transactions for both forward and backward recovery
	 *          In addition apply PBLK for backward recover with noverify */
	apply_pblk = (mur_options.update && !mur_options.forward && !mur_options.verify);
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
		if (mur_options.verbose)
		{
			gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("mur_process_seqno_table returns min_broken_seqno"),
				&min_broken_seqno, &min_broken_seqno);
			gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("mur_process_seqno_table returns losttn_seqno"),
				&losttn_seqno, &losttn_seqno);
		}
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
