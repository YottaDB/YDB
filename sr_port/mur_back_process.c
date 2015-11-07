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
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#include "wbox_test_init.h"
#include "timers.h"
#ifdef GTM_TRUNCATE
#include "gdsfilext_nojnl.h"
#endif
#include "have_crit.h"

GBLREF 	mur_gbls_t	murgbl;
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	mur_opt_struct	mur_options;
GBLREF 	jnl_gbls_t	jgbl;

#ifdef DEBUG
static boolean_t	iterationcnt;
static jnl_tm_t		prev_max_lvrec_time, prev_min_bov_time;
#endif

error_def(ERR_CHNGTPRSLVTM);
error_def(ERR_DUPTOKEN);
error_def(ERR_EPOCHTNHI);
error_def(ERR_JNLBADRECFMT);
error_def(ERR_JNLREADBOF);
error_def(ERR_MUINFOSTR);
error_def(ERR_MUINFOUINT4);
error_def(ERR_MUINFOUINT8);
error_def(ERR_MUJNLSTAT);
error_def(ERR_NOPREVLINK);
error_def(ERR_RESOLVESEQNO);
error_def(ERR_RESOLVESEQSTRM);
error_def(ERR_TEXT);

#define	MAX_BACK_PROCESS_REDO_CNT	8

#define SAVE_PRE_RESOLVE_SEQNO(rectype, rec_time, rec_token_seq, pre_resolve_seqno)				\
{														\
	if ((JRT_EPOCH == rectype) || (JRT_EOF == rectype))							\
	{													\
		if (rec_token_seq > *pre_resolve_seqno)								\
			*pre_resolve_seqno = rec_token_seq;							\
	} else													\
	{													\
		if ((rec_token_seq + 1) > *pre_resolve_seqno)							\
			*pre_resolve_seqno = rec_token_seq + 1;							\
	}													\
	if (mur_options.verbose)										\
	{													\
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_MUINFOUINT8, 4, LEN_AND_LIT("Pre-resolve seqno"),	\
			pre_resolve_seqno, pre_resolve_seqno);							\
	}													\
}

#define MUR_BACK_PROCESS_ERROR(JCTL, JJCTL, MESSAGE_STRING)					\
{												\
	if (JCTL->after_end_of_data)								\
	{											\
		*JJCTL = JCTL;									\
		return ERR_JNLBADRECFMT;							\
	}											\
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT(MESSAGE_STRING));	\
	if (!mur_report_error(JCTL, MUR_JNLBADRECFMT))						\
	{											\
		*JJCTL = JCTL;									\
		return ERR_JNLBADRECFMT;							\
	} else											\
		continue;									\
}

#define MUR_BACK_PROCESS_ERROR_STR(JCTL, JJCTL, MESSAGE_STRING)					\
{												\
	if (JCTL->after_end_of_data)								\
	{											\
		*JJCTL = JCTL;									\
		return ERR_JNLBADRECFMT;							\
	}											\
	gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_STR(MESSAGE_STRING));	\
	if (!mur_report_error(JCTL, MUR_JNLBADRECFMT))						\
	{											\
		*JJCTL = JCTL;									\
		return ERR_JNLBADRECFMT;							\
	} else											\
		continue;									\
}

#ifdef VMS
#define	VMS_MUR_BACK_PROCESS_GET_IMAGE_COUNT(JCTL, JNLREC, JJCTL, REC_IMAGE_COUNT, STATUS)	\
{												\
	MUR_GET_IMAGE_COUNT(JCTL, JNLREC, REC_IMAGE_COUNT, STATUS);				\
	if (SS_NORMAL != STATUS)								\
	{	/* We saw a corrupt journal record. Possible only if journal file had a crash	\
		 * and have not yet reached the last epoch in backward processing and the	\
		 * pini_addr should also point to an offset that is after the last epoch.	\
		 */										\
		assert(JCTL->jfh->crash && (JCTL->rec_offset >= JNLREC->prefix.pini_addr)	\
			&& (JNLREC->prefix.pini_addr >= JCTL->jfh->end_of_data));		\
		/* Register the offset of the corrupt record to be the PINI record (not the	\
		 * current journal record), this way the "mur_back_processing" redo logic	\
		 * inside "mur_back_process" restarts mur_fread_eof_crash search from the	\
		 * lower offset and avoids lots of mur_back_processing redos (GTM-7393).	\
		 */										\
		JCTL->rec_offset = JNLREC->prefix.pini_addr;					\
		MUR_BACK_PROCESS_ERROR(JCTL, JJCTL, "pini_addr is bad");			\
	}											\
}
#else
#define	VMS_MUR_BACK_PROCESS_GET_IMAGE_COUNT(JCTL, JNLREC, JJCTL, REC_IMAGE_COUNT, STATUS)
#endif

#define	MUR_TCOM_TOKEN_PROCESSING(jctl, jjctl, token, rec_image_count, rec_time, rec_fence, regno, reg_total, jnlrec)		\
{																\
	GBLREF	mur_opt_struct	mur_options;											\
	GBLREF 	mur_gbls_t	murgbl;												\
																\
	multi_struct		*multi;												\
																\
	if (NULL != (multi = MUR_TOKEN_LOOKUP(token, rec_image_count, rec_time, rec_fence)))					\
	{															\
		if ((regno == multi->regnum) || (multi->fence != rec_fence) || (0 == multi->partner))				\
		{														\
			assert(0 != multi->partner);										\
			assert(!mur_options.rollback);	/* jnl_seqno cannot be duplicate */					\
			if (!mur_report_error(jctl, MUR_DUPTOKEN))								\
			{													\
				*jjctl = jctl;											\
				return ERR_DUPTOKEN;										\
			}													\
			SET_THIS_TN_AS_BROKEN(multi, reg_total); /* This is broken */						\
			if (rec_time < multi->time)										\
				multi->time = rec_time;										\
		} else														\
		{														\
			assert(&jnlrec->jrec_tcom.num_participants == &jnlrec->jrec_ztcom.participants);			\
			/* We expect each TCOM record to have the same # of participants. Assert that. There is one exception	\
			 * though in that if the multi structure got created in the hash table as part of a broken transaction	\
			 * (e.g. a TSET or USET record was seen in backward processing without having seen a TCOM record first)	\
			 * we would have set the participants count to one more than the total # of regions participating in	\
			 * the recovery thereby ensuring it gets treated as a broken transaction.				\
			 */													\
			DEBUG_ONLY(												\
				if (jnlrec->jrec_tcom.num_participants != multi->tot_partner)					\
				{												\
					assert(multi->this_is_broken);								\
					assert(multi->tot_partner >= (jnlrec->jrec_tcom.num_participants + 1));			\
				}												\
			)													\
			assert(0 < multi->partner);										\
			multi->partner--;											\
			assert((TPFENCE != rec_fence) || rec_time == multi->time);						\
			assert((ZTPFENCE != rec_fence) || rec_time >= multi->time);						\
			if (0 == multi->partner)										\
				murgbl.broken_cnt--;	/* It is resolved */							\
			multi->regnum = regno;											\
		}														\
	} else															\
	{															\
		assert(&jnlrec->jrec_tcom.num_participants == &jnlrec->jrec_ztcom.participants);				\
		MUR_TOKEN_ADD(multi, token, rec_image_count, rec_time,								\
			jnlrec->jrec_tcom.num_participants, rec_fence, regno);							\
	}															\
}

