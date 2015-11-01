/****************************************************************
 *
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "min_max.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "copy.h"
#include "util.h"
#include "hashdef.h"
#include "buddy_list.h"
#include "muprec.h"
#include "iosp.h"
#include "gtmmsg.h"	/* for gtm_putmsg() prototype */
#include "dbfilop.h"	/* for dbfilop() prototype */

GBLREF 	jnl_gbls_t	jgbl;
GBLREF  int		mur_regno;
GBLREF 	mur_gbls_t	murgbl;
GBLREF 	mur_rab_t	mur_rab;
GBLREF	jnl_ctl_list	*mur_jctl;
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	mur_opt_struct	mur_options;
LITREF	int		jrt_update[JRT_RECTYPES];
LITREF	boolean_t	jrt_is_replicated[JRT_RECTYPES];


static uint4 save_turn_around_point(reg_ctl_list *rctl)
{
	error_def(ERR_TRNARNDTNHI);
	assert(NULL == rctl->jctl_turn_around);
	assert(0 == mur_jctl->turn_around_offset);
	rctl->jctl_turn_around = mur_jctl;
	if (NULL != rctl->csd && mur_jctl->turn_around_tn > rctl->csd->trans_hist.curr_tn)
	{
		gtm_putmsg(VARLSTCNT(6) ERR_TRNARNDTNHI, 4, mur_jctl->jnl_fn_len,
			mur_jctl->jnl_fn, mur_jctl->turn_around_tn, rctl->csd->trans_hist.curr_tn);
		return ERR_TRNARNDTNHI;
	}
	mur_jctl->turn_around_offset = mur_jctl->rec_offset;
	mur_jctl->turn_around_time = mur_rab.jnlrec->prefix.time;
	mur_jctl->turn_around_seqno = mur_rab.jnlrec->jrec_epoch.jnl_seqno;
	mur_jctl->turn_around_tn = ((jrec_prefix *)mur_rab.jnlrec)->tn;
	if (NULL == rctl->jctl_save_turn_around)
	{
		rctl->jctl_save_turn_around = mur_jctl;
		mur_jctl->save_turn_around_offset = mur_jctl->turn_around_offset;
	} else
	{
		/* mur_apply_pblk set this earlier for interrupted recovery */
		assert(rctl->jctl_save_turn_around->save_turn_around_offset);
		/* rctl->jctl_save_turn_around->turn_around_offset  could be 0 */
	}
	return SS_NORMAL;
}

/*	This routine performs backward processing for forward and backward recover/rollback.
 *	This creates list of tokens for broken fenced transactions.
 *	For noverify qualifier in backward recovry, it may apply PBLK calling mur_apply_pblk()
 */
