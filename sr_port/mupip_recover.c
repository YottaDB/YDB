/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdio.h>
#include "gtm_stdlib.h"
#include <unistd.h>
#ifdef VMS
#include <chfdef.h>
#endif

#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
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

#define USER_STACK_SIZE	16384	/* (16 * 1024) */

#ifdef UNIX
#define INIT_PID	1
#endif

GBLDEF	boolean_t	brktrans;
GBLDEF	boolean_t	losttrans;
GBLDEF	mur_opt_struct	mur_options;
GBLDEF	bool		mur_error_allowed;
GBLDEF	int4		mur_error_count;
GBLDEF	int4		mur_wrn_count;
GBLDEF	int4		n_regions;
GBLDEF	seq_num		stop_rlbk_seqno;
GBLDEF	seq_num		max_reg_seqno;
GBLDEF  seq_num		resync_jnl_seqno;
GBLDEF  seq_num		min_epoch_jnl_seqno;
GBLDEF	seq_num		max_epoch_jnl_seqno;
GBLDEF	broken_struct	*broken_array;
GBLREF	void		(*call_on_signal)();
GBLDEF	char		*log_rollback = NULL;
GBLDEF	ctl_list	*jnl_files;
GBLDEF	jnl_proc_time	min_jnl_rec_time;

GBLREF	seq_num		consist_jnl_seqno;
GBLREF  seq_num		max_resync_seqno;
GBLREF	int4		gv_keysize;
GBLREF	gv_key		*gv_currkey, *gv_altkey;
GBLREF	gv_namehead	*gv_target;
GBLREF	gd_region	*gv_cur_region;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgmnt_data_ptr_t cs_data;
GBLREF	mv_stent	*mv_chain;
GBLREF	stack_frame	*frame_pointer;
GBLREF	unsigned char	*msp, *stackbase, *stacktop, *stackwarn;
GBLREF	bool		is_standalone;
GBLREF	seq_num		seq_num_zero;
GBLREF	seq_num		seq_num_minus_one;
GBLREF  gd_addr         *gd_header;
GBLREF	gd_binding      *gd_map;
GBLREF	gd_binding      *gd_map_top;
GBLREF  int             participants;
GBLREF	boolean_t	created_core;
GBLREF	boolean_t	need_core;
GBLREF	boolean_t	dont_want_core;
#ifdef VMS
GBLREF	struct chf$signal_array	*tp_restart_fail_sig;
GBLREF	boolean_t	tp_restart_fail_sig_used;
#endif
GBLREF	boolean_t	recovery_success;
GBLREF	uint4		cur_logirec_short_time;
GBLREF	boolean_t	forw_phase_recovery;

error_def(ERR_ASSERT);
error_def(ERR_EPOCHLIMGT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMCHECK);
error_def(ERR_JNLREADEOF);
error_def(ERR_NORECOVERWRN);
error_def(ERR_NORECOVERERR);
error_def(ERR_STACKOFLOW);
error_def(ERR_TRNARNDTNHI);
error_def(ERR_ENDRECOVERY);
error_def(ERR_BLKCNTEDITFAIL);

void		gtm_ret_code();

#define	ROLLBACK_LOG(SEQNO, TEXT)									\
{													\
	if (log_rollback)										\
	{												\
		ptr = i2ascl(qwstring, SEQNO);								\
		ptr1 = i2asclx(qwstring1, SEQNO);							\
		util_out_print(TEXT, TRUE, ptr - qwstring, qwstring, ptr1 - qwstring1, qwstring1);	\
	}												\
}

#define	ROLLBACK_LOG_STOP_JNL_SEQNO												\
if (log_rollback)														\
{																\
	ptr = i2ascl(qwstring, rec->val.jrec_epoch.jnl_seqno);									\
	ptr1 = i2asclx(qwstring1, rec->val.jrec_epoch.jnl_seqno);								\
	util_out_print("MUR-I-DEBUG : Journal !AD : FIRST Stop Jnl Seqno = !AD [0x!AD] : Consist_Stop_Addr = 0x!XL", TRUE,	\
		ctl->jnl_fn_len, ctl->jnl_fn, ptr - qwstring, qwstring, ptr1 - qwstring1, qwstring1, ctl->consist_stop_addr);	\
}

