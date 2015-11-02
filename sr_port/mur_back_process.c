/****************************************************************
 *
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "min_max.h"

#include "gtm_time.h"
#include "gtm_string.h"
#include "gtm_stdio.h"
#ifdef VMS
#include <descrip.h>
#endif
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "copy.h"
#include "util.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "mur_read_file.h"
#include "iosp.h"
#include "gtmmsg.h"		/* for gtm_putmsg() prototype */
#include "dbfilop.h"		/* for dbfilop() prototype */
#include "cli.h"
#include "mupip_exit.h"
#include "mur_validate_checksum.h"
#include "gdsblk.h"

GBLREF  int		mur_regno;
GBLREF 	mur_gbls_t	murgbl;
GBLREF 	mur_rab_t	mur_rab;
GBLREF	mur_read_desc_t	mur_desc;
GBLREF	jnl_ctl_list	*mur_jctl;
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	mur_opt_struct	mur_options;
LITREF	int		jrt_update[JRT_RECTYPES];
LITREF	boolean_t	jrt_is_replicated[JRT_RECTYPES];

#ifdef DEBUG
static boolean_t	iterationcnt;
static jnl_tm_t		prev_max_lvrec_time, prev_min_bov_time;
#endif

#define SAVE_PRE_RESOLVE_SEQNO(rectype, rec_time, rec_token_seq)	\
{									\
	if (JRT_EPOCH == rectype || JRT_EOF == rectype)			\
	{								\
		if (rec_token_seq > *pre_resolve_seqno)			\
			*pre_resolve_seqno = rec_token_seq;		\
	} else								\
	{								\
		if ((rec_token_seq + 1) > *pre_resolve_seqno)		\
			*pre_resolve_seqno = rec_token_seq + 1;		\
	}								\
}

#define MUR_BACK_PROCESS_ERROR(message_string)					\
{										\
	if (mur_jctl->after_end_of_data)					\
		return ERR_JNLBADRECFMT;					\
	gtm_putmsg(VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT(message_string));	\
	if (!mur_report_error(MUR_JNLBADRECFMT))				\
		return ERR_JNLBADRECFMT;					\
	else									\
		continue;							\
}

static void save_turn_around_point(reg_ctl_list *rctl, boolean_t apply_pblk)
{
	jnl_ctl_list	*tmpjctl;

	assert(!mur_options.forward);
	assert(JRT_EPOCH == mur_rab.jnlrec->prefix.jrec_type);
	assert(NULL == rctl->jctl_turn_around);
	assert(0 == mur_jctl->turn_around_offset);
	rctl->jctl_turn_around = mur_jctl;
	mur_jctl->turn_around_offset = mur_jctl->rec_offset;
	mur_jctl->turn_around_time = mur_rab.jnlrec->prefix.time;
	mur_jctl->turn_around_seqno = mur_rab.jnlrec->jrec_epoch.jnl_seqno;
	mur_jctl->turn_around_tn = ((jrec_prefix *)mur_rab.jnlrec)->tn;
	DEBUG_ONLY(
		/* before updating, check that previous pblk stop point is later than the final turn-around-point */
		for (tmpjctl = rctl->jctl_apply_pblk; NULL != tmpjctl && tmpjctl != mur_jctl; tmpjctl = tmpjctl->prev_gen)
			;
		assert((NULL == rctl->jctl_apply_pblk)
			|| ((NULL != tmpjctl) && ((tmpjctl != rctl->jctl_apply_pblk)
						|| (tmpjctl->apply_pblk_stop_offset >= mur_jctl->turn_around_offset))));
	)
	if (apply_pblk)
	{	/* we have applied more PBLKs than is already stored in rctl->jctl_apply_pblk. update that and related fields */
		if (NULL != rctl->jctl_apply_pblk)
		{	/* this was set to non-NULL by the previous iteration of mur_back_process. clear that. */
			assert(rctl->jctl_apply_pblk->apply_pblk_stop_offset);
			rctl->jctl_apply_pblk->apply_pblk_stop_offset = 0;
		}
		rctl->jctl_apply_pblk = mur_jctl;
		mur_jctl->apply_pblk_stop_offset = mur_jctl->turn_around_offset;
	}
}