#define TRANS_NUM_CONT_CHK_FAILED		"Transaction number continuity check failed: [0x%08X] vs [0x%08X]"
#define SEQ_NUM_CONT_CHK_FAILED			"Sequence number continuity check failed: [0x%08X] vs [0x%08X]"
#define TRANS_OR_SEQ_NUM_CONT_CHK_FAILED_SZ	(MAX(SIZEOF(TRANS_NUM_CONT_CHK_FAILED), SIZEOF(SEQ_NUM_CONT_CHK_FAILED)) + 2 * 20)

STATICFNDCL void save_turn_around_point(reg_ctl_list *rctl, jnl_ctl_list *jctl, boolean_t apply_pblk);

STATICFNDEF void save_turn_around_point(reg_ctl_list *rctl, jnl_ctl_list *jctl, boolean_t apply_pblk)
{
	jnl_record		*jnlrec;
	DEBUG_ONLY(jnl_ctl_list	*tmpjctl;)

	assert(!mur_options.forward);
	assert(jctl->reg_ctl == rctl);
	jnlrec = rctl->mur_desc->jnlrec;
	assert(JRT_EPOCH == jnlrec->prefix.jrec_type);
	assert(NULL == rctl->jctl_turn_around);
	assert(0 == jctl->turn_around_offset);
	rctl->jctl_turn_around = jctl;
	jctl->turn_around_offset = jctl->rec_offset;
	jctl->turn_around_time = jnlrec->prefix.time;
	jctl->turn_around_seqno = jnlrec->jrec_epoch.jnl_seqno;
	jctl->turn_around_tn = ((jrec_prefix *)jnlrec)->tn;
	/* Note down the fully_upgraded field of the turn around EPOCH record. Later during forward recovery we will use this
	 * field to update rctl->csd->fully_upgraded
	 */
	jctl->turn_around_fullyupgraded = jnlrec->jrec_epoch.fully_upgraded;
	DEBUG_ONLY(
		/* before updating, check that previous pblk stop point is later than the final turn-around-point */
		for (tmpjctl = rctl->jctl_apply_pblk; NULL != tmpjctl && tmpjctl != jctl; tmpjctl = tmpjctl->prev_gen)
			;
		assert((NULL == rctl->jctl_apply_pblk)
			|| ((NULL != tmpjctl) && ((tmpjctl != rctl->jctl_apply_pblk)
						|| (tmpjctl->apply_pblk_stop_offset >= jctl->turn_around_offset))));
	)
	if (apply_pblk)
	{	/* we have applied more PBLKs than is already stored in rctl->jctl_apply_pblk. update that and related fields */
		if (NULL != rctl->jctl_apply_pblk)
		{	/* this was set to non-NULL by the previous iteration of mur_back_process. clear that. */
			assert(rctl->jctl_apply_pblk->apply_pblk_stop_offset);
			rctl->jctl_apply_pblk->apply_pblk_stop_offset = 0;
		}
		rctl->jctl_apply_pblk = jctl;
		jctl->apply_pblk_stop_offset = jctl->turn_around_offset;
	}
}

boolean_t mur_back_process(boolean_t apply_pblk, seq_num *pre_resolve_seqno)
{
	jnl_ctl_list	*jctl;
	reg_ctl_list	*rctl;
	uint4		status;
	int 		redo_cnt, regno, reg_total;
	jnl_tm_t	alt_tp_resolve_time;
	jnl_record	*jnlrec;

	assert(!mur_options.forward || 0 == mur_options.since_time);
	assert(!mur_options.forward || 0 == mur_options.lookback_time);
	reg_total = murgbl.reg_total;
	alt_tp_resolve_time = 0;
	for ( redo_cnt = 0; ; redo_cnt++)
	{
		assert(MAX_BACK_PROCESS_REDO_CNT > redo_cnt);
			/* ensure we are not doing too many redos of "mur_fread_eof_crash"/"mur_back_processing" */
		*pre_resolve_seqno = 0;
		DEBUG_ONLY(jctl = NULL;)
		status = mur_back_processing(&jctl, apply_pblk, pre_resolve_seqno, alt_tp_resolve_time);
		assert((SS_NORMAL == status) || (NULL != jctl));	/* should have been initialized by "mur_back_processing" */
		if ((ERR_JNLBADRECFMT == status) && jctl->after_end_of_data)
		{
			assert(!jctl->next_gen);
			PRINT_VERBOSE_TAIL_BAD(jctl);
			if (SS_NORMAL != mur_fread_eof_crash(jctl, jctl->jfh->end_of_data, jctl->rec_offset))
				return FALSE;
		} else if (ERR_CHNGTPRSLVTM == status)
		{
			jnlrec = jctl->reg_ctl->mur_desc->jnlrec;
			gtm_putmsg_csa(CSA_ARG(jctl->reg_ctl->csa) VARLSTCNT(6) ERR_CHNGTPRSLVTM, 4, jgbl.mur_tp_resolve_time,
				jnlrec->prefix.time, jctl->jnl_fn_len, jctl->jnl_fn);
			assert(jgbl.mur_tp_resolve_time > jnlrec->prefix.time);
			alt_tp_resolve_time = jnlrec->prefix.time;
		} else	/* An error message must have already been printed if status != SS_NORMAL */
			break;
		JNL_PUT_MSG_PROGRESS("Restarting Backward processing");
		REINITIALIZE_LIST(murgbl.multi_list);
		reinitialize_hashtab_int8(&murgbl.token_table);
		murgbl.broken_cnt = 0;
		/* We must restart from latest generation. */
		for (regno = 0; regno < reg_total; regno++)
		{
			rctl = &mur_ctl[regno];
			jctl = rctl->jctl;
			assert(jctl->reg_ctl == rctl);
			for ( ; ;)
			{
				jctl->turn_around_offset = 0;
				jctl->turn_around_time = 0;
				jctl->turn_around_seqno = 0;
				jctl->turn_around_tn = 0;
				if (NULL == jctl->next_gen)
					break;
				jctl = jctl->next_gen;
				assert(jctl->reg_ctl == rctl);
			}
			rctl->jctl = jctl;	/* Restore latest generation before the failure */
			rctl->jctl_turn_around = NULL;
		}
	} /* end infinite for loop */
	return (SS_NORMAL == status);
}