CONDITION_HANDLER(mupip_recover_ch)
{
	error_def(ERR_TPRETRY);

	START_CH;
	PRN_ERROR;	/* Flush out the error message that is driving us */
	if ((int)ERR_TPRETRY == SIGNAL)
	{
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
		mur_close_files();
		NEXTCH; /* Will do prn_error for us */
	} else
	{
		assert(SEVERITY == WARNING || SEVERITY == INFO);
		CONTINUE;
	}
}

/* This routine handles errors reading the journal files.  It's also used by
 * mur_open_files(), mur_sort_files(), and mur_get_pini_jpv().
 */

void	mur_jnl_read_error(ctl_list *ctl, uint4 status, bool ok)
{
	util_out_print("Error reading record from journal file !AD - status:", TRUE, ctl->jnl_fn_len, ctl->jnl_fn);
	mur_output_status(status);
	if (!ok  ||  ++mur_error_count > mur_options.error_limit  &&  (!mur_options.interactive  ||  !mur_interactive()))
	{
		mur_close_files();
		mupip_exit(ERR_NORECOVERERR);
	}
	ctl->bypass = TRUE;	/* Prevent further use of this journal file */
}

/* This routine processes the MUPIP JOURNAL command.  */

void	mupip_recover(void)
{
	bool			first_time;
	char			fn[MAX_FN_LEN];
	unsigned char		*ptr, qwstring[100], *ptr1, qwstring1[100];
	ctl_list		*ctl, *last_jnl_file;
	enum jnl_record_type	rectype;
	jnl_file_header		*header;
	jnl_proc_time		min_lookback_time;
	jnl_record		*rec;
	jrec_suffix		*suffix_ptr;
	mval			v;
	seq_num			tempqw_seqno;	/* Used for temporary manipulation */
	uint4			i, status, tempdw, lookup_lookback_time;
	unsigned char		*mstack_ptr;
	int			epoch_limit;

	error_def(ERR_ROLLBKIMPOS);
	error_def(ERR_MURCLOSEWRN);

	ESTABLISH(mupip_recover_ch);
	call_on_signal = mur_close_files;
	assert(0 == JREC_SUFFIX_SIZE % JNL_REC_START_BNDRY);
	log_rollback = GETENV("LOG_ROLLBACK");
	is_standalone = TRUE;
	epoch_limit = 0;
	mur_get_options();
	jnl_files = mur_get_jnl_files();
	if (NULL == jnl_files)
	{
		util_out_print("", TRUE);
		rts_error(VARLSTCNT(1) ERR_ROLLBKIMPOS);
	}
	QWASSIGN(consist_jnl_seqno, seq_num_zero);
	QWASSIGN(max_resync_seqno, seq_num_zero);
	if (!mur_open_files(&jnl_files))
	{
		mur_close_files();
		mupip_exit(ERR_NORECOVERERR);
	}
	if (mur_options.rollback)
	{
		if (mur_options.fetchresync)
		{
			gtmrecv_fetchresync(jnl_files, mur_options.fetchresync, &resync_jnl_seqno);
			ROLLBACK_LOG(resync_jnl_seqno, "MUR-I-DEBUG : ResyncJnlSeqno = !AD [0x!AD]");
		} else if (CLI_PRESENT == cli_present("RESYNC"))
		{
			/* mur_get_options() fills in resync_jnl_seqno from the command line */
			mur_options.fetchresync = TRUE;
			QWASSIGN(consist_jnl_seqno, resync_jnl_seqno);
			ROLLBACK_LOG(resync_jnl_seqno, "MUR-I-DEBUG : ResyncJnlSeqno = !AD [0x!AD]");
		} else
			QWASSIGN(consist_jnl_seqno, seq_num_minus_one);
	}
	if (mur_options.rollback && mur_options.fetchresync && CLI_PRESENT != cli_present("RESYNC"))
	{
		/* Take the minimum of our resync_seqno and the other
		 * system's resync_seqno */
		if (QWGT(resync_jnl_seqno, max_resync_seqno))
			QWASSIGN(resync_jnl_seqno, max_resync_seqno);
		QWASSIGN(consist_jnl_seqno, resync_jnl_seqno);
	}
	if (!mur_options.forward && mur_options.update && ((!mur_options.rollback && mur_options.chain) || mur_options.fetchresync))
	{
		/* Check to see whether we have all the journal files that satisfy consist_jnl_seqno
			criteria in case of fetchresync or resync */
		if (!mur_check_jnlfiles_present(&jnl_files))
		{
			mur_close_files();
			mupip_exit(ERR_NORECOVERERR);
		}
	}
	if (mur_options.interactive)
		util_in_open(NULL);
	for (last_jnl_file = jnl_files;  NULL != last_jnl_file->next;  last_jnl_file = last_jnl_file->next)
		;
	mur_multi_initialize();		/* do initialization for multi-region TS/TC/ZTS/ZTC completeness check */
	mur_current_initialize();	/* do buddy_list initialization for mur_current routines */
	/* Do backward processing, if applicable */
	if (!mur_options.forward || mur_options.verify || FENCE_NONE != mur_options.fences || mur_options.show & ~SHOW_BROKEN)
	{
		if (!mur_options.forward && mur_options.update)
		{
			if (mur_options.rollback)
			{
				QWASSIGN(min_epoch_jnl_seqno, seq_num_minus_one);
				QWASSIGN(max_epoch_jnl_seqno, seq_num_zero);
			}
			for (ctl = last_jnl_file;
				NULL != ctl  &&  (mur_error_count <= mur_options.error_limit  ||  mur_error_allowed);
					ctl = ctl->prev)
			{
				ctl->consist_stop_addr = 0;
				for (status = mur_get_last(ctl->rab); SS_NORMAL == status; status = mur_previous(ctl->rab, 0))
				{
					rec = (jnl_record *)ctl->rab->recbuff;
					rectype = REF_CHAR(&rec->jrec_type);
					if (JRT_EPOCH == rectype  ||  JRT_TCOM == rectype  ||  JRT_KILL == rectype
						||  JRT_ZKILL == rectype ||  JRT_SET == rectype  ||  JRT_NULL == rectype)
					{
						if (mur_options.rollback)
						{
							assert(QWEQ(rec->val.jrec_epoch.jnl_seqno, rec->val.jrec_tcom.jnl_seqno));
							assert(QWEQ(rec->val.jrec_epoch.jnl_seqno, rec->val.jrec_kill.jnl_seqno));
							assert(QWEQ(rec->val.jrec_epoch.jnl_seqno, rec->val.jrec_zkill.jnl_seqno));
							assert(QWEQ(rec->val.jrec_epoch.jnl_seqno, rec->val.jrec_set.jnl_seqno));
							assert(QWEQ(rec->val.jrec_epoch.jnl_seqno, rec->val.jrec_null.jnl_seqno));
							if (QWLT(max_epoch_jnl_seqno, rec->val.jrec_epoch.jnl_seqno))
								QWASSIGN(max_epoch_jnl_seqno, rec->val.jrec_epoch.jnl_seqno);
							if (!mur_options.fetchresync  ||
								QWLE(rec->val.jrec_epoch.jnl_seqno, resync_jnl_seqno))
							{
								if (ctl->rab->pvt->jfh->crash
									&& QWGT(min_epoch_jnl_seqno, rec->val.jrec_epoch.jnl_seqno))
								       QWASSIGN(min_epoch_jnl_seqno, rec->val.jrec_epoch.jnl_seqno);
								ctl->consist_stop_addr = ctl->rab->dskaddr;
								ROLLBACK_LOG_STOP_JNL_SEQNO;
								break;
							}
						}
					}
				}
				if (mur_options.rollback)
				{
					if (0 == ctl->consist_stop_addr)
					{
						if (!mur_insert_prev(ctl, &jnl_files))
						{
							mur_close_files();
							mupip_exit(ERR_NORECOVERERR);
						}
						continue;
					}
					if (!mur_options.fetchresync)
					{
						if (0 != epoch_limit)
						{
							gtm_putmsg(VARLSTCNT(7) ERR_EPOCHLIMGT, 5, ctl->jnl_fn_len, ctl->jnl_fn,
							epoch_limit, mur_options.epoch_limit, epoch_limit);
							mur_close_files();
							mupip_exit(ERR_NORECOVERERR);
						}
					}
				} else
				{	/* Include previous generation journal files only when lookback/since time
					 * is specified to be less than any default time and if mur_options.chain is TRUE.
					 */
					if (CMP_JNL_PROC_TIME(ctl->rab->pvt->jfh->bov_timestamp, min_jnl_rec_time) >=0)
					{	/* Only one generation and not many updates */
     						if (!mur_options.chain
							|| ((0 == ctl->rab->pvt->jfh->prev_jnl_file_name_length)
								&& !mur_options.since && !mur_options.lookback_time_specified))
							continue;
						if (!mur_insert_prev(ctl, &jnl_files))
						{
							mur_close_files();
							mupip_exit(ERR_NORECOVERERR);
						}
					}
					continue;
				}
				if (SS_NORMAL != status)
				{
					mur_jnl_read_error(ctl, status, TRUE);
					continue;
				}
			}
			if (mur_options.rollback)
			{
				if (QWEQ(seq_num_minus_one, min_epoch_jnl_seqno)  &&  QWGT(consist_jnl_seqno, max_epoch_jnl_seqno))
					QWASSIGN(consist_jnl_seqno, max_epoch_jnl_seqno);
				else if ((QWNE(seq_num_minus_one, min_epoch_jnl_seqno)) &&
						(QWGT(consist_jnl_seqno, min_epoch_jnl_seqno)))
					QWASSIGN(consist_jnl_seqno, min_epoch_jnl_seqno);
				assert(QWNE(seq_num_zero, max_epoch_jnl_seqno));
				if (QWNE(seq_num_minus_one, min_epoch_jnl_seqno))
				{
					QWINCRBYDW(max_epoch_jnl_seqno, (n_regions * EPOCH_SIZE + 2));
					QWSUB(tempqw_seqno, max_epoch_jnl_seqno, consist_jnl_seqno);
					DWASSIGNQW(tempdw, tempqw_seqno);
					broken_array = (broken_struct *)malloc(tempdw * sizeof(broken_struct));
					memset(broken_array, 0, tempdw * sizeof(broken_struct));
				}
			}
		}
		if (!mur_options.forward && mur_options.update && 0 != mur_options.epoch_limit)
		{
			for (ctl = last_jnl_file; NULL != ctl;  ctl = ctl->prev)
			{
				for (status = mur_get_last(ctl->rab); SS_NORMAL == status;  status = mur_previous(ctl->rab, 0))
				{
					rec = (jnl_record *)ctl->rab->recbuff;
					rectype = REF_CHAR(&rec->jrec_type);
					if (JRT_EPOCH == rectype)
					{
						epoch_limit++;
						if (mur_options.rollback && QWLE(rec->val.jrec_epoch.jnl_seqno, consist_jnl_seqno))
							break;
						else if (!mur_options.rollback
							   && rec->val.jrec_epoch.short_time < MUR_OPT_MID_TIME(lookback_time))
							break;
					}
				}
				if (0 == ctl->consist_stop_addr && mur_options.rollback)
					continue;
				if (epoch_limit > mur_options.epoch_limit)
				{
					gtm_putmsg(VARLSTCNT(7) ERR_EPOCHLIMGT, 5, ctl->jnl_fn_len, ctl->jnl_fn,
									epoch_limit, mur_options.epoch_limit, epoch_limit);
					mur_close_files();
					mupip_exit(ERR_NORECOVERERR);
				}
				epoch_limit = 0;
			}
		}
		for (ctl = last_jnl_file;
			NULL != ctl  &&  (mur_error_count <= mur_options.error_limit  ||  mur_error_allowed);
				ctl = ctl->prev)
		{
			ctl->consist_stop_addr = 0;
			ctl->lookback_count = -1;       /* Initialize for mur_back_process() */
			for (status = mur_get_last(ctl->rab), first_time = TRUE;
				(SS_NORMAL == status) && ((mur_error_count <= mur_options.error_limit) || mur_error_allowed);
					status = mur_previous(ctl->rab, 0), first_time = FALSE)
				if (!mur_back_process(ctl))
					break;
			if (mur_options.update  &&  !mur_options.forward  &&  !mur_options.verify)
			{
				cs_addrs = (sgmnt_addrs *)&FILE_INFO(ctl->gd)->s_addrs;
				cs_data = cs_addrs->hdr;
				cs_data->trans_hist.header_open_tn =
				cs_data->trans_hist.early_tn =
				cs_data->trans_hist.curr_tn = ctl->turn_around_tn;
				if (dba_bg == cs_data->acc_meth)
					bt_refresh(cs_addrs);
				if (ctl->turn_around_tn > ctl->db_tn)
					rts_error(VARLSTCNT(6) ERR_TRNARNDTNHI, 4, ctl->jnl_fn_len,
						ctl->jnl_fn, ctl->turn_around_tn, ctl->db_tn);
			}
			if (SS_NORMAL != status  &&  (first_time  ||  ERR_JNLREADEOF != status))
			{
				mur_jnl_read_error(ctl, status, TRUE);
				continue;
			} else if (mur_options.rollback  &&  !mur_options.forward  &&  ERR_JNLREADEOF == status)
			{
				assert(0 == ctl->consist_stop_addr  ||  FALSE == ctl->concat_prev);
				ctl->consist_stop_addr = 0;
				if (FALSE == ctl->concat_prev)
				{
					if (!mur_insert_prev(ctl, &jnl_files))
					{
						mur_close_files();
						mupip_exit(ERR_NORECOVERERR);
					}
				}
			}
			if (mur_error_count > mur_options.error_limit  &&  !mur_error_allowed)
				break;
			if (!mur_options.rollback  &&  ctl->broken_entries > 0)
			{
				if (!mur_report_error(ctl, MUR_INSUFLOOK))
					break;
				if (mur_options.show & SHOW_BROKEN)
					/* These broken transactions will be recovered */
					mur_include_broken(ctl);
			}
			ctl->stop_addr = ctl->rab->dskaddr;
			mur_empty_current(ctl);
			if (!ctl->concat_prev  &&  mur_options.update &&  !mur_options.forward  &&  !mur_options.verify)
			{
				mur_master_map(ctl);
				cs_addrs = (sgmnt_addrs *)&FILE_INFO(ctl->gd)->s_addrs;
				cs_data = cs_addrs->hdr;
				cs_data->trans_hist.free_blocks = mur_blocks_free(ctl);
			}
		}
		if ((!mur_options.rollback) && (n_regions < participants) &&
			mur_options.update && (FENCE_NONE != mur_options.fences))
		{
			mur_report_error(NULL, MUR_MISSING_FILES);
			mur_close_files();
			mupip_exit(ERR_NORECOVERWRN);
		}
		if (mur_options.rollback)
		{
			QWASSIGN(stop_rlbk_seqno, rlbk_lookup_seqno());
			ROLLBACK_LOG(stop_rlbk_seqno, "MUR-I-DEBUG : BrokenJnlSeqno = !AD [0x!AD]");
			if (QWNE(seq_num_minus_one, min_epoch_jnl_seqno))
			{
				QWSUB(tempqw_seqno, max_epoch_jnl_seqno, consist_jnl_seqno);
				DWASSIGNQW(tempdw, tempqw_seqno);
				for (i = 0; i < tempdw; i++)
					if (-1 != broken_array[i].count)
						break;
				QWADDDW(tempqw_seqno, consist_jnl_seqno, i);
				if (QWGT(stop_rlbk_seqno, tempqw_seqno))
					QWASSIGN(stop_rlbk_seqno, tempqw_seqno);
			}
			ROLLBACK_LOG(stop_rlbk_seqno, "MUR-I-DEBUG : BrokenJnlSeqno = !AD [0x!AD]");
			if (!mur_options.fetchresync  &&  QWNE(seq_num_minus_one, stop_rlbk_seqno))
				assert(QWLE(consist_jnl_seqno, stop_rlbk_seqno));
			else if (mur_options.fetchresync)
				assert(QWLE(consist_jnl_seqno, stop_rlbk_seqno) || QWLE(consist_jnl_seqno, resync_jnl_seqno));
			ROLLBACK_LOG(consist_jnl_seqno, "MUR-I-DEBUG : ConsistJnlSeqno = !AD [0x!AD]");
		}
		/* assert(!mur_multi_extant()  ||  !mur_options.rollback); */
		if (mur_multi_extant()  &&  !mur_options.rollback)
		{
			lookup_lookback_time = mur_lookup_lookback_time();
			JNL_WHOLE_FROM_SHORT_TIME(min_lookback_time, lookup_lookback_time);
			if (lookup_lookback_time < MUR_OPT_MID_TIME(lookback_time))
			{
				mur_options.lookback_time = min_lookback_time;
				for (ctl = last_jnl_file; NULL != ctl; ctl = ctl->prev)
					if (MID_TIME(min_lookback_time) < MID_TIME(ctl->lookback_time))
						ctl->reached_lookback_limit = FALSE;
			}
			for (ctl = last_jnl_file;
				NULL != ctl  &&  (mur_error_count <= mur_options.error_limit  ||  mur_error_allowed);
					ctl = ctl->prev)
			{
				if (ctl->bypass  ||  ctl->reached_lookback_limit)
					continue;
				while ((SS_NORMAL == (status = mur_previous(ctl->rab, 0))) && mur_lookback_process(ctl))
					if (!mur_multi_extant())
						goto check_pblk;	/* I know, I know ... but it's exactly what to do */
				if (SS_NORMAL != status  &&  ERR_JNLREADEOF != status)
					mur_jnl_read_error(ctl, status, TRUE);
			}
		}
		if (mur_options.rollback)
			util_out_print("MUR-I-PBLKSTART : Starting Phase for Applying PBLK records", TRUE);
		/* If application of PBLK records was deferred above [see mur_back_process()], apply them now */
check_pblk:
		if (mur_options.update  &&  !mur_options.forward  &&  mur_options.verify)
		{
			for (ctl = last_jnl_file;
				NULL != ctl  &&  (mur_error_count <= mur_options.error_limit  ||  mur_error_allowed);
					ctl = ctl->prev)
			{
				if (ctl->bypass || mur_options.rollback && ctl->concat_next && 0 != ctl->next->consist_stop_addr)
					continue;
				for (status = mur_get_last(ctl->rab), first_time = TRUE;
					(SS_NORMAL == status) && (ctl->rab->dskaddr != ctl->stop_addr);
						status = mur_previous(ctl->rab, 0), first_time = FALSE)
				{
					if (mur_options.rollback
						&&  JRT_EPOCH == REF_CHAR(&((jnl_record *)ctl->rab->recbuff)->jrec_type)
						&&  QWGE(consist_jnl_seqno,
							((jnl_record *)ctl->rab->recbuff)->val.jrec_epoch.jnl_seqno))
					{
						ctl->consist_stop_addr = ctl->stop_addr = ctl->rab->dskaddr;
						break;
					}
					if (JRT_PBLK == REF_CHAR(&((jnl_record *)ctl->rab->recbuff)->jrec_type))
						mur_output_record(ctl);
				}
				if (SS_NORMAL != status  &&  (first_time  ||  ERR_JNLREADEOF != status))
				{
					mur_jnl_read_error(ctl, status, TRUE);
					continue;
				}
				cs_addrs = (sgmnt_addrs *)&FILE_INFO(ctl->gd)->s_addrs;
				cs_data = cs_addrs->hdr;
				if (!ctl->concat_prev)
				{
					/* Its ok to set cs_addrs explicitly instead of a tp_change_reg()
					 * because there will be no actual updates to the database going on
					 * and hence nobody relying on cs_addrs.
					 */
					mur_master_map(ctl);
					cs_data->trans_hist.free_blocks = mur_blocks_free(ctl);
				}
				cs_data->trans_hist.header_open_tn =
				cs_data->trans_hist.early_tn =
				cs_data->trans_hist.curr_tn = ctl->turn_around_tn;
				if (dba_bg == cs_data->acc_meth)
					bt_refresh(cs_addrs);
				if (ctl->turn_around_tn > ctl->db_tn)
					rts_error(VARLSTCNT(6) ERR_TRNARNDTNHI, 4, ctl->jnl_fn_len,
						ctl->jnl_fn, ctl->turn_around_tn, ctl->db_tn);
			}
		}
	}
	if (NULL != mur_options.losttrans_file_info)
		util_out_print("MUR-I-LOSTTRANSSTART : Starting Phase for Extracting Lost Transactions", TRUE);
	/* Do forward processing */
	forw_phase_recovery = TRUE;	/* Initialize to TRUE so that recover copies original time stamps during forward phase
 	 				 * while writing newly generated logical records */
	/* Note that if all regions have before-imaging disabled, we dont need to invoke mur_crejnl_forwphase_file()
	 * but we let it decide that since anyway it scans the ctl list
	 */
	if (mur_options.update && !mur_crejnl_forwphase_file(&jnl_files))
	{
		mur_close_files();
		mupip_exit(ERR_NORECOVERERR);
	}
	mur_forward_buddy_list_init();
        if (((mur_error_count <= mur_options.error_limit  ||  mur_error_allowed)  &&
		(mur_options.update  ||  NULL != mur_options.extr_file_info)  ||
		(mur_options.rollback) ||
		(FENCE_NONE == mur_options.fences)))
	{
		if (mur_options.update)
		{
			if (!gd_header)
			{
				v.mvtype = MV_STR;
				v.str.len = 0;
				gd_header = zgbldir(&v);
				gd_map = gd_header->maps;
				gd_map_top = gd_map + gd_header->n_maps;
			}
			gv_currkey = (gv_key *)malloc(sizeof(gv_key) - 1 + gv_keysize);
			gv_altkey = (gv_key *)malloc(sizeof(gv_key) - 1 + gv_keysize);
			gv_currkey->top = gv_altkey->top = gv_keysize;
			gv_currkey->end = gv_currkey->prev = gv_altkey->end = gv_altkey->prev = 0;
			gv_altkey->base[0] = gv_currkey->base[0] = '\0';
			gv_target = targ_alloc(gv_keysize);
			/* There may be M transactions in the journal files.  If so, op_tstart() and op_tcommit()
			   will be called during recovery;  they require a couple of dummy stack frames to be set up */
			mstack_ptr = (unsigned char *)malloc(USER_STACK_SIZE);
			msp = stackbase
			    = mstack_ptr + USER_STACK_SIZE - 4;
			mv_chain = (mv_stent *)msp;
			stacktop = mstack_ptr + 2 * mvs_size[MVST_NTAB];
			stackwarn = stacktop + 1024;
			msp -= sizeof(stack_frame);
			frame_pointer = (stack_frame *)msp;
			memset(frame_pointer, 0, sizeof(stack_frame));
			frame_pointer->type = SFT_COUNT;
			--frame_pointer;
			memset(frame_pointer, 0, sizeof(stack_frame));
			frame_pointer->type = SFT_COUNT;
			frame_pointer->old_frame_pointer = (stack_frame *)msp;
			msp = (unsigned char *)frame_pointer;
		}
		for (ctl = jnl_files;
			NULL != ctl  &&  (mur_error_count <= mur_options.error_limit  ||  mur_error_allowed);
				ctl = ctl->next)
		{
			if (ctl->bypass)
				continue;
			/* Initialize cur_logirec_short_time to the time of the turn around epoch record.
			 * Ideally we should never be writing an EPOCH record without writing a logical record (which
			 * 	would update cur_logirec_short_time appropriately) but for clean flow, we do the initialization.
			 */
			cur_logirec_short_time = ctl->turn_around_epoch_time;
			if (mur_options.update)
			{
				brktrans = FALSE;
				losttrans = FALSE;
				gv_target->gd_reg = gv_cur_region = ctl->gd;
				gv_target->clue.prev = gv_target->clue.end = 0;
				gv_target->root = 0;
				gv_target->nct = 0;
				gv_target->act = 0;
				gv_target->ver = 0;
				gv_target->collseq = NULL;
				gv_target->noisolation = FALSE;
				tp_change_reg();
				assert(NULL == cs_addrs->dir_tree || cs_addrs->dir_tree->gd_reg == gv_cur_region);
				if (NULL == cs_addrs->dir_tree)
				{
					cs_addrs->dir_tree = targ_alloc(gv_keysize);
					cs_addrs->dir_tree->root = DIR_ROOT;
					cs_addrs->dir_tree->gd_reg = gv_cur_region;
				}
			}
			/* Check that if ctl's region has before-imaging, then stop_addr is > 0 and the converse.
			 * Note that the above is true irrespective of whether we are doing forward or backward recovery.
			 * For forward recovery, stop_addr is guaranteed to be non-zero by mur_crejnl_forwphase_file().
			 * 	In case db's jnl-file-name is same as the first ctl->jnl_fn, stop_addr will point to
			 * 		the offset of the first valid jnl-record in the forw-phase file.
			 * 	In the other case, stop_addr will be HDR_LEN.
			 * 	Note that for forward recovery, stop_addr will be non-zero only if the jnl-file has
			 * 		before-imaged journal records and journalling is enabled.
			 * For backward recovery, stop_addr can be non-zero even if ctl->before_image is FALSE in the
			 * 	case that before-imaging is disabled before recovery.
			 */
			assert(!mur_options.update  && (!ctl->before_image || (0 != ctl->stop_addr))
			    || !mur_options.forward && (0 != ctl->stop_addr)
			    ||  mur_options.forward && (0 != ctl->stop_addr || !ctl->before_image));
			if (0 == ctl->stop_addr)
				status = mur_get_first(ctl->rab);
			else
				status = mur_next(ctl->rab, ctl->stop_addr);
			if (SS_NORMAL != status  ||  SS_NORMAL != (status = mur_forward(ctl))  &&  ERR_JNLREADEOF != status)
				mur_jnl_read_error(ctl, status, TRUE);
			if (FALSE == ctl->concat_next && mur_options.update && SS_NORMAL != mur_block_count_correct())
				gtm_putmsg(VARLSTCNT(4) ERR_BLKCNTEDITFAIL, 2, DB_LEN_STR(gv_cur_region));
		}
	}
	/* Output any reports requested by SHOW options */
	if (mur_options.show  &&  (mur_error_count <= mur_options.error_limit  ||  mur_error_allowed))
		for (ctl = jnl_files;  NULL != ctl;  ctl = ctl->next)
			mur_output_show(ctl);
	/* Moved out of mur_close_files() as it can be called as clean_up routine incase of abnormal mupip_recover termination */
	if (mur_options.update)
	{
		for (ctl = jnl_files;  NULL != ctl;  ctl = ctl->next)
			send_msg(VARLSTCNT(6) ERR_ENDRECOVERY, 4, DB_LEN_STR(ctl->gd), ctl->jnl_fn_len, ctl->jnl_fn);
	}
	/* Done */
	assert(0 <= mur_error_count);	/* never gets decremented anywhere, better be positive */
	if (mur_options.update)
		recovery_success = TRUE;	/* Set it to true so that jnl_file_close will delete the forw_phase_files */
	mur_close_files();
	if (mur_wrn_count > 0)
		mupip_exit(ERR_MURCLOSEWRN);
	if (mur_error_count > 0)
	{
		if (mur_error_count <= mur_options.error_limit  ||  mur_error_allowed)
			mupip_exit(ERR_NORECOVERWRN);
		if (!mur_options.interactive)
			util_out_print("!/Exceeded maximum number of errors allowed (!UL)", TRUE, mur_options.error_limit);
		mupip_exit(ERR_NORECOVERERR);
	}
	if (NULL != mur_options.extr_file_info  ||  mur_options.update)
	{
		if (NULL != mur_options.extr_file_info)
			util_out_print("!/Extraction successful", TRUE);
		if (NULL != mur_options.extr_file_info && mur_multi_extant())
                        mur_report_error(NULL, MUR_MISSING_EXTRACT);
		if (mur_options.update)
			util_out_print("!/Update successful", TRUE);
	} else if (mur_options.verify)
		util_out_print("!/Verification successful", TRUE);
	if (NULL != mur_options.losttrans_file_info)
		util_out_print("!/Lost transaction Extraction successful", TRUE);
	if (mur_options.rollback)
	{
		ptr = i2ascl(qwstring, consist_jnl_seqno);
		ptr1 = i2asclx(qwstring1, consist_jnl_seqno);
		util_out_print("!/Rollback journal seqno is !AD [0x!AD]", TRUE, ptr - qwstring, qwstring, ptr1 - qwstring1,
			qwstring1);
	}
	mupip_exit(SS_NORMAL);
}
