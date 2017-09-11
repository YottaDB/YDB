/****************************************************************
 *								*
 * Copyright (c) 2010-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gvnh_spanreg.h"
#include "gtmimagename.h"
#include "gv_trigger_common.h"	/* for *HASHT* macros used inside GVNH_REG_INIT macro */
#include "gtmcrypt.h"

GBLREF	gd_addr			*gd_header;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	mur_gbls_t		murgbl;
GBLREF	mur_opt_struct		mur_options;
GBLREF	uint4			dollar_tlevel;
GBLREF	jnl_gbls_t		jgbl;
#ifdef DEBUG
GBLREF	boolean_t		forw_recov_lgtrig_only;
#endif

error_def(ERR_DUPTN);
error_def(ERR_FORCEDHALT);
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
	boolean_t		is_set_kill_zkill_ztworm_lgtrig_ztrig, is_set_kill_zkill_ztrig;
	trans_num		curr_tn;
	enum jnl_record_type	rectype;
	enum rec_fence_type	rec_fence;
	enum broken_type	recstat;
	jnl_tm_t		rec_time;
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
	pini_list_struct	*plst;
	int4			gtmcrypt_errno;
	boolean_t		use_new_key;
	forw_multi_struct	*forw_multi;
#	if (defined(DEBUG) && defined(UNIX))
	int4			strm_idx;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!rctl->forw_eof_seen);
	if (multi_proc_in_use)
	{	/* Set key to print this rctl's region-name as prefix in case this forked off process prints any output.
		 * e.g. If this function ends up calling t_end/op_tcommit which in turn needs to do a jnl autoswitch
		 * inside jnl_file_extend and prints a GTM-I-FILERENAME message.
		 */
		MUR_SET_MULTI_PROC_KEY(rctl, multi_proc_key);
	}
	jctl = rctl->jctl;
	/* Ensure we never DOUBLE process the same journal record in the forward phase */
	assert((jctl != rctl->last_processed_jctl) || (jctl->rec_offset != rctl->last_processed_rec_offset));
#	ifdef DEBUG
	rctl->last_processed_jctl = jctl;
	rctl->last_processed_rec_offset = jctl->rec_offset;