boolean_t mur_back_process(boolean_t apply_pblk, seq_num *pre_resolve_seqno)
{
	jnl_ctl_list	*jctl;
	reg_ctl_list	*rctl;
	uint4		status;
	int 		regno, reg_total;
	jnl_tm_t	alt_tp_resolve_time;

	error_def	(ERR_JNLBADRECFMT);
	error_def	(ERR_CHNGTPRSLVTM);
	error_def	(ERR_MUINFOUINT4);
	error_def	(ERR_MUINFOSTR);

	assert(!mur_options.forward || 0 == mur_options.since_time);
	assert(!mur_options.forward || 0 == mur_options.lookback_time);
	reg_total = murgbl.reg_total;
	alt_tp_resolve_time = 0;
	for ( ; ; )
	{
		*pre_resolve_seqno = 0;
		status = mur_back_processing(apply_pblk, pre_resolve_seqno, alt_tp_resolve_time);
		if (ERR_JNLBADRECFMT == status && mur_jctl->after_end_of_data)
		{
			assert(!mur_jctl->next_gen);
			PRINT_VERBOSE_TAIL_BAD(mur_options, mur_jctl);
			if (SS_NORMAL != mur_fread_eof_crash(mur_jctl, mur_jctl->jfh->end_of_data, mur_jctl->rec_offset))
				return FALSE;
		} else if (ERR_CHNGTPRSLVTM == status)
		{
			gtm_putmsg(VARLSTCNT(6) ERR_CHNGTPRSLVTM, 4, murgbl.tp_resolve_time, mur_rab.jnlrec->prefix.time,
							mur_jctl->jnl_fn_len, mur_jctl->jnl_fn);
			assert(murgbl.tp_resolve_time > mur_rab.jnlrec->prefix.time);
			alt_tp_resolve_time = mur_rab.jnlrec->prefix.time;
		} else	/* An error message must have been printed */
			break;
		reinitialize_list(murgbl.multi_list);
		reinitialize_hashtab_int8(&murgbl.token_table);
		murgbl.broken_cnt = 0;
		/* We must restart from latest generation. */
		for (regno = 0; regno < reg_total; regno++)
		{
			rctl = &mur_ctl[regno];
			jctl = rctl->jctl;
			for ( ; ;)
			{
				jctl->turn_around_offset = 0;
				jctl->turn_around_time = 0;
				jctl->turn_around_seqno = 0;
				jctl->turn_around_tn = 0;
				if (NULL == jctl->next_gen)
					break;
				jctl = jctl->next_gen;
			}
			rctl->jctl = jctl;	/* Restore latest generation before the failure */
			rctl->jctl_turn_around = NULL;
		}
	} /* end infinite for loop */
	return (SS_NORMAL == status);
}

/*	This routine performs backward processing for forward and backward recover/rollback.
 *	This creates list of tokens for broken fenced transactions.
 *	For noverify qualifier in backward recovry, it may apply PBLK calling mur_output_pblk()
 */