uint4	mur_back_processing_one_region(mur_back_opt_t *mur_back_options)
{
	boolean_t		apply_pblk_this_region, first_epoch, reached_trnarnd, skip_rec, this_reg_resolved;
	enum jnl_record_type	rectype;
	enum rec_fence_type	rec_fence;
	int			idx, regno, reg_total, strm_idx;
	int4			rec_image_count = 0;	/* This is a dummy variable for UNIX */
	jnl_ctl_list		*jctl, **jjctl;
	jnl_record		*jnlrec;
	jnl_string		*keystr;
	jnl_tm_t		rec_time;
	multi_struct		*multi;
	mur_read_desc_t		*mur_desc;
	reg_ctl_list		*rctl;
	seq_num			*pre_resolve_seqno, rec_token_seq, save_resync_seqno, save_strm_seqno, strm_seqno;
	token_num		token;
	trans_num		rec_tn;
	uint4			max_blk_size, max_rec_size;
	uint4			status, val_len;
	unsigned short		max_key_size;
#	ifdef GTM_CRYPT
	int			gtmcrypt_errno;
#	endif
	char			s[TRANS_OR_SEQ_NUM_CONT_CHK_FAILED_SZ];	/* for appending sequence or transaction number */
#	ifdef GTM_TRUNCATE
	uint4			cur_total, old_total;
#	endif

	jctl = mur_back_options->jctl;
	rctl = jctl->reg_ctl;
	regno = rctl - &mur_ctl[0];
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
	mur_desc = rctl->mur_desc;
	jnlrec = mur_desc->jnlrec;
	rec_tn = jnlrec->prefix.tn;
	rec_token_seq = mur_back_options->rec_token_seq;
	first_epoch = mur_back_options->first_epoch;
	status = mur_back_options->status;
	jjctl = mur_back_options->jjctl;
	this_reg_resolved = FALSE;
	apply_pblk_this_region = mur_back_options->apply_pblk && !rctl->jfh_recov_interrupted;
	pre_resolve_seqno = mur_back_options->pre_resolve_seqno;
	reg_total = murgbl.reg_total;
	for ( ; SS_NORMAL == status; status = mur_prev_rec(&jctl))
	{
		jctl->after_end_of_data = jctl->after_end_of_data && (jctl->rec_offset >= jctl->jfh->end_of_data);
		assert(0 == jctl->turn_around_offset);
		jnlrec = mur_desc->jnlrec;
		rectype = (enum jnl_record_type)jnlrec->prefix.jrec_type;
		/* Even if -verify is NOT specified, if the journal file had a crash, do verification until
		 * the first epoch is reached as the journal file could be corrupt anywhere until then
		 * (mur_fread_eof on the journal file at the start might not have caught it).
		 */
		if (mur_options.verify || (jctl->jfh->crash && jctl->after_end_of_data))
		{
			if (!mur_validate_checksum(jctl))
				MUR_BACK_PROCESS_ERROR(jctl, jjctl, "Checksum validation failed");
			if ((jnlrec->prefix.tn != rec_tn) && (jnlrec->prefix.tn != (rec_tn - 1)))
			{
				SNPRINTF(s, TRANS_OR_SEQ_NUM_CONT_CHK_FAILED_SZ, TRANS_NUM_CONT_CHK_FAILED,
					jnlrec->prefix.tn, rec_tn);
				rec_tn = jnlrec->prefix.tn;
				MUR_BACK_PROCESS_ERROR_STR(jctl, jjctl, s);
			}
			if (mur_options.rollback && REC_HAS_TOKEN_SEQ(rectype) && (GET_JNL_SEQNO(jnlrec) > rec_token_seq))
			{
				SNPRINTF(s, TRANS_OR_SEQ_NUM_CONT_CHK_FAILED_SZ, SEQ_NUM_CONT_CHK_FAILED,
					GET_JNL_SEQNO(jnlrec), rec_token_seq);
				rec_token_seq = GET_JNL_SEQNO(jnlrec);
				MUR_BACK_PROCESS_ERROR_STR(jctl, jjctl, s);
			}
			if (IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype))
			{
				keystr = (jnl_string *)&jnlrec->jrec_set_kill.mumps_node;
				/* Assert that ZTWORMHOLE type record too has same layout as KILL/SET */
				assert((sm_uc_ptr_t)keystr == (sm_uc_ptr_t)&jnlrec->jrec_ztworm.ztworm_str);
#				ifdef GTM_CRYPT
				if (jctl->jfh->is_encrypted)
				{
					MUR_DECRYPT_LOGICAL_RECS(keystr, jnlrec->prefix.forwptr, jctl->encr_key_handle,
									gtmcrypt_errno);
					if (0 != gtmcrypt_errno)
					{
						GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, jctl->jnl_fn_len, jctl->jnl_fn);
						*jjctl = jctl;
						return gtmcrypt_errno;
					}
				}
#				endif
				if (IS_ZTWORM(rectype))
				{	/* ZTWORMHOLE type */
#					ifdef GTM_TRIGGER
					if (MAX_ZTWORMHOLE_SIZE < keystr->length)
						MUR_BACK_PROCESS_ERROR(jctl, jjctl, "ZTWORMHOLE size check failed");
#					endif
				} else
				{	/* SET or KILL type */
					if (keystr->length > max_key_size)
						MUR_BACK_PROCESS_ERROR(jctl, jjctl, "Key size check failed");
					if (0 != keystr->text[keystr->length - 1])
						MUR_BACK_PROCESS_ERROR(jctl, jjctl, "Key null termination check failed");
					if (IS_SET(rectype))
					{
						GET_MSTR_LEN(val_len, &keystr->text[keystr->length]);
						if (val_len > max_rec_size)
							MUR_BACK_PROCESS_ERROR(jctl, jjctl, "Record size check failed");
					}
				}
			} else if (JRT_PBLK == rectype)
			{
				if (jnlrec->jrec_pblk.bsiz > max_blk_size)
					MUR_BACK_PROCESS_ERROR(jctl, jjctl, "PBLK size check failed");
				assert((FALSE == apply_pblk_this_region) || !mur_options.verify);
				/* In case this journal file was crashed it is possible that we see a good PBLK at
				 * this point in time but could find bad journal data in the journal file at an
				 * EARLIER offset (further in backward processing). If the current recovery has been
				 * invoked with -noverify, we dont have a separate pblk application phase. One might
				 * wonder if in such a case, it is safe to apply good pblks at this point without
				 * knowing if bad pblks could be encountered later in backward processing. Turns out
				 * it is safe. If there were bad pblks BEFORE this good pblk, this means the good pblk
				 * landed in the journal file on disk because of pure chance (the IO system scheduled
				 * this write before the crash whereas the write of the previous bad pblks did not get
				 * a chance). As long as the bad pblks were not synced to the file, this means the db
				 * blocks corresponding to the good blks did NOT get modified at all (because the
				 * function wcs_wtstart ensures a db blk is written ONLY if all journal records until
				 * its corresponding pblk have been fsynced to the journal file). So basically the good
				 * pblk would be identical to the copy of the block in the unrecovered database at
				 * this point so it does not hurt to play it on top. For cases where there are no such
				 * bad data gaps in the journal file, playing the pblk when -noverify is specified is
				 * necessary as the pblk would be different from the copy of the blk in the database.
				 * So it is safe to do the play in both cases.
				 */
				if (apply_pblk_this_region)
				{
					assert(!mur_options.rollback_losttnonly);
					mur_output_pblk(rctl);
				}
				continue;
			}
		} else if (JRT_PBLK == rectype && apply_pblk_this_region)
		{
			assert(!mur_options.rollback_losttnonly);
			mur_output_pblk(rctl);
			continue;
		}
		if (JRT_TRUNC == rectype)
		{
			NON_GTM_TRUNCATE_ONLY(assert(FALSE));
			GTM_TRUNCATE_ONLY(
				if (mur_options.forward)
					continue;
				old_total = jnlrec->jrec_trunc.orig_total_blks;
				cur_total = rctl->csa->ti->total_blks;
				assert(cur_total >= jnlrec->jrec_trunc.total_blks_after_trunc);
				if (cur_total < old_total)
					status = gdsfilext_nojnl(rctl->gd, old_total, cur_total);
				if (0 != status)
					MUR_BACK_PROCESS_ERROR(jctl, jjctl, "File extend for JRT_TRUNC record failed");
			)
			continue;
		}
		rec_tn = jnlrec->prefix.tn;
		rec_time = jnlrec->prefix.time;
		/* In journal records token_seq field is a union of jnl_seqno and token for TP, ZTP or unfenced records.
		 * For non-replication (that is, doing recover) token_seq.token field is used as token in hash table.
		 * For replication (that is, doing rollback) token_seq.jnl_seqno is used as token in hash table.
		 * Note : ZTP is not supported with replication.
		 */
		if (REC_HAS_TOKEN_SEQ(rectype))
		{
			assert(IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype) || IS_COM(rectype) || (JRT_EPOCH == (rectype))
				|| (JRT_EOF == (rectype)) || (JRT_NULL == (rectype)));
			assert(&jnlrec->jrec_set_kill.token_seq == (token_seq_t *)&jnlrec->jrec_epoch.jnl_seqno);
			assert(&jnlrec->jrec_set_kill.token_seq == (token_seq_t *)&jnlrec->jrec_eof.jnl_seqno);
			assert(&jnlrec->jrec_set_kill.token_seq == (token_seq_t *)&jnlrec->jrec_null.jnl_seqno);
			assert(&jnlrec->jrec_set_kill.token_seq == (token_seq_t *)&jnlrec->jrec_tcom.token_seq);
			assert(&jnlrec->jrec_set_kill.token_seq == (token_seq_t *)&jnlrec->jrec_ztcom.token);
			rec_token_seq = GET_JNL_SEQNO(jnlrec);
			if (mur_options.rollback)
			{
#				ifdef UNIX
				/* In case of -rollback with -resync or -fetchresync on a supplementary instance
				 * with a <strm_seqno>, map back the input resync_strm_seqno to a resync_seqno
				 * as this is needed to set murgbl.losttn_seqno at the end of mur_back_process.
				 */
				if (murgbl.resync_strm_seqno_nonzero && IS_REPLICATED(rectype))
				{
					assert(IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype) || IS_COM(rectype)
						|| (JRT_NULL == (rectype)));
					assert(&jnlrec->jrec_set_kill.strm_seqno == &jnlrec->jrec_null.strm_seqno);
					assert(&jnlrec->jrec_tcom.strm_seqno == &jnlrec->jrec_null.strm_seqno);
					strm_seqno = GET_STRM_SEQNO(jnlrec);
					strm_idx = GET_STRM_INDEX(strm_seqno);
					strm_seqno = GET_STRM_SEQ60(strm_seqno);
					if (murgbl.resync_strm_seqno[strm_idx]
							&& (strm_seqno >= murgbl.resync_strm_seqno[strm_idx])
							&& (murgbl.resync_seqno > rec_token_seq))
					{
						/* Assert that no adjustment of resync_seqno should happen in the second
						 * invocation of "mur_back_processing_one_region" for the same region.
						 */
						assert(mur_back_options->first_epoch);
						murgbl.resync_seqno = rec_token_seq;
					}
				}
