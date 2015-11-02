/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

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
#include "hashtab.h"
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

GBLREF	gv_key		*gv_currkey;
GBLREF	gv_namehead	*gv_target;
GBLREF  gd_region       *gv_cur_region;
GBLREF  int4		gv_keysize;
GBLREF  gv_key		*gv_altkey;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	sgmnt_data_ptr_t cs_data;
GBLREF	gd_region	*gv_cur_region;
GBLREF 	mur_gbls_t	murgbl;
GBLREF 	mur_rab_t	mur_rab;
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	jnl_ctl_list	*mur_jctl;
GBLREF 	int		mur_regno;
GBLREF	mur_opt_struct	mur_options;
GBLREF	short          	dollar_trestart;
GBLREF	short		dollar_tlevel;
GBLREF 	jnl_gbls_t	jgbl;
GBLREF	seq_num		seq_num_zero;
GBLREF	jnl_fence_control	jnl_fence_ctl;
LITREF	int		jrt_update[JRT_RECTYPES];
LITREF	boolean_t	jrt_is_replicated[JRT_RECTYPES];
static	void	(* const extraction_routine[])() =
{
#define JNL_TABLE_ENTRY(rectype, extract_rtn, label, update, fixed_size, is_replicated)	extract_rtn,
#include "jnl_rec_table.h"
#undef JNL_TABLE_ENTRY
};