boolean_t mur_back_process(boolean_t apply_pblk, seq_num *pre_resolve_seqno)
{
	boolean_t		apply_pblk_this_region, resolve_seq, inactive_check, this_reg_resolved;
	enum jnl_record_type 	rectype;
	enum rec_fence_type	rec_fence;
	int4			rec_image_count = 0;	/* This is a dummy variable for UNIX */
	int			regno, reg_total, partner;
	uint4			rec_pid, status;
	jnl_tm_t		reg_tp_resolve_time, max_lvrec_time, min_broken_time = MAXUINT4, rec_time;
	token_num		token;
	seq_num			rec_token_seq;
	multi_struct		*multi;
	ht_entry		*hentry;
	pini_list_struct	*plst;
	reg_ctl_list		*rctl, *rctl_top;
	file_control		*fc;
	error_def(ERR_JNLREADBOF);
	error_def(ERR_NOPREVLINK);
	error_def(ERR_MUJNINFO);
	error_def(ERR_RESOLVESEQNO);

	reg_total = murgbl.reg_total;
	if (mur_options.forward)
	{
		if (mur_options.verify) /* verify continues till beginning of journal file */
			murgbl.tp_resolve_time = 0;
		else
		{
			murgbl.tp_resolve_time = mur_ctl[0].lvrec_time;
			for (regno = 1; regno < reg_total; regno++)
			{
				if (mur_ctl[regno].lvrec_time < murgbl.tp_resolve_time)
					murgbl.tp_resolve_time = mur_ctl[regno].lvrec_time;
			}
			assert(murgbl.tp_resolve_time);
		}
	} else
	{
		/* mur_ctl[regno].lvrec_time is the last valid record's timestamp of journal file mur_ctl[regno].jctl->jnl_fn.
		 * mur_sort_files will sort regions so that:
		 * 	for regno = 0 to (reg_total-2) we have
		 *		mur_ctl[regno].lvrec_time <= mur_ctl[regno+1].lvrec_time
		 */
		mur_sort_files();
		murgbl.tp_resolve_time = MAXUINT4;
		max_lvrec_time = mur_ctl[reg_total - 1].lvrec_time;
		for (regno = 0; regno < reg_total; regno++)
		{
			rctl = &mur_ctl[regno];
			/* Assumption : It is guaranteed to see an EPOCH in every
			 * "rctl->jctl->jfh->epoch_interval + MAX_EPOCH_DELAY" seconds. */
			assert(max_lvrec_time >= rctl->jctl->jfh->epoch_interval + MAX_EPOCH_DELAY);
			reg_tp_resolve_time = max_lvrec_time - rctl->jctl->jfh->epoch_interval - MAX_EPOCH_DELAY;
			if (rctl->lvrec_time > reg_tp_resolve_time)
				reg_tp_resolve_time = rctl->lvrec_time;
			if (reg_tp_resolve_time < murgbl.tp_resolve_time)
				murgbl.tp_resolve_time = reg_tp_resolve_time;
			if (mur_options.update)
			{
				assert(NULL != rctl->csd);
				assert(!rctl->jfh_recov_interrupted || (rctl->jctl_head == rctl->jctl_save_turn_around));
				/* assert(!rctl->jfh_recov_interrupted || rctl->recov_interrupted); ???
				 * The above assert is temporarily commented out because in mur_close_files we set
				 * csd->recov_interrupted = FALSE before we set jctl->jfh->recover_interrupted = FALSE
				 * so it can fail if recover crashes in between those two assignments. But the assert is
				 * not removed as the implications of the assert not being true have to be handled in
				 * the entire recover code before removing it.
				 */
				if (rctl->recov_interrupted)
				{
					assert(murgbl.intrpt_recovery);
					/* Previous backward recovery/rollback was interrupted.
					 * Update tp_resolve_time/resync_seqno to reflect the minimum of the previous and
					 * 	current recovery/rollback's turn-around-points.
					 * It is possible that both rctl->csd->intrpt_recov_resync_seqno and
					 * 	rctl->csd->intrpt_recov_tp_resolve_time are zero in case previous recover
					 * 	was killed after mur_open_files (which sets csd->recov_interrupted) but before
					 * 	mur_back_process() which would have set csd->intrpt_recov_tp_resolve_time
					 */
					if (rctl->csd->intrpt_recov_resync_seqno)
					{
						if (!mur_options.rollback)
							GTMASSERT;	/* better error out ??? */
						if ((0 == murgbl.resync_seqno) ||
								(rctl->csd->intrpt_recov_resync_seqno < murgbl.resync_seqno))
							murgbl.resync_seqno = rctl->csd->intrpt_recov_resync_seqno;
					}
					if (rctl->csd->intrpt_recov_tp_resolve_time &&
							rctl->csd->intrpt_recov_tp_resolve_time < murgbl.tp_resolve_time)
						murgbl.tp_resolve_time = rctl->csd->intrpt_recov_tp_resolve_time;
				}
			}
		}
		if (mur_options.since_time < murgbl.tp_resolve_time)
			murgbl.tp_resolve_time = mur_options.since_time;
		if (FENCE_NONE == mur_options.fences && !mur_options.since_time_specified && !murgbl.intrpt_recovery)
			murgbl.tp_resolve_time = max_lvrec_time;
		if (murgbl.stop_rlbk_seqno < murgbl.resync_seqno)
		{
			assert(murgbl.intrpt_recovery);
			gtm_putmsg(VARLSTCNT(3) ERR_RESOLVESEQNO, 1, &murgbl.stop_rlbk_seqno);
			murgbl.resync_seqno = murgbl.stop_rlbk_seqno;
		} else if ((mur_options.resync_specified || mur_options.fetchresync_port) &&
						murgbl.stop_rlbk_seqno > murgbl.resync_seqno)
			gtm_putmsg(VARLSTCNT(3) ERR_RESOLVESEQNO, 1, &murgbl.resync_seqno);
		if (!murgbl.intrpt_recovery)
			resolve_seq = (mur_options.rollback && mur_options.resync_specified);
		else
			resolve_seq = (0 != murgbl.resync_seqno);
		if (mur_options.update)
		{
			for (regno = 0; regno < reg_total; regno++)
			{
				rctl = &mur_ctl[regno];
				assert(rctl->csd->recov_interrupted);	/* mur_open_files set this */
				if (apply_pblk && !rctl->jfh_recov_interrupted)
				{	/* When the 'if' condition is TRUE, we apply PBLKs in mur_back_process.
					 * Store the murgbl.tp_resolve_time/murgbl.resync_seqno.
					 * So we remember to undo PBLKs at least upto that point,
					 * in case this recovery is interrupted/crashes.
					 */
					rctl->csd->intrpt_recov_tp_resolve_time = murgbl.tp_resolve_time;
					rctl->csd->intrpt_recov_resync_seqno = (resolve_seq ? murgbl.resync_seqno : 0);
					/* flush the changed csd to disk */
					fc = rctl->gd->dyn.addr->file_cntl;
					fc->op = FC_WRITE;
					fc->op_buff = (sm_uc_ptr_t)rctl->csd;
					fc->op_len = ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE);
					fc->op_pos = 1;
					dbfilop(fc);
				}
			}
		}
	} /* end else !mur_options.forward */
	*pre_resolve_seqno = 0;
	murgbl.db_updated = mur_options.update && !mur_options.verify;
	/* At this point we have computed murgbl.tp_resolve_time. It is the time upto which (at least)
	 * we need to do token resolution. This is for all kinds of recovery and rollback.
	 * Following for loop will do backward processing and resolve token up to this murgbl.tp_resolve_time.
	 * (For recover with lower since_time, we already set murgbl.tp_resolve_time as since_time.
	 *  For interrupted recovery we also considered previous recovery's murgbl.tp_resolve_time.)
	 * For rollback command without resync qualifier (even fetch_resync) we also resolve upto this murgbl.tp_resolve_time.
	 * For rollback with resync qualifier, we may need to resolve more than the murgbl.tp_resolve_time */
	for (mur_regno = 0, rctl = mur_ctl, rctl_top = mur_ctl + reg_total; rctl < rctl_top; rctl++, mur_regno++)
	{
		apply_pblk_this_region = apply_pblk && !rctl->jfh_recov_interrupted;
		/* Note that for rctl->jfh_recov_interrupted we do not apply pblks in this routine */
		mur_jctl = rctl->jctl;
		assert(NULL == mur_jctl->next_gen);
		mur_jctl->rec_offset = mur_jctl->lvrec_off;
		rec_token_seq = MAXUINT8;
		rec_time = 0;
		inactive_check = (rctl->lvrec_time < murgbl.tp_resolve_time) && !resolve_seq && (FENCE_NONE != mur_options.fences);
		this_reg_resolved = FALSE;
		for (status = mur_prev(mur_jctl->rec_offset); SS_NORMAL == status; status = mur_prev_rec())
		{
			assert(0 == mur_jctl->turn_around_offset);
			rectype = mur_rab.jnlrec->prefix.jrec_type;
			if (inactive_check && !(JRT_EPOCH == rectype || JRT_EOF == rectype ||
							JRT_PFIN == rectype || JRT_ALIGN == rectype))
				/* When region is inactive, that is, no logical updates are done, only EPOCH/PFIN/ALIGN/EOF
				 * can be seen. Otherwise, it is an out of design situation */
				GTMASSERT;
			/* Resolve point is defined as the offset of the earliest journal record whose
			 *      a) timestamp is >= murgbl.tp_resolve_time (if resolve_seq == FALSE)
	 		 *      b) jnl_seqno is >= murgbl.resync_seqno is reached (if resolve_seq == TRUE)
			 * Turn around point is defined as the offset of the earliest EPOCH whose
			 *      a) timestamp is less than murgbl.tp_resolve_time
			 *              (if recover OR rollback with murgbl.resync_seqno == 0)
			 *      b) jnl_seqno is < murgbl.resync_seqno (if rollback with murgbl.resync_seqno != 0)
			 * We resolve tokens or find holes for rollback till Resolve Point,
			 *      though Turn Around Point can be much before this.
			 * We apply PBLK till Turn Around Point.
			 */
			if (JRT_PBLK == rectype && apply_pblk_this_region)
			{
				mur_output_pblk();
				continue;
			}
			/* In journal records token_seq field is a union of jnl_seqno and token for TP or unfenced records.
			 * It is in the same offset for all records when they are present.
			 * For ZTP jnl_seqno and token are two different fields.
			 * offset of jnl_seqno in ZTP is same as that of token_seq in TP or unfenced records.
			 * token field is only present in ZTP.
			 * For non-replication (that is, doing recover) and unfenced records token_seq field has no use.
			 * For replication (that is, doing rollback) unfenced and TP records contain jnl_seqno in token_seq field.
			 * This is used as token in the hash table.
			 * Note : ZTP is not currently supported in replication. When supported here for rollback
			 *	  we may need to use both token and sequence number fields to do token resolution and
			 *	  find holes in sequence number.
			 */
			rec_time = mur_rab.jnlrec->prefix.time;
			if (REC_HAS_TOKEN_SEQ(rectype))
			{
				rec_token_seq = GET_JNL_SEQNO(mur_rab.jnlrec);
				if (mur_options.rollback && !this_reg_resolved && rec_time < murgbl.tp_resolve_time)
				{
					if (JRT_EPOCH == rectype || JRT_EOF == rectype)
					{
						if (rec_token_seq > *pre_resolve_seqno)
							*pre_resolve_seqno = rec_token_seq;
					} else
					{
						if ((rec_token_seq + 1) > *pre_resolve_seqno)
							*pre_resolve_seqno = rec_token_seq + 1;
					}
					this_reg_resolved = TRUE;
				}
			} else
				continue;
			if (JRT_EPOCH == rectype)
			{
				assert(mur_options.forward || murgbl.intrpt_recovery ||
					NULL == rctl->csd || mur_rab.jnlrec->prefix.tn <= rctl->csd->trans_hist.curr_tn);
				if (rec_time < murgbl.tp_resolve_time &&
					(!murgbl.resync_seqno || rec_token_seq <= murgbl.resync_seqno))
				{
					if (SS_NORMAL != (status = save_turn_around_point(rctl)))
						return status;
					break;
				}
				continue;
			}
			if (rec_time < murgbl.tp_resolve_time && (!resolve_seq || rec_token_seq < murgbl.resync_seqno))
				continue;
			if ((FENCE_NONE == mur_options.fences || rec_time > mur_options.before_time)
				|| (rec_time < murgbl.tp_resolve_time && (!resolve_seq || rec_token_seq < murgbl.resync_seqno)))
				continue;
			if (IS_FENCED(rectype))
			{	/* Note for a ZTP if FSET/GSET is present before mur_options.before_time and
				 * GUPD/ZTCOM are present after mur_options.before_time, it is considered broken. */
				rec_fence = GET_REC_FENCE_TYPE(rectype);
				if (SS_NORMAL != (status = mur_get_pini(mur_rab.jnlrec->prefix.pini_addr, &plst)))
					break;
				rec_pid = plst->jpv.jpv_pid;
				VMS_ONLY(rec_image_count = plst->jpv.jpv_image_count;)
				token = (TPFENCE == rec_fence) ? rec_token_seq :
					((struct_jrec_ztp_upd *)mur_rab.jnlrec)->token;
				if (IS_SET_KILL_ZKILL(rectype))	/* TUPD/UUPD/FUPD/GUPD */
				{
					if (NULL != (multi = MUR_TOKEN_LOOKUP(token, rec_pid, rec_image_count,
												rec_time, rec_fence)))
					{
						if (multi->fence != rec_fence)
						{
							assert(!mur_options.rollback);	/* jnl_seqno cannot be duplicate */
							if (!(mur_report_error(MUR_DUPTOKEN)))
								return FALSE;
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
								return FALSE;
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
				{
					/* We reuse same token table. most of the fields in multi_struct are unused */
					MUR_TOKEN_ADD(multi, token, 0, 0, 0, 0, rec_fence, 0);
				} else
				{
					assert(FALSE);
					if (!(mur_report_error(MUR_DUPTOKEN)))
						return FALSE;
				}
			}
		} /* end for mur_prev */
		assert(!mur_options.rollback || this_reg_resolved);
		if (mur_options.verbose)
			gtm_putmsg(VARLSTCNT(15) ERR_MUJNINFO, 13, LEN_AND_LIT("Mur_back_process:trnarnd "),
			mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, mur_jctl->rec_offset,
			murgbl.tp_resolve_time, mur_jctl->turn_around_offset, mur_jctl->turn_around_time,
			mur_jctl->turn_around_tn, &mur_jctl->turn_around_seqno,
			min_broken_time, murgbl.token_table.count, murgbl.broken_cnt);
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
						return FALSE;
					} else
					{
						mur_jctl->rec_offset = JNL_HDR_LEN + PINI_RECLEN;
						status = mur_prev(mur_jctl->rec_offset);
						if (SS_NORMAL != status)
							return FALSE;
						assert(JRT_EPOCH == mur_rab.jnlrec->prefix.jrec_type);
						if (SS_NORMAL != (status = save_turn_around_point(rctl)))
							return status;
					}
				} else	/* mur_read_file should have issued messages */
					return FALSE;
			} else if (ERR_JNLREADBOF != status)	/* mur_read_file should have issued messages */
			/* for mur_options.forward ERR_JNLREADBOF is not error but others are */
				return FALSE;
		}
	} /* end for mur_regno */
	if (!mur_forward && NULL == rctl->jctl_turn_around)
		GTMASSERT;
	/* Since murgbl.tp_resolve_time is one resolve time for all regions, no implicit lookback processing
	 * to resolve transactions is not necessary */
	return TRUE;
}