#	endif
	rec = rctl->mur_desc->jnlrec;
	rectype = (enum jnl_record_type)rec->prefix.jrec_type;
	rec_time = rec->prefix.time;
	assert(rec_time <= mur_options.before_time);
	assert(rec_time >= mur_options.after_time);
	assert((0 == mur_options.after_time) || (mur_options.forward && !rctl->db_updated));
	is_set_kill_zkill_ztworm_lgtrig_ztrig = (boolean_t)(IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype));
	if (is_set_kill_zkill_ztworm_lgtrig_ztrig)
	{
		keystr = (jnl_string *)&rec->jrec_set_kill.mumps_node;
		if (USES_ANY_KEY(jctl->jfh))
		{
			use_new_key = USES_NEW_KEY(jctl->jfh);
			/* Note: JRT_ALIGN does not have a "prefix.tn" field. But in that case we would not have come in the "if" */
			assert((JRT_ALIGN != rectype) && NEEDS_NEW_KEY(jctl->jfh, rec->prefix.tn) == use_new_key);
			MUR_DECRYPT_LOGICAL_RECS(
					keystr,
					(use_new_key ? TRUE : jctl->jfh->non_null_iv),
					rec->prefix.forwptr,
					(use_new_key ? jctl->encr_key_handle2 : jctl->encr_key_handle),
					gtmcrypt_errno);
			if (0 != gtmcrypt_errno)
			{
				GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, gtm_putmsg, jctl->jnl_fn_len, jctl->jnl_fn);
				return gtmcrypt_errno;
			}
		}
	}
	if (mur_options.selection && !mur_select_rec(jctl))
		return SS_NORMAL;
	rec_token_seq = (REC_HAS_TOKEN_SEQ(rectype)) ? GET_JNL_SEQNO(rec) : 0;
	process_losttn = rctl->process_losttn;
	if (!process_losttn && mur_options.rollback)
	{
		if (IS_REPLICATED(rectype) && (rec_token_seq >= murgbl.losttn_seqno))
			process_losttn = rctl->process_losttn = TRUE;
#		if (defined(UNIX) && defined(DEBUG))
		if ((rec_token_seq < murgbl.losttn_seqno) && murgbl.resync_strm_seqno_nonzero && IS_REPLICATED(rectype))
		{
			assert(IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype) || IS_COM(rectype) || (JRT_NULL == (rectype)));
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
#			ifdef DEBUG
			/* assert that all TP records before min_broken_time are not broken */
			if (IS_TP(rectype) && ((!mur_options.rollback && rec_time < murgbl.min_broken_time)
						|| (mur_options.rollback && rec_token_seq < murgbl.min_broken_seqno)))
			{
				if (NULL != (multi = MUR_TOKEN_LOOKUP(rec_token_seq, rec_time, TPFENCE)))
				{
					assert(0 == multi->partner);
					assert(FALSE == multi->this_is_broken);
				}
			}
#			endif
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
				rec_fence = (IS_TP(rectype) ? TPFENCE : ZTPFENCE);
				assert(rec_token_seq == ((struct_jrec_upd *)rec)->token_seq.token);
				multi = MUR_TOKEN_LOOKUP(rec_token_seq, rec_time, rec_fence);
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
		} else if ((FENCE_ALWAYS == mur_options.fences) && is_set_kill_zkill_ztworm_lgtrig_ztrig)
		{
			process_losttn = rctl->process_losttn = TRUE;
			recstat = BROKEN_TN;
		}
	} else
		forw_multi = NULL;
	if (mur_options.show)
	{
		assert(SS_NORMAL == status);
		/* Note that ALIGN records do not have a pini_addr field. So skip the "mur_pini_state" call in that case */
		if (BROKEN_TN != recstat)
		{
			if (JRT_PFIN == rectype)
				status = mur_pini_state(jctl, rec->prefix.pini_addr, FINISHED_PROC);
			else if ((JRT_EOF != rectype) && (JRT_ALIGN != rectype))
				status = mur_pini_state(jctl, rec->prefix.pini_addr, ACTIVE_PROC);
		} else if (JRT_ALIGN != rectype)
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
			gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(6) ERR_JNLTPNEST, 4, jctl->jnl_fn_len,
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
			DEBUG_ONLY(forw_recov_lgtrig_only = TRUE;)	/* gets reset later if a non-LGTRIG record is seen */
		}
		tp_set_sgm();	/* needed to set "sgm_info_ptr" to correspond to "rctl" */
	}
	/* For extract, if database was present we would have done gvcst_init().
	 * For recover/rollback gvcst_init() should definitely have been done.
	 * In both cases rctl->gd->open will be non-NULL. Note that rctl->csa could be non-NULL
	 * (set in mur_forward) even if rctl->gd->open is non-NULL. So don't use that.
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
		{	/* find out collation of key in the jnl-record from the database corresponding to the jnl file */
			gvent.var_name.addr = (char *)gv_currkey->base;
			gvent.var_name.len = STRLEN((char *)gv_currkey->base);
			COMPUTE_HASH_MNAME(&gvent);
			if (NULL != (tabent = lookup_hashtab_mname(&rctl->gvntab, &gvent)))	/* WARNING ASSIGNMENT */
			{
				gvnh_reg = (gvnh_reg_t *)tabent->value;
				assert(NULL != gvnh_reg);
				gv_target = gvnh_reg->gvt;
				gv_cur_region = gvnh_reg->gd_reg;
				assert(gv_cur_region->open);
			} else
			{
				assert(IS_REG_BG_OR_MM(gv_cur_region));
				gv_target = (gv_namehead *)targ_alloc(gv_cur_region->max_key_size, &gvent, gv_cur_region);
				GVNH_REG_INIT(gd_header, &rctl->gvntab, NULL, gv_target, gv_cur_region, gvnh_reg, tabent);
			}
			if (!TREF(jnl_extract_nocol))
				GVCST_ROOT_SEARCH;
		}
	}
	if (GOOD_TN == recstat)
	{
		if ((is_set_kill_zkill_ztworm_lgtrig_ztrig && !IS_TP(rectype)) || (JRT_TCOM == rectype))
		{
			/* Do forward journaling, detecting operations with duplicate transaction numbers.  While doing
			 * journaling on a database, a process may be killed immediately after updating (or partially
			 * updating) the journal file, but before the database gets updated.  Since the transaction was
			 * never fully committed, the database transaction number has not been updated, and the last
			 * journal record does not reflect the actual state of the database. One would think the next
			 * process to update the database will write a journal record with the same transaction number as
			 * the previous record. But before writing the journal record, it would do a "grab_crit" which
			 * will notice the previous crit holder having been killed and will salvage crit and as part of
			 * that would have set wc_blocked and invoked "wcs_recover" which would have written an INCTN
			 * record to bump the curr_tn. Therefore it is not possible for two logical records corresponding
			 * to different transactions (non-tp or tp) to have the same rec_tn value. Therefore an ERR_DUPTN
			 * is not possible in normal usage.  But just in case, let us recognize this and issue a DUPTN
			 * warning so the user knows this was encountered during the recovery.  Note: DUPTN is possible
			 * with -NOTNCHECK (if two identical jnl files with just one tn is presented to forward recovery
			 * like the v54003/C9K08003315 subtest does) so ignore that.
			 */
			curr_tn = rec->prefix.tn;
			if ((rctl->last_tn == curr_tn) && !mur_options.notncheck)
			{
				assert(FALSE); /* We want to debug this */
				murgbl.wrn_count++;
				gtm_putmsg_csa(CSA_ARG(rctl->csa)
					VARLSTCNT(6) ERR_DUPTN, 4, &curr_tn, jctl->rec_offset, jctl->jnl_fn_len, jctl->jnl_fn);
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
			assert(!mur_options.rollback || !IS_REPLICATED(rectype) || (rec_token_seq < murgbl.losttn_seqno));
			if (SS_NORMAL != (status = mur_output_record(rctl))) /* updates murgbl.consist_jnl_seqno */
				return status;
			assert(!mur_options.rollback || (murgbl.consist_jnl_seqno <= murgbl.losttn_seqno));
		}
	}
	if ((GOOD_TN != recstat) || jgbl.mur_extract)
	{
		if (!rctl->extr_file_created[recstat])
		{	/* Before creating a persistent file, check if parent is still alive. If not return right away
			 * with abnormal status (parent is not there to clean up this file anyways).
			 */
			if (multi_proc_in_use && multi_proc_shm_hdr->parent_pid != getppid())
			{
				SET_FORCED_MULTI_PROC_EXIT;	/* Also signal sibling children to stop processing */
				return ERR_FORCEDHALT;
			}
			if (SS_NORMAL != (status = mur_cre_file_extfmt(jctl, recstat)))
				return status;
		}
		/* extract "rec" using routine "extraction_routine[rectype]" into broken transaction file */
		/* Use GET_JREC_PINI_ADDR macro instead of "rec->prefix.pini_addr" to account for JRT_ALIGN rectype */
		status = mur_get_pini(jctl, GET_JREC_PINI_ADDR(rec, rectype), &plst);
		if (SS_NORMAL == status)
			(*extraction_routine[rectype])(jctl, recstat, rec, plst);
		else
			return status;
	}
	return SS_NORMAL;
}