uint4 mur_back_processing(boolean_t apply_pblk, seq_num *pre_resolve_seqno, jnl_tm_t alt_tp_resolve_time)
{
	boolean_t			apply_pblk_this_region, resolve_seq, this_reg_resolved, first_epoch;
	enum jnl_record_type		rectype;
	enum rec_fence_type		rec_fence;
	int				regno, reg_total, partner;
	int4				rec_image_count = 0;	/* This is a dummy variable for UNIX */
	uint4				rec_pid, status, max_rec_size, val_len, max_blk_size;
	unsigned short			max_key_size;
	jnl_tm_t			max_lvrec_time, min_bov_time, rec_time;
	seq_num				rec_token_seq;
	token_num			token;
	trans_num			rec_tn;
	file_control			*fc;
	multi_struct			*multi;
	pini_list_struct		*plst;
	reg_ctl_list			*rctl, *rctl_top;
	jnl_string			*keystr;

	error_def		(ERR_JNLREADBOF);
	error_def		(ERR_NOPREVLINK);
	error_def		(ERR_DUPTOKEN);
	error_def		(ERR_RESOLVESEQNO);
	error_def		(ERR_MUJNLSTAT);
	error_def 		(ERR_ROLLBKINTERRUPT);
	error_def 		(ERR_EPOCHTNHI);
	error_def		(ERR_JNLBADRECFMT);
	error_def		(ERR_MUINFOUINT4);
	error_def		(ERR_MUINFOUINT8);
	error_def		(ERR_MUINFOSTR);
	error_def		(ERR_CHNGTPRSLVTM);
	error_def		(ERR_TEXT);

	reg_total = murgbl.reg_total;
	max_lvrec_time = 0;			/* To find maximum of all valid record's timestamp */
	min_bov_time = MAXUINT4;		/* For forward qualifier we need to find minimum of bov_timestamps */
	for (regno = 0; regno < reg_total; regno++)
	{
		rctl = &mur_ctl[regno];
		rctl->lvrec_time = mur_ctl[regno].jctl->lvrec_time;
		if (rctl->lvrec_time > max_lvrec_time)
			max_lvrec_time = rctl->lvrec_time;
		/* copy lvrec_time into region structure */
		if (mur_options.forward && (jnl_tm_t)rctl->jctl_head->jfh->bov_timestamp < min_bov_time)
			min_bov_time = (jnl_tm_t)rctl->jctl_head->jfh->bov_timestamp;
	}
	/* Time qualifier processing cannot be done in mur_get_options() as it does not have max_lvrec_time
	 * Also this should be done after interrupted recovery processing.
	 * Otherwise delta time of previous command and delta time of this recover may not be same. */
	assert(0 == iterationcnt || prev_max_lvrec_time >= max_lvrec_time);
	assert(0 == iterationcnt || prev_min_bov_time >= min_bov_time);
	mur_process_timequal(max_lvrec_time, min_bov_time);
	DEBUG_ONLY(prev_max_lvrec_time = max_lvrec_time;)
	DEBUG_ONLY(prev_min_bov_time = min_bov_time;)
	JNL_PUT_MSG_PROGRESS("Backward processing started");
	mur_tp_resolve_time(max_lvrec_time);
	if (0 != alt_tp_resolve_time && alt_tp_resolve_time < murgbl.tp_resolve_time)
		murgbl.tp_resolve_time = alt_tp_resolve_time;
	if (!mur_options.forward && mur_options.update)
	{
		/* Following code until the assignment of resolve_seq does the same for every call to "mur_back_processing".
		 * But multiple calls to this rountine is very rare (possible in case of tail corruption in journal file)
		 * So let's keep it here instead of moving to mur_back_process.
		 */
		for (rctl = mur_ctl, rctl_top = mur_ctl + reg_total; rctl < rctl_top; rctl++)
		{
			assert(NULL != rctl->csd);
			/* If we have done interrupted recovery processing (through mur_apply_pblk) already, we
			 * would have played all PBLKs until the turn-around-point of last interrupted recovery.
			 * We would not have inserted any more journal file generations as part of backward processing.
			 * Therefore we expect "rctl->jctl_head" to be equal to "rctl->jctl_apply_pblk" when we come here.
			 * There is one exception though and that is if we come here for "iterationcnt > 0". In this case,
			 * it is possible that "rctl->jctl_head" is set to a generation earlier than "rctl->jctl_apply_pblk"
			 * during the previous iteration of "mur_back_processing" which did complete for this region but
			 * later encountered a JNLRECFMT error in a different region and hence had to restart.
			 */
			assert(!rctl->jfh_recov_interrupted || rctl->jctl_head == rctl->jctl_apply_pblk || iterationcnt);
			assert(!rctl->recov_interrupted || murgbl.intrpt_recovery);
			/* assert(!rctl->jfh_recov_interrupted || rctl->recov_interrupted); ???
			 * The above assert is temporarily commented out because in mur_close_files we set
			 * csd->recov_interrupted = FALSE before we set jctl->jfh->recover_interrupted = FALSE
			 * so it can fail if recover crashes in between those two assignments. But the assert is
			 * not removed as the implications of the assert not being true have to be handled in
			 * the entire recover code before removing it.
			 */
			if (rctl->recov_interrupted && rctl->csd->intrpt_recov_resync_seqno)
			{
				if (!mur_options.rollback)
					gtm_putmsg(VARLSTCNT(4) ERR_ROLLBKINTERRUPT, 2, DB_LEN_STR(rctl->gd));
				if ((0 == murgbl.resync_seqno) ||
					(rctl->csd->intrpt_recov_resync_seqno < murgbl.resync_seqno))
					murgbl.resync_seqno = rctl->csd->intrpt_recov_resync_seqno;
			}
		}
		if (murgbl.stop_rlbk_seqno < murgbl.resync_seqno)
		{
			assert(murgbl.intrpt_recovery);
			murgbl.resync_seqno = murgbl.stop_rlbk_seqno;
		}
		if (murgbl.resync_seqno)
			gtm_putmsg(VARLSTCNT(3) ERR_RESOLVESEQNO, 1, &murgbl.resync_seqno);
		if (!murgbl.intrpt_recovery)
			resolve_seq = (mur_options.rollback && mur_options.resync_specified);
		else
			resolve_seq = (0 != murgbl.resync_seqno);
		for (rctl = mur_ctl, rctl_top = mur_ctl + reg_total; rctl < rctl_top; rctl++)
		{
			assert(rctl->csd->recov_interrupted);	/* mur_open_files set this */
			if (apply_pblk && !rctl->jfh_recov_interrupted)
			{	/* When the 'if' condition is TRUE, we apply PBLKs in mur_back_process.
				 * Store the murgbl.tp_resolve_time/murgbl.resync_seqno.
				 * So we remember to undo PBLKs at least upto that point,
				 * in case this recovery is interrupted/crashes.
				 */
				assert(0 == iterationcnt || rctl->csd->intrpt_recov_tp_resolve_time >= murgbl.tp_resolve_time);
				rctl->csd->intrpt_recov_tp_resolve_time = murgbl.tp_resolve_time;
				assert(0 == iterationcnt ||
				       rctl->csd->intrpt_recov_resync_seqno == (resolve_seq ? murgbl.resync_seqno : 0));
				rctl->csd->intrpt_recov_resync_seqno = (resolve_seq ? murgbl.resync_seqno : 0);
				/* flush the changed csd to disk */
				fc = rctl->gd->dyn.addr->file_cntl;
				fc->op = FC_WRITE;
				fc->op_buff = (sm_uc_ptr_t)rctl->csd;
				fc->op_len = (int)ROUND_UP(SIZEOF_FILE_HDR(rctl->csd), DISK_BLOCK_SIZE);
				fc->op_pos = 1;
				dbfilop(fc);
			}
		}
	} /* end else !mur_options.forward */
	DEBUG_ONLY(iterationcnt++;)
	*pre_resolve_seqno = 0;
	if (!mur_options.rollback_losttnonly)
		murgbl.db_updated = mur_options.update && !mur_options.verify;
	/* At this point we have computed murgbl.tp_resolve_time. It is the time upto which (at least)
	 * we need to do token resolution. This is for all kinds of recovery and rollback.
	 * Following for loop will do backward processing and resolve token up to this murgbl.tp_resolve_time.
	 * (For recover with lower since_time, we already set murgbl.tp_resolve_time as since_time.
	 *  For interrupted recovery we also considered previous recovery's murgbl.tp_resolve_time.)
	 * For rollback command without resync qualifier (even fetch_resync) we also resolve upto this murgbl.tp_resolve_time.
	 * For rollback with resync qualifier, we may need to resolve time less than the murgbl.tp_resolve_time */
	for (mur_regno = 0, rctl = mur_ctl, rctl_top = mur_ctl + reg_total; rctl < rctl_top; rctl++, mur_regno++)
	{
		apply_pblk_this_region = apply_pblk && !rctl->jfh_recov_interrupted;
		/* Note that for rctl->jfh_recov_interrupted we do not apply pblks in this routine */
		mur_jctl = rctl->jctl;
		assert(NULL == mur_jctl->next_gen);
		mur_jctl->rec_offset = mur_jctl->lvrec_off;
		status = mur_prev(mur_jctl->rec_offset);
		if (!mur_options.forward && FENCE_NONE != mur_options.fences)
		{ 	/* This is for the latest generation only */
			rectype = (enum jnl_record_type)mur_rab.jnlrec->prefix.jrec_type;
			if (JRT_EOF != rectype)
			{ 	/* When a region is inactive but not closed, that is, no logical updates are done for some
				 * period of time (8 second), then EPOCH is written by periodic timer. However, for some
				 * existing bug/issue periodic timers can be deferred for long period of time.
				 * So we need this check here.*/
				for ( ; ; )
				{
					if (JRT_PFIN == rectype || JRT_ALIGN == rectype || JRT_INCTN == rectype)
					{
						if (JRT_INCTN == rectype)
							MUR_INCTN_BLKS_TO_UPGRD_ADJUST(rctl, mur_rab);
						if (SS_NORMAL == (status = mur_prev(0)))
						{
							mur_jctl->rec_offset -= mur_rab.jreclen;
							assert(mur_jctl->rec_offset >= mur_desc.cur_buff->dskaddr);
							assert(JNL_HDR_LEN <= mur_jctl->rec_offset);
							rectype = (enum jnl_record_type)mur_rab.jnlrec->prefix.jrec_type;
						} else
							break;
					} else
						break;
				}
				if (SS_NORMAL == status && JRT_EPOCH != rectype &&
						mur_rab.jnlrec->prefix.time < murgbl.tp_resolve_time)
					return ERR_CHNGTPRSLVTM;
			}
		}
		if (NULL != rctl->csa)
		{
			max_key_size = rctl->gd->max_key_size;
			max_rec_size = rctl->gd->max_rec_size;
			max_blk_size = rctl->csa->hdr->blk_size;
		} else
		{
			max_key_size = MAX_KEY_SZ;
			max_rec_size = MAX_LOGI_JNL_REC_SIZE;
			max_blk_size = MAX_DB_BLK_SIZE;
		}
		rec_tn = mur_rab.jnlrec->prefix.tn;
		for (rec_token_seq = MAXUINT8, first_epoch = mur_jctl->after_end_of_data = TRUE, this_reg_resolved = FALSE;
										SS_NORMAL == status; status = mur_prev_rec())
		{
			mur_jctl->after_end_of_data = mur_jctl->after_end_of_data &&
					(mur_jctl->rec_offset >= mur_jctl->jfh->end_of_data);
			assert(0 == mur_jctl->turn_around_offset);
			rectype = (enum jnl_record_type)mur_rab.jnlrec->prefix.jrec_type;
			if (!mur_validate_checksum())
				MUR_BACK_PROCESS_ERROR("Checksum validation failed");
			if (mur_rab.jnlrec->prefix.tn != rec_tn && mur_rab.jnlrec->prefix.tn != rec_tn - 1)
			{
				rec_tn = mur_rab.jnlrec->prefix.tn;
				MUR_BACK_PROCESS_ERROR("Transaction number continuty check failed");
			}
			if (mur_options.rollback && REC_HAS_TOKEN_SEQ(rectype) && GET_JNL_SEQNO(mur_rab.jnlrec) > rec_token_seq)
			{
				rec_token_seq = GET_JNL_SEQNO(mur_rab.jnlrec);
				MUR_BACK_PROCESS_ERROR("Sequence number continuty check failed");
			}
			if (IS_SET_KILL_ZKILL(rectype))
			{
				if (IS_ZTP(rectype))
					keystr = (jnl_string *)&mur_rab.jnlrec->jrec_fkill.mumps_node;
				else
					keystr = (jnl_string *)&mur_rab.jnlrec->jrec_kill.mumps_node;
				if (keystr->length > max_key_size)
					MUR_BACK_PROCESS_ERROR("Key size check failed");
				if (0 != keystr->text[keystr->length - 1])
					MUR_BACK_PROCESS_ERROR("Key null termination check failed");
				if (IS_SET(rectype))
				{
					GET_MSTR_LEN(val_len, &keystr->text[keystr->length]);
					if (keystr->length + 1 + sizeof(rec_hdr) + val_len > max_rec_size)
						MUR_BACK_PROCESS_ERROR("Record size check failed");
				}
			} else if (JRT_PBLK == rectype)
			{
				if (mur_rab.jnlrec->jrec_pblk.bsiz > max_blk_size)
					MUR_BACK_PROCESS_ERROR("PBLK size check failed");
				if (apply_pblk_this_region)
					mur_output_pblk();
				continue;
			}
			rec_tn = mur_rab.jnlrec->prefix.tn;
			rec_time = mur_rab.jnlrec->prefix.time;
			/* In journal records token_seq field is a union of jnl_seqno and token for TP or unfenced records.
			 * It is in the same offset for all records when they are present.
			 * For ZTP jnl_seqno and token are two different fields.
			 * offset of jnl_seqno in ZTP is same as that of token_seq in TP or unfenced records.
			 * Seperate token field is only present in ZTP.
			 * For non-replication (that is, doing recover) and unfenced records token_seq field has no use.
			 * For replication (that is, doing rollback) unfenced and TP records contain jnl_seqno in token_seq field.
			 * This is used as token in the hash table.
			 * Note : ZTP is not currently supported in replication. When supported here for rollback
			 *	  we may need to use both token and sequence number fields to do token resolution and
			 *	  find holes in sequence number.
			 */
			if (REC_HAS_TOKEN_SEQ(rectype))
			{
				rec_token_seq = GET_JNL_SEQNO(mur_rab.jnlrec);
				/* this_reg_resolved is set to true first time a sequence number is seen before the
				 * murgbl.tp_resolve_time. This is necessary to find any gap in sequence numbers.
				 * Any gap will result in broken or lost transactions from the gap. */
				if (mur_options.rollback && !this_reg_resolved && rec_time < murgbl.tp_resolve_time)
				{
					SAVE_PRE_RESOLVE_SEQNO(rectype, rec_time, rec_token_seq);
					this_reg_resolved = TRUE;
				}
			} else
			{
				if (JRT_INCTN == rectype)
					MUR_INCTN_BLKS_TO_UPGRD_ADJUST(rctl, mur_rab);
				continue;
			}
			/* Resolve point is defined as the offset of the earliest journal record whose
			 *      a) timestamp >= murgbl.tp_resolve_time (if resolve_seq == FALSE)
	 		 *      b) jnl_seqno >= murgbl.resync_seqno (if resolve_seq == TRUE)
			 * Turn around point is defined as the offset of the earliest EPOCH whose
			 *      a) timestamp is less than murgbl.tp_resolve_time
			 *              (if recover OR rollback with murgbl.resync_seqno == 0)
			 *      b) jnl_seqno is < murgbl.resync_seqno (if rollback with murgbl.resync_seqno != 0)
			 * We resolve tokens rollback till Resolve Point, though Turn Around Point can be much before this.
			 * We apply PBLK till Turn Around Point.
			 */
			if (JRT_EPOCH == rectype)
			{
				if (!mur_options.forward && first_epoch && !rctl->recov_interrupted &&
					(NULL != rctl->csd) && (rec_tn > rctl->csd->trans_hist.curr_tn))
				{
					assert(FALSE);
					gtm_putmsg(VARLSTCNT(7) ERR_EPOCHTNHI, 5, mur_jctl->rec_offset,
						mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, &rec_tn, &rctl->csd->trans_hist.curr_tn);
					MUR_BACK_PROCESS_ERROR("Epoch transaction number check failed");
				}
				assert(mur_options.forward || murgbl.intrpt_recovery ||
					NULL == rctl->csd || mur_rab.jnlrec->prefix.tn <= rctl->csd->trans_hist.curr_tn);
				if (rec_time < murgbl.tp_resolve_time &&
					(!murgbl.resync_seqno || rec_token_seq <= murgbl.resync_seqno))
				{
					if (!mur_options.forward)
						save_turn_around_point(rctl, apply_pblk_this_region);
					PRINT_VERBOSE_STAT("mur_back_processing:save_turn_around_point");
					break;
				} else if (first_epoch && mur_options.verbose)
				{
					gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4,
						LEN_AND_LIT("    First Epoch Record Offset"),
						mur_jctl->rec_offset, mur_jctl->rec_offset);
					gtm_putmsg(VARLSTCNT(6) ERR_MUINFOUINT4, 4,
						LEN_AND_LIT("    First Epoch Record timestamp"), rec_time, rec_time);
				}
				first_epoch = FALSE;
				continue;
			}
			if ((FENCE_NONE == mur_options.fences || rec_time > mur_options.before_time)
				|| (rec_time < murgbl.tp_resolve_time && (!resolve_seq || rec_token_seq < murgbl.resync_seqno)))
				continue;
			if (IS_FENCED(rectype))
			{	/* Note for a ZTP if FSET/GSET is present before mur_options.before_time and
				 * GUPD/ZTCOM are present after mur_options.before_time, it is considered broken. */
				rec_fence = GET_REC_FENCE_TYPE(rectype);
				if (SS_NORMAL != (status = mur_get_pini(mur_rab.jnlrec->prefix.pini_addr, &plst)))
					MUR_BACK_PROCESS_ERROR("pini_addr is bad");
				rec_pid = plst->jpv.jpv_pid;
				VMS_ONLY(rec_image_count = plst->jpv.jpv_image_count;)
				token = (TPFENCE == rec_fence) ? rec_token_seq : ((struct_jrec_ztp_upd *)mur_rab.jnlrec)->token;
				if (IS_SET_KILL_ZKILL(rectype))	/* TUPD/UUPD/FUPD/GUPD */
				{
					if (NULL != (multi = MUR_TOKEN_LOOKUP(token, rec_pid, rec_image_count,
												rec_time, rec_fence)))
					{
						if (multi->fence != rec_fence)
						{
							assert(!mur_options.rollback);	/* jnl_seqno cannot be duplicate */
							if (!(mur_report_error(MUR_DUPTOKEN)))
								return ERR_DUPTOKEN;
							multi->partner = reg_total;	/* This is broken */
							if (rec_time < multi->time)
								multi->time = rec_time;
						} else
						{
							assert((TPFENCE != rec_fence) || multi->time == rec_time);
							if (ZTPFENCE == rec_fence && multi->time > rec_time)
								multi->time = rec_time;
						}
					} else
					{	/* This is broken */
						MUR_TOKEN_ADD(multi, token, rec_pid, rec_image_count,
								rec_time, reg_total, rec_fence, mur_regno);
					}
				} else	/* TCOM/ZTCOM */
				{
					if (NULL != (multi = MUR_TOKEN_LOOKUP(token, rec_pid, rec_image_count,
												rec_time, rec_fence)))
					{
						if (mur_regno == multi->regnum || multi->fence != rec_fence)
						{
							assert(!mur_options.rollback);	/* jnl_seqno cannot be duplicate */
							if (!mur_report_error(MUR_DUPTOKEN))
								return ERR_DUPTOKEN;
							multi->partner = reg_total;	/* It is broken */
							if (rec_time < multi->time)
								multi->time = rec_time;
						} else
						{
							multi->partner--;
							assert(0 <= multi->partner);
							assert((TPFENCE != rec_fence) || rec_time == multi->time);
							assert((ZTPFENCE != rec_fence) || rec_time >= multi->time);
							if (0 == multi->partner)
								murgbl.broken_cnt--;	/* It is resolved */
							multi->regnum = mur_regno;
						}
					} else
					{
						partner = (TPFENCE == rec_fence) ?
							((struct_jrec_tcom *)mur_rab.jnlrec)->participants :
							((struct_jrec_ztcom *)mur_rab.jnlrec)->participants;
						partner--;
						MUR_TOKEN_ADD(multi, token, rec_pid, rec_image_count,
								rec_time, partner, rec_fence, mur_regno);
					}
				}
			} else if (mur_options.rollback && IS_REPLICATED(rectype) && rec_token_seq <= murgbl.stop_rlbk_seqno)
			{	/* Process unfenced transactions. They are either lost or good.
				 * For RESYNC and FETCH_RESYNC qualifiers, all non-tp transactions
				 * 	after murgbl.stop_rlbk_seqno are considered lost or broken.
				 * So, we do not need to add them in token(seqnum) table to find gap in sequence number.
				 * For consistent rollback murgbl.stop_rlbk_seqno == MAXUINT8,
				 * 	so all records till tp_resolve_time are considered for broken/lost/good determination.
				 * For rollback, pid or image_type or time are not necessary to establish uniqueness of token.
				 * Because token (jnl_seqno) is already guaranteed to be unique for an instance
				 */
				rec_fence = GET_REC_FENCE_TYPE(rectype);
				token = (ZTPFENCE != rec_fence) ? rec_token_seq :
					((struct_jrec_ztp_upd *)mur_rab.jnlrec)->token;
				/* For rollback pid or image_type or time are not necessary to establish uniqueness of token.
				 * Because token is already guaranteed to be unique for an instance */
				if (NULL == (multi = MUR_TOKEN_LOOKUP(token, 0, 0, 0, rec_fence)))
				{	/* We reuse same token table. Most of the fields in multi_struct are unused */
					MUR_TOKEN_ADD(multi, token, 0, 0, 0, 0, rec_fence, 0);
				} else
				{
					assert(FALSE);
					if (!(mur_report_error(MUR_DUPTOKEN)))
						return ERR_DUPTOKEN;
				}
			}
		} /* end for mur_prev */
		PRINT_VERBOSE_STAT("mur_back_processing:at the end");
		assert(SS_NORMAL != status || !mur_options.rollback || this_reg_resolved);
		if (SS_NORMAL != status)
		{
			if (!mur_options.forward)
			{
				if (ERR_NOPREVLINK == status)
				{
					assert(JNL_HDR_LEN ==  mur_jctl->rec_offset);
					if ((rec_time > murgbl.tp_resolve_time) || (0 != murgbl.resync_seqno &&
							MAXUINT8 != rec_token_seq && rec_token_seq > murgbl.resync_seqno))
					{	/* We do not issue error for this boundary condition */
						gtm_putmsg(VARLSTCNT(4) ERR_NOPREVLINK, 2, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn);
						return ERR_NOPREVLINK;
					} else
					{
						mur_jctl->rec_offset = JNL_HDR_LEN + PINI_RECLEN;
						status = mur_prev(mur_jctl->rec_offset);
						if (SS_NORMAL != status)
							return status;
						rectype = (enum jnl_record_type)mur_rab.jnlrec->prefix.jrec_type;
						rec_time = mur_rab.jnlrec->prefix.time;
						rec_token_seq = GET_JNL_SEQNO(mur_rab.jnlrec);
						assert(JRT_EPOCH == rectype);
						if (mur_options.rollback && !this_reg_resolved)
						{
							SAVE_PRE_RESOLVE_SEQNO(rectype, rec_time, rec_token_seq);
							this_reg_resolved = TRUE;
						}
						save_turn_around_point(rctl, apply_pblk_this_region);
					}
				} else	/* mur_read_file should have issued messages as necessary */
					return status;
			} else if (ERR_JNLREADBOF != status)	/* mur_read_file should have issued messages */
				return status;
			/* for mur_options.forward ERR_JNLREADBOF is not error but others are */
		}
		if (!mur_options.forward && NULL == rctl->jctl_turn_around)
			GTMASSERT;
	} /* end for mur_regno */
	/* Since murgbl.tp_resolve_time is one resolve time for all regions, no implicit lookback processing
	 * to resolve transactions is necessary */
	return SS_NORMAL;
}
