/****************************************************************
 *								*
 *	Copyright 2010, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h> /* for offsetof() macro */

#include "gtm_time.h"
#include "gtm_string.h"
#include "min_max.h"
#ifdef VMS
#include <rms.h>
#include <devdef.h>
#include <ssdef.h>
#endif

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "mur_jnl_ext.h"
#include "iosp.h"
#include "gtmmsg.h"
#include "op.h"
#include "mu_gv_stack_init.h"
#include "targ_alloc.h"
#include "tp_change_reg.h"
#include "gvcst_protos.h"	/* for gvcst_root_search prototype */
#include "tp_set_sgm.h"
#include "tp_frame.h"
#include "wbox_test_init.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif

GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF  gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF 	mur_gbls_t		murgbl;
GBLREF	mur_opt_struct		mur_options;
GBLREF	uint4			dollar_tlevel;
GBLREF 	jnl_gbls_t		jgbl;

error_def(ERR_DUPTN);
error_def(ERR_JNLTPNEST);

static	void	(* const extraction_routine[])() =
{
#define JNL_TABLE_ENTRY(rectype, extract_rtn, label, update, fixed_size, is_replicated)	extract_rtn,
#include "jnl_rec_table.h"
#undef JNL_TABLE_ENTRY
};

uint4	mur_forward_play_cur_jrec(reg_ctl_list *rctl)
{
	boolean_t		process_losttn;
	boolean_t		is_set_kill_zkill_ztrig_ztworm, is_set_kill_zkill_ztrig, added;
	trans_num		curr_tn;
	enum jnl_record_type	rectype;
	enum rec_fence_type	rec_fence;
	enum broken_type	recstat;
	jnl_tm_t		rec_time;
	int4			rec_image_count = 0;	/* This is a dummy variable for UNIX */
	uint4			status;
	mval			mv;
	seq_num 		rec_token_seq, rec_strm_seqno, resync_strm_seqno;
	jnl_record		*rec;
	jnl_string		*keystr;
	multi_struct 		*multi;
	jnl_ctl_list		*jctl;
	ht_ent_mname		*tabent;
	mname_entry	 	gvent;
	gvnh_reg_t		*gvnh_reg;
#	ifdef GTM_CRYPT
	int4			gtmcrypt_errno;
#	endif
	forw_multi_struct	*forw_multi;
#	if (defined(DEBUG) && defined(UNIX))
	int4			strm_idx;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!rctl->forw_eof_seen);
	jctl = rctl->jctl;
	/* Ensure we never DOUBLE process the same journal record in the forward phase */
	assert((jctl != rctl->last_processed_jctl) || (jctl->rec_offset != rctl->last_processed_rec_offset));
	DEBUG_ONLY(
		rctl->last_processed_jctl = jctl;
		rctl->last_processed_rec_offset = jctl->rec_offset;
	)
	rec = rctl->mur_desc->jnlrec;
	rectype = (enum jnl_record_type)rec->prefix.jrec_type;
	rec_time = rec->prefix.time;
	assert(rec_time <= mur_options.before_time);
	assert(rec_time >= mur_options.after_time);
	assert((0 == mur_options.after_time) || mur_options.forward && !rctl->db_updated);
	is_set_kill_zkill_ztrig_ztworm = (boolean_t)(IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype));
	if (is_set_kill_zkill_ztrig_ztworm)
	{
		keystr = (jnl_string *)&rec->jrec_set_kill.mumps_node;
#		ifdef GTM_CRYPT
		if (jctl->jfh->is_encrypted)
		{
			MUR_DECRYPT_LOGICAL_RECS(keystr, rec->prefix.forwptr, jctl->encr_key_handle, gtmcrypt_errno);
			if (0 != gtmcrypt_errno)
			{
				GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, jctl->jnl_fn_len, jctl->jnl_fn);
				return gtmcrypt_errno;
			}
		}