#				endif
				/* this_reg_resolved is set to true first time a sequence number is seen before the
				 * jgbl.mur_tp_resolve_time. This is necessary to find any gap in sequence numbers
				 * (C9D11-002465). Any gap will result in broken or lost transactions from the gap.
				 */
				if (!this_reg_resolved && (rec_time < jgbl.mur_tp_resolve_time))
				{
					SAVE_PRE_RESOLVE_SEQNO(rectype, rec_time, rec_token_seq, pre_resolve_seqno);
					this_reg_resolved = TRUE;
				}
			}
		} else
		{
			if (JRT_INCTN == rectype)
				MUR_INCTN_BLKS_TO_UPGRD_ADJUST(rctl);
			continue;
		}
		/* Resolve point is defined as the offset of the earliest journal record whose
		 *      a) timestamp >= jgbl.mur_tp_resolve_time
		 * Turn around point is defined as the offset of the earliest EPOCH whose
		 *      a) timestamp is < jgbl.mur_tp_resolve_time
		 *              (if recover OR rollback with murgbl.resync_seqno == 0)
		 *      b) timestamp is < jgbl.mur_tp_resolve_time AND jnl_seqno is < murgbl.resync_seqno
		 *		(if rollback with murgbl.resync_seqno != 0)
		 * We maintain tokens (hash table) till Resolve Point, though Turn Around Point can be much before this.
		 * We apply PBLK till Turn Around Point.
		 */
		if (JRT_EPOCH == rectype)
		{
			if (!mur_options.forward && first_epoch && !rctl->recov_interrupted &&
				(NULL != rctl->csd) && (rec_tn > rctl->csd->trans_hist.curr_tn))
			{
				assert(FALSE);
				gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(7) ERR_EPOCHTNHI, 5, jctl->rec_offset,
					jctl->jnl_fn_len, jctl->jnl_fn, &rec_tn, &rctl->csd->trans_hist.curr_tn);
				MUR_BACK_PROCESS_ERROR(jctl, jjctl, "Epoch transaction number check failed");
			}
			if (first_epoch)
			{
				if (mur_options.verbose)
				{
					gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(6) ERR_MUINFOUINT4, 4,
						LEN_AND_LIT("    First Epoch Record Offset"),
						jctl->rec_offset, jctl->rec_offset);
					gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(6) ERR_MUINFOUINT4, 4,
						LEN_AND_LIT("    First Epoch Record timestamp"), rec_time, rec_time);
				}
				first_epoch = FALSE;
			}
			assert(mur_options.forward || murgbl.intrpt_recovery || (NULL == rctl->csd)
				|| (jnlrec->prefix.tn <= rctl->csd->trans_hist.curr_tn));
			if (rec_time < jgbl.mur_tp_resolve_time)
			{	/* Reached EPOCH before resolve-time. Check if we have reached turnaround point.
				 * For no rollback OR for simple rollback with -resync or -fetchresync NOT specified,
				 *	we need to go to an epoch BEFORE the resolve-time.
				 * For simple (i.e. journal files were cleanly shutdown) rollback with -resync or
				 *	-fetchresync specified, we need to check if the epoch seqno is less than or
				 *	equal to the input resync_seqno. If yes, then we can stop.
				 * For interrupted rollback, we need to additionally check if any of the potentially
				 *	16 streams had a resync_seqno specified as part of previous interrupted rollbacks
				 *	and if so ensure the epoch is before all those points. That is, even if the epoch
				 *	timestamp is LESSER than the tp_resolve_time, we have to continue backward
				 *	processing until all the epoch's stream seqnos are less than any resync_seqnos
				 *	specified as part of this or previous interrupted rollbacks.
				 */
				reached_trnarnd = TRUE;	/* Assume we have reached turnaround point.
							 * Will be reset if we find otherwise.
							 */
				if (mur_options.rollback && (murgbl.resync_seqno UNIX_ONLY(|| murgbl.resync_strm_seqno_nonzero)))
				{
					assert(!mur_options.forward || !murgbl.resync_seqno);
					if (murgbl.resync_seqno && (rec_token_seq > murgbl.resync_seqno))
						reached_trnarnd = FALSE;
#					ifdef UNIX
					assert(!murgbl.resync_strm_seqno_nonzero || !mur_options.forward);
					if (reached_trnarnd && murgbl.resync_strm_seqno_nonzero)
					{	/* Check if any stream seqnos need to be compared as well */
						if (!rctl->recov_interrupted)
						{	/* For non-interrupted recovery, one strm needs checking */
							idx = murgbl.resync_strm_index;
							if (INVALID_SUPPL_STRM != idx)
							{
								assert((0 <= idx) && (MAX_SUPPL_STRMS > idx));
								strm_seqno = jnlrec->jrec_epoch.strm_seqno[idx];
								if (strm_seqno > murgbl.resync_strm_seqno[idx])
									reached_trnarnd = FALSE;
							}
						} else
						{	/* For interrupted recovery, upto 16 strms need checking */
							for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
							{
								strm_seqno = jnlrec->jrec_epoch.strm_seqno[idx];
								if (strm_seqno > murgbl.resync_strm_seqno[idx])
								{
									reached_trnarnd = FALSE;
									break;
								}
							}
						}
					}
#					endif
				}
				if (reached_trnarnd)
				{
					if (mur_options.rollback && !this_reg_resolved)
					{	/* It is possible this region is not resolved yet in case we found an
						 * EPOCH whose rec_time is EQUAL to the tp_resolve_time. In this case
						 * use the epoch to note down pre-resolve-seqno.
						 */
						SAVE_PRE_RESOLVE_SEQNO(rectype, rec_time, rec_token_seq, pre_resolve_seqno);
						this_reg_resolved = TRUE;
					}
					if (!mur_options.forward)
						save_turn_around_point(rctl, jctl, apply_pblk_this_region);
					PRINT_VERBOSE_STAT(jctl, "mur_back_processing:save_turn_around_point");
					break;
				}
			}
			continue;
		}
		/* Do preliminary checks to see if the jnl record needs to be involved in hashtable token processing */
		if ((FENCE_NONE == mur_options.fences) || (rec_time > mur_options.before_time)
				|| (rec_time < jgbl.mur_tp_resolve_time))
			continue;
		/* Do detailed checks on the jnl record for token processing */
		token = rec_token_seq;
		if (IS_FENCED(rectype))
		{	/* Note for a ZTP if FSET/GSET is present before mur_options.before_time and
			 * GUPD/ZTCOM are present after mur_options.before_time, it is considered broken. */
			rec_fence = GET_REC_FENCE_TYPE(rectype);
			VMS_MUR_BACK_PROCESS_GET_IMAGE_COUNT(jctl, jnlrec, jjctl, rec_image_count, status);
			assert(token == ((struct_jrec_upd *)jnlrec)->token_seq.token);
			if (IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype))	/* TUPD/UUPD/FUPD/GUPD */
			{
				if (NULL != (multi = MUR_TOKEN_LOOKUP(token, rec_image_count, rec_time, rec_fence)))
				{
					if (multi->fence != rec_fence)
					{
						assert(!mur_options.rollback);	/* jnl_seqno cannot be duplicate */
						if (!(mur_report_error(jctl, MUR_DUPTOKEN)))
						{
							*jjctl = jctl;
							return ERR_DUPTOKEN;
						}
						SET_THIS_TN_AS_BROKEN(multi, reg_total); /* This is broken */
						if (rec_time < multi->time)
							multi->time = rec_time;
					} else
					{
						assert((TPFENCE != rec_fence) || multi->time == rec_time);
						if (ZTPFENCE == rec_fence && multi->time > rec_time)
							multi->time = rec_time;
						if (multi->regnum != regno)
						{	/* No TCOM or ZTCOM was seen in this region but corresponding
							 * TUPD/UUPD/FUPD/GUPD records are seen. This is automatically
							 * treated as broken because of the absence of TCOM/ZTCOM. But
							 * we need to signal to forward processing that this region
							 * (even though broken) was seen in backward processing. That is
							 * done by incrementing tot_partner.
							 */
							multi->tot_partner++;
							multi->regnum = regno;
							/* Set a debug-only flag indicating this "multi" structure never
							 * be treated as a GOOD_TN in forward processing. This will be
							 * checked there.
							 */
							DEBUG_ONLY(multi->this_is_broken = TRUE;)
						}
					}
				} else
				{	/* This is broken */
					MUR_TOKEN_ADD(multi, token, rec_image_count,
							rec_time, reg_total + 1, rec_fence, regno);
					/* Set a debug-only flag indicating this "multi" structure never be
					 * treated as a GOOD_TN in forward processing. This will be checked there.
					 */
					DEBUG_ONLY(multi->this_is_broken = TRUE;)
				}
			} else	/* TCOM/ZTCOM */
				MUR_TCOM_TOKEN_PROCESSING(jctl, jjctl, token, rec_image_count,
									rec_time, rec_fence, regno, reg_total, jnlrec);
		} else if (mur_options.rollback && IS_REPLICATED(rectype))
		{	/* Process unfenced transactions. They are either lost or good.
			 * For RESYNC and FETCH_RESYNC qualifiers, all non-tp transactions
			 * 	at or after murgbl.resync_seqno are considered lost.
			 * So, we do not need to add them in token(seqnum) table to find gap in sequence number.
			 * For consistent rollback murgbl.resync_seqno == 0 and we want to consider all records
			 *	till tp_resolve_time for broken/lost/good determination so check accordingly..
			 */
			skip_rec = (murgbl.resync_seqno && (rec_token_seq > murgbl.resync_seqno));
			UNIX_ONLY(
				assert(!murgbl.resync_strm_seqno_nonzero || !mur_options.forward);
				if (!skip_rec && murgbl.resync_strm_seqno_nonzero)
				{
					assert(IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype) || (JRT_NULL == (rectype)));
					assert(&jnlrec->jrec_set_kill.strm_seqno == &jnlrec->jrec_null.strm_seqno);
					/* strm_seqno & strm_idx have already been initialized before for this record.
					 * Assert that (i.e. they have not been changed since then) before using them.
					 */
					DEBUG_ONLY(save_strm_seqno = GET_STRM_SEQNO(jnlrec);)
					assert(strm_idx == GET_STRM_INDEX(save_strm_seqno));
					assert(strm_seqno == GET_STRM_SEQ60(save_strm_seqno));
					skip_rec = (murgbl.resync_strm_seqno[strm_idx]
							&& (strm_seqno > murgbl.resync_strm_seqno[strm_idx]));
				}
			)
			if (!skip_rec)
			{
				rec_fence = GET_REC_FENCE_TYPE(rectype);
				assert(token == ((struct_jrec_upd *)jnlrec)->token_seq.token);
				/* For rollback, pid/image_type/time are not necessary to establish uniqueness of token
				 * as token (which is a seqno) is already guaranteed to be unique for an instance.
				 */
				if (NULL == (multi = MUR_TOKEN_LOOKUP(token, 0, 0, rec_fence)))
				{	/* We reuse same token table. Most of the fields in multi_struct are unused */
					MUR_TOKEN_ADD(multi, token, 0, 0, 1, rec_fence, 0);
				} else
				{
					assert(FALSE);
					if (!(mur_report_error(jctl, MUR_DUPTOKEN)))
					{
						*jjctl = jctl;
						return ERR_DUPTOKEN;
					}
				}
			}
		}
	}
	PRINT_VERBOSE_STAT(jctl, "mur_back_processing:at the end");
	assert((SS_NORMAL != status) || !mur_options.rollback || this_reg_resolved);
	if (SS_NORMAL != status)
	{
		if (!mur_options.forward)
		{
			if (ERR_NOPREVLINK == status)
			{	/* We check if there is an EPOCH with a time EQUAL to the tp_resolve_time. If so we
				 * try not to issue the NOPREVLINK error for this boundary condition.
				 */
				assert(JNL_HDR_LEN ==  jctl->rec_offset);
				if (rec_time <= jgbl.mur_tp_resolve_time)
				{
					jctl->rec_offset = JNL_HDR_LEN + PINI_RECLEN;
					status = mur_prev(jctl, jctl->rec_offset);
					if (SS_NORMAL != status)
					{
						*jjctl = jctl;
						return status;
					}
					jnlrec = mur_desc->jnlrec;
					rectype = (enum jnl_record_type)jnlrec->prefix.jrec_type;
					rec_time = jnlrec->prefix.time;
					rec_token_seq = GET_JNL_SEQNO(jnlrec);
					assert(JRT_EPOCH == rectype);
					/* handle non-epoch (out-of-design) situation in pro nevertheless */
					reached_trnarnd = (JRT_EPOCH == rectype)
							&& (!murgbl.resync_seqno || (rec_token_seq <= murgbl.resync_seqno));
					UNIX_ONLY(
						if (reached_trnarnd && murgbl.resync_strm_seqno_nonzero)
						{	/* Check if any stream seqnos need to be compared as well */
							if (!rctl->recov_interrupted)
							{	/* For non-interupted recovery, one stream needs checking */
								idx = murgbl.resync_strm_index;
								if (INVALID_SUPPL_STRM != idx)
								{
									assert((0 <= idx) && (MAX_SUPPL_STRMS > idx));
									strm_seqno = jnlrec->jrec_epoch.strm_seqno[idx];
									if (strm_seqno > murgbl.resync_strm_seqno[idx])
										reached_trnarnd = FALSE;
								}
							} else
							{	/* For interupted recovery, upto 16 streams need checking */
								for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
								{
									strm_seqno = jnlrec->jrec_epoch.strm_seqno[idx];
									if (strm_seqno > murgbl.resync_strm_seqno[idx])
									{
										reached_trnarnd = FALSE;
										break;
									}
								}
							}
						}
					)
				} else
					reached_trnarnd = FALSE;
				if (reached_trnarnd)
				{
					if (mur_options.rollback && !this_reg_resolved)
					{
						SAVE_PRE_RESOLVE_SEQNO(rectype, rec_time, rec_token_seq, pre_resolve_seqno);
						this_reg_resolved = TRUE;
					}
					save_turn_around_point(rctl, jctl, apply_pblk_this_region);
				} else
				{
					gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(4) ERR_NOPREVLINK,
						2, jctl->jnl_fn_len, jctl->jnl_fn);
					*jjctl = jctl;
					return ERR_NOPREVLINK;
				}
			} else	/* mur_read_file should have issued messages as necessary */
			{
				*jjctl = jctl;
				return status;
			}
		} else if (ERR_JNLREADBOF != status)	/* mur_read_file should have issued messages */
		{
			*jjctl = jctl;
			return status;
		}
		/* for mur_options.forward ERR_JNLREADBOF is not error but others are */
	}
	if (!mur_options.forward && (NULL == rctl->jctl_turn_around))
		GTMASSERT;
	return SS_NORMAL;
}