uint4	mur_forward(jnl_tm_t min_broken_time, seq_num min_broken_seqno, seq_num losttn_seqno)
{
	char			new;
	boolean_t		process_losttn, extr_file_create[TOT_EXTR_TYPES], added;
	trans_num		curr_tn, last_tn;
	enum jnl_record_type	rectype;
	enum rec_fence_type	rec_fence;
	enum broken_type	recstat;
	jnl_tm_t		rec_time;
	int4			rec_image_count = 0;	/* This is a dummy variable for UNIX */
	uint4			rec_pid, status;
	mval			mv;
	seq_num 		rec_token_seq;
	jnl_record		*rec;
	jnl_string		*keystr;
	multi_struct 		*multi;
	pini_list_struct	*plst;
	reg_ctl_list		*rctl, *rctl_top;
	unsigned char		*mstack_ptr;
	ht_ent_mname		*tabent;
	mname_entry	 	gvent;
	gvnh_reg_t		*gvnh_reg;

	error_def(ERR_JNLREADEOF);
	error_def(ERR_DUPTN);
	error_def(ERR_BLKCNTEDITFAIL);
	error_def(ERR_JNLTPNEST);

	murgbl.extr_buff = (char *)malloc(murgbl.max_extr_record_length);
	for (recstat = (enum broken_type)0; recstat < TOT_EXTR_TYPES; recstat++)
		extr_file_create[recstat] = TRUE;
	jgbl.dont_reset_gbl_jrec_time = jgbl.forw_phase_recovery = TRUE;
	jgbl.mur_pini_addr_reset_fnptr = (pini_addr_reset_fnptr)mur_pini_addr_reset;
	gv_keysize = ROUND_UP2(MAX_KEY_SZ + MAX_NUM_SUBSC_LEN, 4);
	mu_gv_stack_init(&mstack_ptr);
	assert(!mur_options.rollback_losttnonly || !murgbl.db_updated);
	if (!mur_options.rollback_losttnonly)
		murgbl.db_updated = mur_options.update;
	/* At this point, murgbl.db_updated is the same as mur_options.update except in the case of LOSTTNONLY rollback in which
	 * case it is FALSE while mur_options.update is TRUE. Both these variables are used in the below code depending on
	 * whichever is appropriate. Note that we do NOT want to update the database (i.e. play forward any jnl records forward)
	 * in case of a LOSTTNONLY rollback.
	 */
	murgbl.consist_jnl_seqno = 0;
	assert(!mur_options.rollback || (losttn_seqno <= min_broken_seqno));
	for (mur_regno = 0, rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++, mur_regno++)
	{
		process_losttn = FALSE;
		if (mur_options.forward)
		{
			assert(NULL == rctl->jctl_turn_around);
			mur_jctl = rctl->jctl = rctl->jctl_head;
			mur_jctl->rec_offset = JNL_HDR_LEN;
			jnl_fence_ctl.fence_list = JNL_FENCE_LIST_END; /* initialized to reflect journaling is not enabled */
		} else
		{
			mur_jctl = rctl->jctl = (NULL == rctl->jctl_turn_around) ? rctl->jctl_head : rctl->jctl_turn_around;
			mur_jctl->rec_offset = mur_jctl->turn_around_offset;
			jgbl.mur_jrec_seqno = mur_jctl->turn_around_seqno;
			if (mur_options.rollback && murgbl.consist_jnl_seqno < jgbl.mur_jrec_seqno)
				murgbl.consist_jnl_seqno = jgbl.mur_jrec_seqno;
			assert(murgbl.consist_jnl_seqno <= losttn_seqno);
			assert((NULL != rctl->jctl_turn_around) || (0 == mur_jctl->rec_offset));
		}
		if (mur_options.update || mur_options.extr[GOOD_TN])
		{
			gv_cur_region = rctl->gd;
			tp_change_reg();
			SET_CSA_DIR_TREE(cs_addrs, gv_keysize, gv_cur_region);
			/* Keep gv_target and gv_cur_region in sync always. Now that region is changed, set gv_target to
			 * csa->dir_tree. We dont want to set it to NULL as there is code in t_begin that assumes it is non-NULL.
			 */
			gv_target = cs_addrs->dir_tree;
		}
		curr_tn = 0;
		rec_token_seq = 0;
		for (mur_jctl->after_end_of_data = FALSE, status = mur_next(mur_jctl->rec_offset);
							SS_NORMAL == status; status = mur_next_rec())
		{
			rec = mur_rab.jnlrec;
			rectype = (enum jnl_record_type)rec->prefix.jrec_type;
			rec_time = rec->prefix.time;
			if (rec_time > mur_options.before_time)
				/* Even they do not go to losttrans or brkntrans files */
				break;
			if (mur_options.selection && !mur_select_rec())
				continue;
			assert((0 == mur_options.after_time) || mur_options.forward && !murgbl.db_updated);
			if (rec_time < mur_options.after_time)
				continue;
			if (REC_HAS_TOKEN_SEQ(rectype))
				rec_token_seq = GET_JNL_SEQNO(rec);
			if (!process_losttn && mur_options.rollback && rec_token_seq >= losttn_seqno)
				process_losttn = TRUE;
			/* Note: Broken transaction determination is done below only based on the records that got selected as
			 * part of the mur_options.selection criteria. Therefore depending on whether a broken transaction gets
			 * selected or not, future complete transactions might either go to the lost transaction or extract file.
			 */
			recstat = GOOD_TN;
			if (FENCE_NONE != mur_options.fences)
			{
				if (IS_FENCED(rectype))
				{
					assert(IS_REPLICATED(rectype));
					DEBUG_ONLY(
						/* assert that all TP records before min_broken_time are not broken */
						if (IS_TP(rectype) &&
							((!mur_options.rollback && rec_time < min_broken_time) ||
							  (mur_options.rollback && rec_token_seq < min_broken_seqno)))
						{
							status = mur_get_pini(rec->prefix.pini_addr, &plst);
							assert(SS_NORMAL == status);
							rec_pid = plst->jpv.jpv_pid;
							VMS_ONLY(rec_image_count = plst->jpv.jpv_image_count;)
							rec_fence = GET_REC_FENCE_TYPE(rectype);
							if (NULL != (multi = MUR_TOKEN_LOOKUP(rec_token_seq, rec_pid,
								rec_image_count, rec_time, rec_fence)))
								assert(0 == multi->partner);
						}
					)
					if ((!mur_options.rollback && rec_time >= min_broken_time) ||
					     (mur_options.rollback && rec_token_seq >= min_broken_seqno))
					{	/* the above if checks are to avoid hash table lookup (performance),
						 * when it is not needed */
						assert(!mur_options.rollback || process_losttn);
						status = mur_get_pini(rec->prefix.pini_addr, &plst);
						if (SS_NORMAL != status)
							break;
						rec_pid = plst->jpv.jpv_pid;
						VMS_ONLY(rec_image_count = plst->jpv.jpv_image_count;)
						rec_fence = GET_REC_FENCE_TYPE(rectype);
						if (ZTPFENCE == rec_fence)
							rec_token_seq = ((struct_jrec_ztp_upd *)rec)->token;
						if ((NULL != (multi = MUR_TOKEN_LOOKUP(rec_token_seq, rec_pid,
								rec_image_count, rec_time, rec_fence))) && (0 < multi->partner))
						{
							process_losttn = TRUE;
							recstat = BROKEN_TN;
						}
					}
				} else if ((FENCE_ALWAYS == mur_options.fences) && (IS_SET_KILL_ZKILL(rectype)))
				{
					process_losttn = TRUE;
					recstat = BROKEN_TN;
				}
			}
			if (GOOD_TN == recstat && process_losttn)
			{
				if (!mur_options.rollback)
				{
					murgbl.err_cnt = murgbl.err_cnt + 1;
					/* JRT_INCTN will not be applied to database after a broken transaction is found */
					if (murgbl.err_cnt > mur_options.error_limit || JRT_INCTN == rectype)
						recstat = LOST_TN;
					/* the above check needs to be transaction based instead of record based ??? */
				} else
					recstat = LOST_TN;
			}
			if (mur_options.show)
			{
				assert(SS_NORMAL == status);
				if (BROKEN_TN != recstat)
				{
					if (JRT_PFIN == rectype)
						status = mur_pini_state(rec->prefix.pini_addr, FINISHED_PROC);
					else if ((JRT_EOF != rectype)
							&& ((JRT_ALIGN != rectype) || (JNL_HDR_LEN != rec->prefix.pini_addr)))
					{	/* Note that it is possible that we have a PINI record followed by a PFIN record
						 * and later an ALIGN record with the pini_addr pointing to the original PINI
						 * record (see comment in jnl_write.c where pini_addr gets assigned to JNL_HDR_LEN)
						 * In this case we do not want the ALIGN record to cause the process to become
						 * ACTIVE although it has written a PFIN record. Hence the check above.
						 */
						status = mur_pini_state(rec->prefix.pini_addr, ACTIVE_PROC);
					}
				} else
					status = mur_pini_state(rec->prefix.pini_addr, BROKEN_PROC);
				if (SS_NORMAL != status)
					break;	/* mur_pini_state() failed due to bad pini_addr */
				++mur_jctl->jnlrec_cnt[rectype];	/* for -show=STATISTICS */
			}
			if (!mur_options.update && !mur_options.extr[GOOD_TN])
				continue;
			if (murgbl.db_updated && IS_TUPD(rectype) && GOOD_TN == recstat)
			{	/* Even for FENCE_NONE we apply fences. Otherwise an TUPD becomes UPD etc. */
				if (dollar_tlevel)
				{
					assert(FALSE);
					gtm_putmsg(VARLSTCNT(6) ERR_JNLTPNEST, 4, mur_jctl->jnl_fn_len,
						mur_jctl->jnl_fn, mur_jctl->rec_offset, &rec->prefix.tn);
					op_trollback(0);
				}
				/* Note: op_tstart resets gv_currkey. So set gv_currkey later. */
				/* mv is used to determine transaction id. But it is ignored by recover/rollback */
				mv.mvtype = MV_STR;
				mv.str.len = 0;
				mv.str.addr = NULL;
				op_tstart(TRUE, TRUE, &mv, -1);
				tp_set_sgm();
			}
			/* For extract, if database was present we would have done gvcst_init().
			 * For recover/rollback gvcst_init() should definitely have been done.
			 * In both cases rctl->csa will be non-NULL.
			 * Only then can we call gvcst_root_search() to find out collation set up for this global.
			 */
			assert(!mur_options.update || (NULL != rctl->csa));
			if (IS_SET_KILL_ZKILL(rectype))
			{	/* ZTP has different record format than TP or non-TP. TP and non-TP has same format */
				keystr = (IS_ZTP(rectype)) ? (jnl_string *)&rec->jrec_fkill.mumps_node
							   : (jnl_string *)&rec->jrec_kill.mumps_node;
				memcpy(gv_currkey->base, &keystr->text[0], keystr->length);
				gv_currkey->base[keystr->length] = '\0';
				gv_currkey->end = keystr->length;
				if (NULL != rctl->csa)
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
						if (dollar_trestart)
							gv_target->clue.end = 0;
					} else
					{
						assert(gv_cur_region->max_key_size <= MAX_KEY_SZ);
						gv_target = (gv_namehead *)targ_alloc(gv_cur_region->max_key_size,
							&gvent, gv_cur_region);
						gvnh_reg = (gvnh_reg_t *)malloc(sizeof(gvnh_reg_t));
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
					if ((0 == gv_target->root) || (DIR_ROOT == gv_target->root))
					{
						assert(gv_target != cs_addrs->dir_tree);
						gvcst_root_search();
					}
				}
			}
			if (GOOD_TN == recstat)
			{
				if ((IS_SET_KILL_ZKILL(rectype) && !IS_TP(rectype)) || JRT_TCOM == rectype)
				{
					/*
					 * Do forward journaling, eliminating operations with duplicate transaction
					 * numbers.
					 *
					 * While doing journaling on a database, a process may be killed immediately
					 * after updating (or partially updating) the journal file, but before the
					 * database gets updated.  Since the transaction was never fully committed,
					 * the database transaction number has not been updated, and the last journal
					 * record does not reflect the actual state of the database.  The next process
					 * to update the database writes a journal record with the same transaction
					 * number as the previous record.  While processing the journal file, we must
					 * recognize this and delete the uncommitted transaction.
					 *
					 * This process is fairly straightforward (queue up the journal records,
					 * writing out the ones in the queue when prev_tn != curr_tn), except for
					 * the following special conditions:
					 *
					 *	-------------------------------------------
					 *	|  tn  | PBLK | PBLK | PBLK | PBLK | tn+1 |    case 1 (normal)
					 *	-------------------------------------------
					 *	       ^
					 *
					 *	-------------------------------------------
					 *	|  tn  | PBLK | PBLK | PBLK | PBLK |  tn  |    case 2
					 *	-------------------------------------------
					 *	       ^
					 *
					 * PBLK records (before-image database blocks) don't have a transaction
					 * number associated with them.  We may have any number of these PBLKs
					 * before the next record with a transaction number, so we must queue up
					 * the record prior to the first PBLK, and not update prev_tn until we
					 * have seen the record following the sequence of PBLKs.  When we encounter
					 * it, we do the comparison.  If prev_tn == curr_tn, we delete all the records
					 * in the queue.  If prev_tn != curr_tn, we commit all the records in the
					 * queue, then start the process over again, queueing up the current record.
					 *
					 * Similarly, although ZTCOM records do have a transaction number associated
					 * with them, they do not represent a separate database update;  thus, the
					 * next record following a ZTCOM that corresponds to an update may have the
					 * same transaction number as the ZTCOM.  Therefore, We do not update prev_tn
					 * after encountering a ZTCOM record.
					 *
					 * Transaction processing:
					 * Each journal record has the same transaction number, so we queue them
					 * up and when we reach the record following the tcommit, we check its
					 * transaction number.  If it matches, we throw away the queue, otherwise
					 * commit.
					 *
					 */
					last_tn = curr_tn;
					curr_tn =  rec->prefix.tn;
					if (last_tn == curr_tn)
					{
						assert(FALSE); /* We want to debug this */
						murgbl.wrn_count++;
						gtm_putmsg(VARLSTCNT(6) ERR_DUPTN, 4, &curr_tn, mur_jctl->rec_offset,
							mur_jctl->jnl_fn_len, mur_jctl->jnl_fn);
						if (murgbl.db_updated && dollar_tlevel)
							op_trollback(0);
					}
				}
				if (murgbl.db_updated)
				{
					assert(!mur_options.rollback || (rec_token_seq < losttn_seqno));
					if (SS_NORMAL != (status = mur_output_record())) /* updates murgbl.consist_jnl_seqno */
						break;
					assert(!mur_options.rollback || (murgbl.consist_jnl_seqno <= losttn_seqno));
				}
			}
			if (GOOD_TN != recstat || mur_options.extr[GOOD_TN])
			{
				if (extr_file_create[recstat])
				{
					if (SS_NORMAL != (status = mur_cre_file_extfmt(recstat)))
						break;
					extr_file_create[recstat] = FALSE;
				}
				/* extract "rec" using routine "extraction_routine[rectype]" into broken transaction file */
				EXTRACT_JNLREC(rec, extraction_routine[rectype], murgbl.file_info[recstat], status);
				if (SS_NORMAL != status)
					break;
			}
		}
		if (SS_NORMAL != status && ERR_JNLREADEOF != status)
			return status;
		jgbl.mur_plst = NULL;	/* No more simulation of GT.M activity for this region. */
		if (murgbl.db_updated && SS_NORMAL != mur_block_count_correct())
		{
			gtm_putmsg(VARLSTCNT(4) ERR_BLKCNTEDITFAIL, 2, DB_LEN_STR(gv_cur_region));
			murgbl.wrn_count++;
		}
		assert(!mur_options.rollback || 0 != murgbl.consist_jnl_seqno);
		assert(mur_options.rollback || 0 == murgbl.consist_jnl_seqno);
		assert(!dollar_tlevel);	/* In case it applied a broken TUPD */
	}
	return SS_NORMAL;
}