#		endif
	}
	if (mur_options.selection && !mur_select_rec(jctl))
		return SS_NORMAL;
	rec_token_seq = (REC_HAS_TOKEN_SEQ(rectype)) ? GET_JNL_SEQNO(rec) : 0;
	process_losttn = rctl->process_losttn;
	if (!process_losttn && mur_options.rollback)
	{
		if (rec_token_seq >= murgbl.losttn_seqno)
			process_losttn = rctl->process_losttn = TRUE;
#		if (defined(UNIX) && defined(DEBUG))
		if ((rec_token_seq < murgbl.losttn_seqno) && murgbl.resync_strm_seqno_nonzero && IS_REPLICATED(rectype))
		{
			assert(IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype) || IS_COM(rectype) || (JRT_NULL == (rectype)));
			assert(&rec->jrec_set_kill.strm_seqno == &rec->jrec_null.strm_seqno);
			assert(&rec->jrec_set_kill.strm_seqno == &rec->jrec_tcom.strm_seqno);
			rec_strm_seqno = GET_STRM_SEQNO(rec);
			if (rec_strm_seqno)
			{
				strm_idx = GET_STRM_INDEX(rec_strm_seqno);
				rec_strm_seqno = GET_STRM_SEQ60(rec_strm_seqno);
				resync_strm_seqno = murgbl.resync_strm_seqno[strm_idx];
				assert(!resync_strm_seqno || (rec_strm_seqno < resync_strm_seqno));
			}
		}