/*	This routine performs backward processing for forward and backward recover/rollback.
 *	This creates list of tokens for broken fenced transactions.
 *	For noverify qualifier in backward recovry, it may apply PBLK calling "mur_output_pblk"
 */
uint4 mur_back_processing(jnl_ctl_list **jjctl, boolean_t apply_pblk, seq_num *pre_resolve_seqno, jnl_tm_t alt_tp_resolve_time)
{
	enum jnl_record_type		rectype;
	file_control			*fc;
	int				idx, regno, reg_total, status;
	jnl_ctl_list			*jctl;
	jnl_record			*jnlrec;
	jnl_tm_t			max_lvrec_time, min_bov_time;
	mur_back_opt_t			mur_back_options;
	mur_read_desc_t			*mur_desc;
	reg_ctl_list			*rctl, *rctl_top;
	seq_num				rec_token_seq, save_resync_seqno, strm_seqno;
	sgmnt_data_ptr_t		csd;

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
	if (0 != alt_tp_resolve_time && alt_tp_resolve_time < jgbl.mur_tp_resolve_time)
		jgbl.mur_tp_resolve_time = alt_tp_resolve_time;
	if (!mur_options.forward && mur_options.update)
	{
		/* Save murgbl.resync_seqno before it gets modified just in case we needed the original value for debugging */
		DEBUG_ONLY(murgbl.save_resync_seqno = murgbl.resync_seqno;)
		/* Following for loop code block does the same thing for every call to "mur_back_processing".
		 * Tail corruption in journal could cause multiple calls to this routine but that case should be very rare.
		 * So let's keep it here instead of moving to mur_back_process.
		 */
		for (rctl = mur_ctl, rctl_top = mur_ctl + reg_total; rctl < rctl_top; rctl++)
		{
			csd = rctl->csd;
			assert(NULL != csd);
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
			if (rctl->recov_interrupted)
			{
				if (csd->intrpt_recov_resync_seqno)
				{
					assert(mur_options.rollback);	/* otherwise we would have issued a ERR_ROLLBKINTERRUPT
									 * error in mur_open_files.c
									 */
					if ((0 == murgbl.resync_seqno) || (csd->intrpt_recov_resync_seqno < murgbl.resync_seqno))
						murgbl.resync_seqno = csd->intrpt_recov_resync_seqno;
				}
				UNIX_ONLY(
					for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
					{
						strm_seqno = csd->intrpt_recov_resync_strm_seqno[idx];
						if (strm_seqno)
						{
							assert(mur_options.rollback);	/* otherwise we would have issued a
											 * ERR_ROLLBKINTERRUPT error in
											 * mur_open_files.c
											 */
							if ((0 == murgbl.resync_strm_seqno[idx])
									|| (strm_seqno < murgbl.resync_strm_seqno[idx]))
							{
								murgbl.resync_strm_seqno[idx] = strm_seqno;
								murgbl.resync_strm_seqno_nonzero = TRUE;
							}
						}
					}
				)
			}
		}
		if (murgbl.resync_seqno)
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_RESOLVESEQNO, 2, &murgbl.resync_seqno, &murgbl.resync_seqno);
		UNIX_ONLY(
			if (murgbl.resync_strm_seqno_nonzero)
			{
				for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
				{
					if (murgbl.resync_strm_seqno[idx])
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_RESOLVESEQSTRM, 3, idx,
							&murgbl.resync_strm_seqno[idx], &murgbl.resync_strm_seqno[idx]);
				}
				/* If -resync=<strm_seqno> is specified, we dont yet know what jnl_seqno it maps back to.
				 * To facilitate that determination, set resync_seqno to maximum possible value. It will
				 * be adjusted below based on the records we see in backward processing.
				 */
				if (!murgbl.resync_seqno)
					murgbl.resync_seqno = MAXUINT8;
			}
		)
		for (rctl = mur_ctl, rctl_top = mur_ctl + reg_total; rctl < rctl_top; rctl++)
		{
			csd = rctl->csd;
			assert(csd->recov_interrupted);	/* mur_open_files set this */
			if (apply_pblk && !rctl->jfh_recov_interrupted)
			{	/* When the 'if' condition is TRUE, we apply PBLKs in mur_back_process.
				 * Store the jgbl.mur_tp_resolve_time/murgbl.resync_seqno.
				 * So we remember to undo PBLKs at least upto that point,
				 * in case this recovery is interrupted/crashes.
				 */
				assert(0 == iterationcnt || csd->intrpt_recov_tp_resolve_time >= jgbl.mur_tp_resolve_time);
				csd->intrpt_recov_tp_resolve_time = jgbl.mur_tp_resolve_time;
				assert(0 == iterationcnt || (csd->intrpt_recov_resync_seqno == murgbl.resync_seqno));
				assert(!csd->intrpt_recov_resync_seqno || (csd->intrpt_recov_resync_seqno >= murgbl.resync_seqno));
				csd->intrpt_recov_resync_seqno = murgbl.resync_seqno;
				UNIX_ONLY(
					assert(!murgbl.resync_strm_seqno_nonzero || rctl->recov_interrupted
						|| (INVALID_SUPPL_STRM == idx) || (-1 == iterationcnt)
						|| ((0 == iterationcnt) && !strm_seqno)
						|| (iterationcnt && (strm_seqno == murgbl.resync_strm_seqno[idx])));
				)
				MUR_SAVE_RESYNC_STRM_SEQNO(rctl, csd);
				/* flush the changed csd to disk */
				fc = rctl->gd->dyn.addr->file_cntl;
				fc->op = FC_WRITE;
				fc->op_buff = (sm_uc_ptr_t)csd;
				fc->op_len = (int)ROUND_UP(SIZEOF_FILE_HDR(csd), DISK_BLOCK_SIZE);
				fc->op_pos = 1;
				dbfilop(fc);
			}
		}
	} /* end else !mur_options.forward */
	DEBUG_ONLY(iterationcnt++;)
	*pre_resolve_seqno = 0;
	save_resync_seqno = murgbl.resync_seqno;
	assert(murgbl.ok_to_update_db == apply_pblk);
	/* At this point we have computed jgbl.mur_tp_resolve_time. It is the time upto which (at least)
	 * we need to do token resolution. This is for all kinds of recovery and rollback.
	 * Following for loop will do backward processing and resolve token up to this jgbl.mur_tp_resolve_time.
	 * (For recover with lower since_time, we already set jgbl.mur_tp_resolve_time as since_time.
	 * For interrupted recovery we also considered previous recovery's jgbl.mur_tp_resolve_time.)
	 * For rollback command (with resync or fetchresync qualifier) we resolve only upto jgbl.mur_tp_resolve_time.
	 */
	for (regno = 0, rctl = mur_ctl, rctl_top = mur_ctl + reg_total; rctl < rctl_top; rctl++, regno++)
	{
		/* Note that for rctl->jfh_recov_interrupted we do not apply pblks in this routine */
		jctl = rctl->jctl;
		assert(jctl->reg_ctl == rctl);
		assert(NULL == jctl->next_gen);
		if (mur_options.verbose)
			gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(6) ERR_MUINFOSTR, 4,
				LEN_AND_LIT("Processing started for journal file"), jctl->jnl_fn_len, jctl->jnl_fn);
		jctl->rec_offset = jctl->lvrec_off;
		status = mur_prev(jctl, jctl->rec_offset);
		mur_desc = rctl->mur_desc;
		jnlrec = mur_desc->jnlrec;
		if (!mur_options.forward && FENCE_NONE != mur_options.fences)
		{ 	/* This is for the latest generation only */
			rectype = (enum jnl_record_type)jnlrec->prefix.jrec_type;
			if (JRT_EOF != rectype)
			{ 	/* When a region is inactive but not closed, that is, no logical updates are done for some
				 * period of time (8 second), then EPOCH is written by periodic timer. However, for some
				 * existing bug/issue periodic timers can be deferred for long period of time.
				 * So we need this check here.
				 */
				for ( ; ; )
				{
					if ((JRT_PFIN == rectype) || (JRT_ALIGN == rectype) || (JRT_INCTN == rectype))
					{
						if (JRT_INCTN == rectype)
							MUR_INCTN_BLKS_TO_UPGRD_ADJUST(rctl);
						if (SS_NORMAL == (status = mur_prev(jctl, 0)))
						{
							jnlrec = mur_desc->jnlrec;	/* keep jnlrec uptodate */
							jctl->rec_offset -= mur_desc->jreclen;
							assert(jctl->rec_offset >= mur_desc->cur_buff->dskaddr);
							assert(JNL_HDR_LEN <= jctl->rec_offset);
							rectype = (enum jnl_record_type)jnlrec->prefix.jrec_type;
						} else
							break;
					} else
						break;
				}
				if (SS_NORMAL == status && (JRT_EPOCH != rectype)
						&& (jnlrec->prefix.time < jgbl.mur_tp_resolve_time))
				{
					*jjctl = jctl;
#					ifdef UNIX
					/* Assert that the new about-to-be-set TP resolve time does not differ by more than the
					 * twice the free-EPOCH interval (which is defined by TIM_DEFER_SYNC). Twice is not a magic
					 * number, but just to allow for some relaxation. The only exception is if this is an
					 * interrupted recovery in which case the difference could be significant. One reason we
					 * know why this could happen is because mur_close_files calls gds_rundown on all regions
					 * AFTER resetting csd->intrpt_recov_tp_resolve_time to 0. So, if we get killed at
					 * right AFTER doing gds_rundown on one region, but BEFORE doing gds_rundown on other
					 * regions, then a subsequent ROLLBACK finds a higher TP resolve time on one region and
					 * sets the value to jgbl.mur_tp_resolve_time but later finds other regions with records
					 * having timestamps less than jgbl.mur_tp_resolve_time. See GTM-7204 for more details.
					 */
					assert(((TIM_DEFER_DBSYNC * 2) >= (jgbl.mur_tp_resolve_time - jnlrec->prefix.time))
							|| ((WBTEST_CRASH_SHUTDOWN_EXPECTED  == gtm_white_box_test_case_number)
								&& murgbl.intrpt_recovery));
#					endif
					return ERR_CHNGTPRSLVTM;
				}
			}
		}
		/* Do intializations before invoking "mur_back_processing_one_region" function */
		jctl->after_end_of_data = TRUE;
		mur_back_options.jctl = jctl;
		mur_back_options.rec_token_seq = MAXUINT8;
		mur_back_options.first_epoch = TRUE;
		mur_back_options.status = status;
		mur_back_options.jjctl = jjctl;
		mur_back_options.apply_pblk = apply_pblk;
		mur_back_options.pre_resolve_seqno = pre_resolve_seqno;
		status = mur_back_processing_one_region(&mur_back_options);
		if (SS_NORMAL != status)
			return status;
	} /* end rctl for loop */
	if (save_resync_seqno != murgbl.resync_seqno)
	{	/* murgbl.resync_seqno was adjusted in the middle of backward processing due to a -rsync_strm= specification.
		 * Check if any regions have to be further involved in backward processing. This is necessary because we might
		 * have stopped the first backward processing on seeing an EPOCH record whose strm_seqno is less than or equal
		 * to the input resync strm_seqno. But it is possible that murgbl.resync_seqno was initially at a higher value
		 * when a particular region stopped its backward processing but later got adjusted to a lower value during
		 * processing for the next region. In that case, we should redo processing for the first region with the new
		 * murgbl.resync_seqno in case this takes us back to a previous epoch record. <C9J02_003091_strm_seqno_rollback>
		 */
		assert(murgbl.resync_seqno < save_resync_seqno);
		assert(mur_options.rollback);
		UNIX_ONLY(assert(murgbl.resync_strm_seqno_nonzero);)
		JNL_PUT_MSG_PROGRESS("Backward processing Round-II started");
		for (regno = 0, rctl = mur_ctl, rctl_top = mur_ctl + reg_total; rctl < rctl_top; rctl++, regno++)
		{
			jctl = rctl->jctl_turn_around;
			/* Check if this regions turn-around-point-seqno is higher than the final value of murgbl.resync_seqno.
			 * If so, we need to do further backward processing on this region.
			 */
			if (jctl->turn_around_seqno > murgbl.resync_seqno)
			{	/* Do intializations before invoking "mur_back_processing_one_region" function */
				/* jctl->after_end_of_data is already set from previous invocation of this function */
				/* rctl->mur_desc already points to the turnaround point so no further adjustment needed */
				mur_back_options.jctl = rctl->jctl_turn_around;
				mur_desc = rctl->mur_desc;
				jnlrec = mur_desc->jnlrec;
				assert(JRT_EPOCH == jnlrec->prefix.jrec_type);
				assert(jctl->turn_around_time == jnlrec->prefix.time);
				assert(jctl->turn_around_seqno == jnlrec->jrec_epoch.jnl_seqno);
				assert(jctl->turn_around_tn == jnlrec->prefix.tn);
				assert(jctl->rec_offset == jctl->turn_around_offset);
				/* Now that jctl->rec_offset points to the same offset as jctl->turn_around_offset, reset
				 * the latter as a lot of the code inside "mur_back_processing_one_region" relies on this.
				 */
				jctl->turn_around_offset = 0;
				/* By a similar token, reset "rctl->jctl_turn_around" as later asserts rely on this
				 * and we have already stored this in mur_back_options.jctl.
				 */
				rctl->jctl_turn_around = NULL;
				mur_back_options.rec_token_seq = GET_JNL_SEQNO(jnlrec);
				mur_back_options.first_epoch = FALSE;	/* since we have already seen at least one EPOCH in
									 * previous invocation of "mur_back_processing_one_region"
									 */
				mur_back_options.status = SS_NORMAL;
				mur_back_options.jjctl = jjctl;
				mur_back_options.apply_pblk = apply_pblk;
				mur_back_options.pre_resolve_seqno = pre_resolve_seqno;
				status = mur_back_processing_one_region(&mur_back_options);
				if (SS_NORMAL != status)
					return status;
			}
		}
	}
	/* Since jgbl.mur_tp_resolve_time is one resolve time for all regions, no implicit lookback processing
	 * to resolve transactions is necessary */
	*jjctl = NULL;
	return SS_NORMAL;
}