#		endif
	}
	/* Note: Broken transaction determination is done below only based on the records that got selected as
	 * part of the mur_options.selection criteria. Therefore depending on whether a broken transaction gets
	 * selected or not, future complete transactions might either go to the lost transaction or extract file.
	 */
	recstat = process_losttn ? LOST_TN : GOOD_TN;
	status = SS_NORMAL;
	if (FENCE_NONE != mur_options.fences)
	{
		if (IS_FENCED(rectype))
		{
			assert(rec_token_seq);
			DEBUG_ONLY(
				/* assert that all TP records before min_broken_time are not broken */
				if (IS_TP(rectype) &&
					((!mur_options.rollback && rec_time < murgbl.min_broken_time) ||
					  (mur_options.rollback && rec_token_seq < murgbl.min_broken_seqno)))
				{
					VMS_ONLY(
						MUR_GET_IMAGE_COUNT(jctl, rec, rec_image_count, status);
						assert(SS_NORMAL == status);
					)
					rec_fence = GET_REC_FENCE_TYPE(rectype);
					if (NULL != (multi = MUR_TOKEN_LOOKUP(rec_token_seq,
						rec_image_count, rec_time, rec_fence)))
					{
						assert(0 == multi->partner);
						assert(FALSE == multi->this_is_broken);
					}
				}
			)
			/* In most cases, the fact whether a TP tn is broken or not would have been determined already in
			 * mur_forward. In this case, rctl->forw_multi would be set appropriately. So use that to get to
			 * "multi" and avoid a hashtable lookup. If forw_multi is NULL (e.g. for ZTP or single-region TP),
			 * the hash-table lookup cannot be avoided.
			 */
			multi = NULL;
			forw_multi = rctl->forw_multi;
			if (NULL != forw_multi)
			{
				multi = forw_multi->multi;
				/* Always honor the "recstat" from the forw_multi since that has been determined taking into
				 * consideration the BROKEN_TN status of ALL participating regions.
				 */
				assert((GOOD_TN != forw_multi->recstat) || (GOOD_TN == recstat));
				recstat = forw_multi->recstat;
			} else if (IS_REC_POSSIBLY_BROKEN(rec_time, rec_token_seq))
			{
				assert(!mur_options.rollback || process_losttn);
				VMS_ONLY(
					MUR_GET_IMAGE_COUNT(jctl, rec, rec_image_count, status);
					if (SS_NORMAL != status)
						return status;
				)
				rec_fence = GET_REC_FENCE_TYPE(rectype);
				assert(rec_token_seq == ((struct_jrec_upd *)rec)->token_seq.token);
				multi = MUR_TOKEN_LOOKUP(rec_token_seq, rec_image_count, rec_time, rec_fence);
				if ((NULL != multi) && (0 < multi->partner))
				{
					process_losttn = rctl->process_losttn = TRUE;
					recstat = BROKEN_TN;
				}
			}
			/* Check that if the hashtable reports a tn as GOOD, it better have had the same
			 * # of participants in the TCOM records across all the participating regions.
			 */
			assert((NULL == multi) || (BROKEN_TN == recstat) || (FALSE == multi->this_is_broken));
		} else if ((FENCE_ALWAYS == mur_options.fences) && is_set_kill_zkill_ztrig_ztworm)
		{
			process_losttn = rctl->process_losttn = TRUE;
			recstat = BROKEN_TN;
		}
	} else
		forw_multi = NULL;
	if (mur_options.show)
	{
		assert(SS_NORMAL == status);
		if (BROKEN_TN != recstat)
		{
			if (JRT_PFIN == rectype)
				status = mur_pini_state(jctl, rec->prefix.pini_addr, FINISHED_PROC);
			else if ((JRT_EOF != rectype)
					&& ((JRT_ALIGN != rectype) || (JNL_HDR_LEN != rec->prefix.pini_addr)))
			{	/* Note that it is possible that we have a PINI record followed by a PFIN record
				 * and later an ALIGN record with the pini_addr pointing to the original PINI
				 * record (see comment in jnl_write.c where pini_addr gets assigned to JNL_HDR_LEN)
				 * In this case we do not want the ALIGN record to cause the process to become
				 * ACTIVE although it has written a PFIN record. Hence the check above.
				 */
				status = mur_pini_state(jctl, rec->prefix.pini_addr, ACTIVE_PROC);
			}
		} else
			status = mur_pini_state(jctl, rec->prefix.pini_addr, BROKEN_PROC);
		if (SS_NORMAL != status)
			return status;	/* "mur_pini_state" failed due to bad pini_addr */
		++jctl->jnlrec_cnt[rectype];	/* for -show=STATISTICS */
	}
	if (!mur_options.update && !jgbl.mur_extract)
		return SS_NORMAL;
	if (murgbl.ok_to_update_db && IS_TUPD(rectype) && (GOOD_TN == recstat))
	{	/* Even for FENCE_NONE we apply fences. Otherwise a TUPD becomes UPD etc.
		 * If forw_multi is non-NULL, a multi-region TP transaction is being played as a SINGLE
		 * TP transaction across all the involved regions. Therefore only ONE op_tstart is done
		 * even though more than one TSET might be encountered. In this case, do not issue JNLTPNEST error.
		 */
		if (dollar_tlevel && (NULL == forw_multi))
		{
			assert(FALSE);
			murgbl.wrn_count++;
			gtm_putmsg(VARLSTCNT(6) ERR_JNLTPNEST, 4, jctl->jnl_fn_len,
				jctl->jnl_fn, jctl->rec_offset, &rec->prefix.tn);
			OP_TROLLBACK(0);
		}
		if (!dollar_tlevel)
		{	/* Note: op_tstart resets gv_currkey. So set gv_currkey later. */
			/* mv is used to determine transaction id. But it is ignored by recover/rollback */
			mv.mvtype = MV_STR;
			mv.str.len = 0;
			mv.str.addr = NULL;
			op_tstart(IMPLICIT_TSTART, TRUE, &mv, -1);
			DEBUG_ONLY(jgbl.max_tp_ztp_jnl_upd_num = 0;)
		}
		tp_set_sgm();	/* needed to set "sgm_info_ptr" to correspond to "rctl" */
	}
	/* For extract, if database was present we would have done gvcst_init().
	 * For recover/rollback gvcst_init() should definitely have been done.
	 * In both cases rctl->gd->open will be non-NULL. Note that rctl->csa could be non-NULL
	 * (set in mur_forward) even if rctl->gd->open is non-NULL. So dont use that.
	 * Only then can we call gvcst_root_search() to find out collation set up for this global.
	 */
	assert(gv_cur_region == rctl->gd);
	assert(!mur_options.update || (gv_cur_region->open && (NULL != rctl->csa)));
	is_set_kill_zkill_ztrig = (boolean_t)(IS_SET_KILL_ZKILL_ZTRIG(rectype));
	if (is_set_kill_zkill_ztrig)
	{
		assert(NULL != keystr);
		memcpy(gv_currkey->base, &keystr->text[0], keystr->length);
		gv_currkey->base[keystr->length] = '\0';
		gv_currkey->end = keystr->length;
		if (gv_cur_region->open)
		{/* find out collation of key in the jnl-record from the database corresponding to the jnl file */
			gvent.var_name.addr = (char *)gv_currkey->base;
			gvent.var_name.len = STRLEN((char *)gv_currkey->base);
			COMPUTE_HASH_MNAME(&gvent);
			if ((NULL !=  (tabent = lookup_hashtab_mname(&rctl->gvntab, &gvent)))
				&& (NULL != (gvnh_reg = (gvnh_reg_t *)tabent->value)))
			{
				gv_target = gvnh_reg->gvt;
				gv_cur_region = gvnh_reg->gd_reg;
				assert(gv_cur_region->open);
			} else
			{
				assert(gv_cur_region->max_key_size <= MAX_KEY_SZ);
				gv_target = (gv_namehead *)targ_alloc(gv_cur_region->max_key_size,
					&gvent, gv_cur_region);
				gvnh_reg = (gvnh_reg_t *)malloc(SIZEOF(gvnh_reg_t));
				gvnh_reg->gvt = gv_target;
				gvnh_reg->gd_reg = gv_cur_region;
				if (NULL != tabent)
				{	/* Since the global name was found but gv_target was null and
					 * now we created a new gv_target, the hash table key must point
					 * to the newly created gv_target->gvname. */
					tabent->key = gv_target->gvname;
					tabent->value = (char *)gvnh_reg;
				} else
				{
					added = add_hashtab_mname(&rctl->gvntab, &gv_target->gvname,
							gvnh_reg, &tabent);
					assert(added);
				}
			}
			if (!TREF(jnl_extract_nocol))
				GVCST_ROOT_SEARCH;
		}
	}
	if (GOOD_TN == recstat)
	{
		if ((is_set_kill_zkill_ztrig_ztworm && !IS_TP(rectype)) || JRT_TCOM == rectype)
		{
			/* Do forward journaling, detecting operations with duplicate transaction numbers.
			 * While doing journaling on a database, a process may be killed immediately after
			 * updating (or partially updating) the journal file, but before the database gets
			 * updated.  Since the transaction was never fully committed, the database
			 * transaction number has not been updated, and the last journal record does not
			 * reflect the actual state of the database.  The next process to update the
			 * database writes a journal record with the same transaction number as the
			 * previous record.  While processing the journal file, we must recognize this and
			 * issue a DUPTN warning so the user knows this was encountered during the recovery.
			 */
			curr_tn = rec->prefix.tn;
			if (rctl->last_tn == curr_tn)
			{
				assert(FALSE); /* We want to debug this */
				murgbl.wrn_count++;
				gtm_putmsg(VARLSTCNT(6) ERR_DUPTN, 4, &curr_tn, jctl->rec_offset, jctl->jnl_fn_len, jctl->jnl_fn);
				if (dollar_tlevel)
				{
					assert(murgbl.ok_to_update_db);
					OP_TROLLBACK(0);
				}
			}
			rctl->last_tn = curr_tn;
		}
		if (murgbl.ok_to_update_db)
		{
			assert(!mur_options.rollback || (rec_token_seq < murgbl.losttn_seqno));
			if (SS_NORMAL != (status = mur_output_record(rctl))) /* updates murgbl.consist_jnl_seqno */
				return status;
			assert(!mur_options.rollback || (murgbl.consist_jnl_seqno <= murgbl.losttn_seqno));
		}
	}
	if (GOOD_TN != recstat || jgbl.mur_extract)
	{
		if (murgbl.extr_file_create[recstat])
		{
			if (SS_NORMAL != (status = mur_cre_file_extfmt(jctl, recstat)))
				return status;
			murgbl.extr_file_create[recstat] = FALSE;
		}
		/* extract "rec" using routine "extraction_routine[rectype]" into broken transaction file */
		EXTRACT_JNLREC(jctl, rec, extraction_routine[rectype], murgbl.file_info[recstat], status);
		if (SS_NORMAL != status)
			return status;
	}
	return SS_NORMAL;
}
