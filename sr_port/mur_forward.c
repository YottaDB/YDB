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
#include "gdskill.h"		/* needed for tp.h */
#include "gdscc.h"		/* needed for tp.h */
#include "tp.h"
#include "mur_jnl_ext.h"
#include "iosp.h"
#include "gtmmsg.h"
#include "op.h"
#include "mu_gv_stack_init.h"
#include "targ_alloc.h"
#include "tp_change_reg.h"
#include "gvcst_protos.h"	/* for gvcst_root_search prototype */
#include "tp_set_sgm.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif

GBLREF	gv_namehead		*gv_target;
GBLREF  gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t 	cs_data;
GBLREF  int4			gv_keysize;
GBLREF 	mur_gbls_t		murgbl;
GBLREF	reg_ctl_list		*mur_ctl, *rctl_start;
GBLREF  jnl_process_vector	*prc_vec;
GBLREF	mur_opt_struct		mur_options;
GBLREF	uint4			dollar_tlevel;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	boolean_t		skip_dbtriggers;	/* see gbldefs.c for description of this global */

error_def(ERR_BLKCNTEDITFAIL);
error_def(ERR_JNLREADEOF);

static	void	(* const extraction_routine[])() =
{
#define JNL_TABLE_ENTRY(rectype, extract_rtn, label, update, fixed_size, is_replicated)	extract_rtn,
#include "jnl_rec_table.h"
#undef JNL_TABLE_ENTRY
};

uint4	mur_forward(jnl_tm_t min_broken_time, seq_num min_broken_seqno, seq_num losttn_seqno)
{
	boolean_t		added, this_reg_stuck;
	boolean_t		is_set_kill_zkill_ztworm, is_set_kill_zkill;
	jnl_record		*rec;
	enum jnl_record_type	rectype;
	enum rec_fence_type	rec_fence;
	enum broken_type	recstat;
	jnl_tm_t		rec_time;
	int4			rec_image_count = 0;	/* This is a dummy variable for UNIX */
	uint4			status, regcnt_stuck, num_partners;
	mval			mv;
	reg_ctl_list		*rctl, *rctl_top, *prev_rctl;
	jnl_ctl_list		*jctl;
	gd_region		*reg;
	sgmnt_addrs		*csa;
	seq_num 		rec_token_seq;
	forw_multi_struct	*forw_multi;
	multi_struct 		*multi;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	skip_dbtriggers = TRUE;	/* do not want to invoke any triggers for updates done by journal recovery */
#	ifdef UNIX
	/* In case of mupip journal -recover -backward or -rollback -backward, the forward phase replays the journal records
	 * and creates new journal records. If there is no space to write these journal records, "jnl_file_lost" will eventually
	 * get called. In this case, we want it to issue a runtime error (thereby terminating the journal recovery with an
	 * abnormal exit status, forcing the user to free up more space and reissue the journal recovery) and not turn
	 * journaling off (which would silently let recovery proceed and exit with normal status even though the db might
	 * have integ errors at that point). Use the error_on_jnl_file_lost feature to implement this error triggering.
	 * This error_on_jnl_file_lost feature is not currently implemented in VMS hence the #ifdef UNIX for now.
	 */
	if (mur_options.update)
		TREF(error_on_jnl_file_lost) = JNL_FILE_LOST_ERRORS;
#	endif
	murgbl.extr_buff = (char *)malloc(murgbl.max_extr_record_length);
	for (recstat = (enum broken_type)0; recstat < TOT_EXTR_TYPES; recstat++)
		murgbl.extr_file_create[recstat] = TRUE;
	jgbl.dont_reset_gbl_jrec_time = jgbl.forw_phase_recovery = TRUE;
	assert(NULL == jgbl.mur_pini_addr_reset_fnptr);
	jgbl.mur_pini_addr_reset_fnptr = (pini_addr_reset_fnptr)mur_pini_addr_reset;
	gv_keysize = DBKEYSIZE(MAX_KEY_SZ);
	mu_gv_stack_init();
	murgbl.consist_jnl_seqno = 0;
	/* Note down passed in values in murgbl global so "mur_forward_play_cur_jrec" function can see it as well */
	murgbl.min_broken_time = min_broken_time;
	murgbl.min_broken_seqno = min_broken_seqno;
	murgbl.losttn_seqno = losttn_seqno;
	DEBUG_ONLY(murgbl.save_losttn_seqno = losttn_seqno;) /* keep save_losttn_seqno in sync at start of mur_forward.
							      * an assert in mur_close_files later checks this did not change.
							      */
	assert(!mur_options.rollback || (murgbl.losttn_seqno <= murgbl.min_broken_seqno));
	prev_rctl = NULL;
	rctl_start = NULL;
	assert(0 == murgbl.regcnt_remaining);
	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++)
	{
		if (mur_options.forward)
		{
			assert(NULL == rctl->jctl_turn_around);
			jctl = rctl->jctl = rctl->jctl_head;
			assert(jctl->reg_ctl == rctl);
			jctl->rec_offset = JNL_HDR_LEN;
			jnl_fence_ctl.fence_list = JNL_FENCE_LIST_END; /* initialized to reflect journaling is not enabled */
		} else
		{
			jctl = rctl->jctl = (NULL == rctl->jctl_turn_around) ? rctl->jctl_head : rctl->jctl_turn_around;
			assert(jctl->reg_ctl == rctl);
			jctl->rec_offset = jctl->turn_around_offset;
			jgbl.mur_jrec_seqno = jctl->turn_around_seqno;
			if (mur_options.rollback && (murgbl.consist_jnl_seqno < jgbl.mur_jrec_seqno))
			{	/* Assert that murgbl.losttn_seqno is never lesser than jgbl.mur_jrec_seqno (the turnaround
				 * point seqno) as this is what murgbl.consist_jnl_seqno is going to be set to and will
				 * eventually be the post-rollback seqno. If this condition is violated, the result of the
				 * recovery is a compromised database (the file header will indicate a Region Seqno which
				 * is not necessarily correct since seqnos prior to it might be absent in the database).
				 * Therefore, this is an out-of-design situation with respect to rollback and so stop it.
				 */
				assert(murgbl.losttn_seqno >= jgbl.mur_jrec_seqno);
				murgbl.consist_jnl_seqno = jgbl.mur_jrec_seqno;
			}
			assert(murgbl.consist_jnl_seqno <= murgbl.losttn_seqno);
			assert((NULL != rctl->jctl_turn_around) || (0 == jctl->rec_offset));
		}
		if (mur_options.update || mur_options.extr[GOOD_TN])
		{
			reg = rctl->gd;
			gv_cur_region = reg;
			tp_change_reg();	/* note : sets cs_addrs to non-NULL value even if gv_cur_region->open is FALSE
						 * (cs_data could still be NULL). */
			rctl->csa = cs_addrs;
			cs_addrs->rctl = rctl;
			rctl->csd = cs_data;
			rctl->sgm_info_ptr = cs_addrs->sgm_info_ptr;
			assert(!reg->open || (NULL != cs_addrs->dir_tree));
			gv_target = cs_addrs->dir_tree;
		}
		jctl->after_end_of_data = FALSE;
		status = mur_next(jctl, jctl->rec_offset);
		assert(ERR_JNLREADEOF != status);	/* cannot get EOF at start of forward processing */
		if (SS_NORMAL != status)
			return status;
		PRINT_VERBOSE_STAT(jctl, "mur_forward:at the start");
		rctl->process_losttn = FALSE;
		/* Any multi-region TP transaction will be processed as multiple single-region TP transactions up
		 * until the tp-resolve-time is reached. From then on, they will be treated as one multi-region TP
		 * transaction. This is needed for proper lost-tn determination (any multi-region transaction that
		 * gets played in a region AFTER it has already encountered a broken tn should treat this as a lost tn).
		 */
		do
		{
			assert(jctl == rctl->jctl);
			rec = rctl->mur_desc->jnlrec;
			rec_time = rec->prefix.time;
			if (rec_time > mur_options.before_time)
				break;	/* Records after -BEFORE_TIME do not go to extract or losttrans or brkntrans files */
			if (rec_time < mur_options.after_time)
			{
				status = mur_next_rec(&jctl);
				continue; /* Records before -AFTER_TIME do not go to extract or losttrans or brkntrans files */
			}
			if (rec_time >= jgbl.mur_tp_resolve_time)
				break;	/* Records after tp-resolve-time will be processed below */
			/* TODO: what do we do if we find a BROKEN tn here? */
			status = mur_forward_play_cur_jrec(rctl);
			if (SS_NORMAL != status)
				break;
			status = mur_next_rec(&jctl);
		} while (SS_NORMAL == status);
		CHECK_IF_EOF_REACHED(rctl, status); /* sets rctl->forw_eof_seen if needed; resets "status" to SS_NORMAL */
		if (SS_NORMAL != status)
			return status;
		if (rctl->forw_eof_seen)
		{
			PRINT_VERBOSE_STAT(jctl, "mur_forward:Reached EOF before tp_resolve_time");
			continue;	/* Reached EOF before even getting to tp_resolve_time.
					 * Do not even consider region for next processing loop */
		}
		rctl->last_tn = 0;
		murgbl.regcnt_remaining++;	/* # of regions participating in recovery at this point */
		if (NULL == rctl_start)
			rctl_start = rctl;
		if (NULL != prev_rctl)
		{
			prev_rctl->next_rctl = rctl;
			rctl->prev_rctl = prev_rctl;
		}
		prev_rctl = rctl;
		assert(murgbl.ok_to_update_db || !rctl->db_updated);
		PRINT_VERBOSE_STAT(jctl, "mur_forward:at tp_resolve_time");
	}
	/* Note that it is possible for rctl_start to be NULL at this point. That is there is no journal record in any region
	 * AFTER the calculated tp-resolve-time. This is possible if for example -AFTER_TIME was used and has a time later
	 * than any journal record in all journal files. If rctl_start is NULL, prev_rctl should also be NULL and vice versa.
	 */
	if (prev_rctl != rctl_start)
	{
		assert(NULL != prev_rctl);
		assert(NULL != rctl_start);
		prev_rctl->next_rctl = rctl_start;
		rctl_start->prev_rctl = prev_rctl;
	} else
	{	/* prev_rctl & rctl_start are identical. They both should be NULL or should point to a single element linked list */
		assert((NULL == rctl_start) || (NULL == rctl_start->next_rctl) && (NULL == rctl_start->prev_rctl));
	}
	rctl = rctl_start;
	regcnt_stuck = 0; /* # of regions we are stuck in waiting for other regions to resolve a multi-region TP transaction */
	assert((NULL == rctl) || (NULL == rctl->forw_multi));
	gv_cur_region = NULL;	/* clear out any previous value to ensure gv_cur_region/cs_addrs/cs_data
				 * all get set in sync by the MUR_CHANGE_REG macro below.
				 */
	while (NULL != rctl)
	{	/* while there is at least one region remaining with unprocessed journal records */
		assert(NULL != rctl_start);
		assert(0 < murgbl.regcnt_remaining);
		if (NULL != rctl->forw_multi)
		{	/* This region's current journal record is part of a TP transaction waiting for other regions */
			regcnt_stuck++;
			if (regcnt_stuck >= murgbl.regcnt_remaining)
				GTMASSERT;	/* Out-of-design situation. Stuck in ALL regions. */
			rctl = rctl->next_rctl;	/* Move on to the next available region */
			assert(NULL != rctl);
			continue;
		}
		regcnt_stuck = 0;	/* restart the counter now that we found at least one non-stuck region */
		MUR_CHANGE_REG(rctl);
		jctl = rctl->jctl;
		this_reg_stuck = FALSE;
		for ( status = SS_NORMAL; SS_NORMAL == status; )
		{
			assert(jctl == rctl->jctl);
			rec = rctl->mur_desc->jnlrec;
			rec_time = rec->prefix.time;
			assert(rec_time >= jgbl.mur_tp_resolve_time);
			if (rec_time > mur_options.before_time)
				break;	/* Records after -BEFORE_TIME do not go to extract or losttrans or brkntrans files */
			assert((0 == mur_options.after_time) || mur_options.forward && !rctl->db_updated);
			if (rec_time < mur_options.after_time)
			{
				status = mur_next_rec(&jctl);
				continue; /* Records before -AFTER_TIME do not go to extract or losttrans or brkntrans files */
			}
			/* Check if current journal record can be played right away or need to wait for corresponding journal
			 * records from other participating TP regions to be reached. A non-TP or ZTP transaction can be played
			 * without issues (i.e. has no dependencies with any other regions). A single-region TP transaction too
			 * falls in the same category. A multi-region TP transaction needs to wait until all participating regions
			 * have played all journal records BEFORE this TP in order to ensure recover plays records in the exact
			 * same order that GT.M performed them in.
			 */
			/* If FENCE_NONE is specified, we would not have maintained any multi hashtable in mur_back_process for
			 * broken transaction processing. So we process multi-region TP transactions as multiple single-region
			 * TP transactions in forward phase.
			 */
			if (FENCE_NONE != mur_options.fences)
			{
				rectype = (enum jnl_record_type)rec->prefix.jrec_type;
				if (IS_TP(rectype) && IS_TUPD(rectype))
				{
					assert(IS_SET_KILL_ZKILL_ZTRIG_ZTWORM(rectype));
					assert(&rec->jrec_set_kill.num_participants == &rec->jrec_ztworm.num_participants);
					num_partners = rec->jrec_set_kill.num_participants;
					assert(0 < num_partners);
					if (1 < num_partners)
					{
						this_reg_stuck = TRUE;
						assert(&rec->jrec_set_kill.update_num == &rec->jrec_ztworm.update_num);
					}
				}
			}
			if (this_reg_stuck)
			{
				rec_token_seq = GET_JNL_SEQNO(rec);
				VMS_ONLY(
					/* In VMS, pid is not unique. We need "image_count" as well. But this is not needed
					 * in case of rollback as the token is guaranteed to be unique in that case.
					 */
					if (!mur_options.rollback)
					{
						MUR_GET_IMAGE_COUNT(jctl, rec, rec_image_count, status);
						if (SS_NORMAL != status)
						{
							this_reg_stuck = FALSE;	/* so abnormal "status" gets checked below */
							break;
						}
					}
				)
				/* In Unix, "rec_image_count" is ignored by the MUR_FORW* macros */
				MUR_FORW_TOKEN_LOOKUP(forw_multi, rec_token_seq, rec_time, rec_image_count);
				if (NULL != forw_multi)
				{	/* This token has already been seen in another region in forward processing.
					 * Add current region as well. If all regions have been resolved, then play
					 * the entire transaction maintaining the exact same order of updates within.
					 */
					MUR_FORW_TOKEN_ONE_MORE_REG(forw_multi, rctl);
				} else
				{	/* First time we are seeing this token in forward processing. Check if this
					 * has already been determined to be a broken transaction.
					 */
					recstat = GOOD_TN;
					multi = NULL;
					if (IS_REC_POSSIBLY_BROKEN(rec_time, rec_token_seq))
					{
						multi = MUR_TOKEN_LOOKUP(rec_token_seq, rec_image_count, rec_time, TPFENCE);
						if ((NULL != multi) && (0 < multi->partner))
							recstat = BROKEN_TN;
					}
					MUR_FORW_TOKEN_ADD(forw_multi, rec_token_seq, rec_time, rctl, num_partners,
								recstat, multi, rec_image_count);
				}
				/* Check that "tabent" field has been initialized above (by either the MUR_FORW_TOKEN_LOOKUP
				 * or MUR_FORW_TOKEN_ADD macros). This is relied upon by "mur_forward_play_multireg_tp" below.
				 */
				assert(NULL != forw_multi->u.tabent);
				assert(forw_multi->num_reg_seen_forward <= forw_multi->num_reg_seen_backward);
				if (forw_multi->num_reg_seen_forward == forw_multi->num_reg_seen_backward)
				{	/* All regions have been seen in forward processing. Now play it.
					 * Note that the TP could be BROKEN_TN or GOOD_TN. The callee handles it.
					 */
					assert(forw_multi == rctl->forw_multi);
					status = mur_forward_play_multireg_tp(forw_multi, rctl);
					this_reg_stuck = FALSE;
					/* Note that as part of playing the TP transaction, we could have reached
					 * the EOF of rctl. In this case, we need to break out of the loop.
					 */
					if ((SS_NORMAL != status) || rctl->forw_eof_seen)
						break;
					assert(NULL == rctl->forw_multi);
					assert(!dollar_tlevel);
					jctl = rctl->jctl;	/* In case the first record after the most recently processed
								 * TP transaction is in the next generation journal file */
					continue;
				}
				break;
			} else
			{
				status = mur_forward_play_cur_jrec(rctl);
				if (SS_NORMAL != status)
					break;
			}
			assert(!this_reg_stuck);
			status = mur_next_rec(&jctl);
		}
		assert((NULL == rctl->forw_multi) || this_reg_stuck);
		assert((NULL != rctl->forw_multi) || !this_reg_stuck);
		if (!this_reg_stuck)
		{	/* We are not stuck in this region (to resolve a multi-region TP).
			 * This means we are done processing all the records of this region.
			 */
			assert(NULL == rctl->forw_multi);
			if (!rctl->forw_eof_seen)
			{
				CHECK_IF_EOF_REACHED(rctl, status);
					/* sets rctl->forw_eof_seen if needed; resets "status" to SS_NORMAL */
				if (SS_NORMAL != status)
					return status;
				assert(!dollar_tlevel);
				DELETE_RCTL_FROM_UNPROCESSED_LIST(rctl); /* since all of its records should have been processed */
			} else
			{	/* EOF was seen in rctl inside "mur_forward_play_multireg_tp" and it was removed
				 * from the unprocessed list of rctls. At the time rctl was removed, its "next_rctl"
				 * field could have been pointing to another <rctl> that has since then also been
				 * removed inside the same function. Therefore the "next_rctl" field is not reliable
				 * in this case but instead we should rely on the global variable "rctl_start" which
				 * points to the list of unprocessed rctls. Set "next_rctl" accordingly.
				 */
				rctl->next_rctl = rctl_start;
			}
			assert(rctl->deleted_from_unprocessed_list);
		}
		assert(!this_reg_stuck || !rctl->forw_eof_seen);
		assert((NULL == rctl->next_rctl) || (NULL != rctl_start));
		assert((NULL == rctl->next_rctl) || (0 < murgbl.regcnt_remaining));
		rctl = rctl->next_rctl;	/* Note : even though "rctl" could have been deleted from the doubly linked list above,
					 * rctl->next_rctl is not touched so we can still use it to get to the next element. */
	}
	assert(0 == murgbl.regcnt_remaining);
	jgbl.mur_pini_addr_reset_fnptr = NULL;	/* No more simulation of GT.M activity for any region */
	prc_vec = murgbl.prc_vec;	/* Use process-vector of MUPIP RECOVER (not any simulating GT.M process) now onwards */
	assert(0 == dollar_tlevel);
	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++)
	{
		PRINT_VERBOSE_STAT(rctl->jctl, "mur_forward:at the end");
		assert(!mur_options.rollback || (0 != murgbl.consist_jnl_seqno));
		assert(mur_options.rollback || (0 == murgbl.consist_jnl_seqno));
		assert(!dollar_tlevel);	/* In case it applied a broken TUPD */
		assert(murgbl.ok_to_update_db || !rctl->db_updated);
		rctl->mur_plst = NULL;	/* reset now that simulation of GT.M updates is done */
		/* Ensure mur_block_count_correct is called if updates allowed*/
		if ((murgbl.ok_to_update_db) && (SS_NORMAL != mur_block_count_correct(rctl)))
		{
			gtm_putmsg(VARLSTCNT(4) ERR_BLKCNTEDITFAIL, 2, DB_LEN_STR(rctl->gd));
			murgbl.wrn_count++;
		}
	}
	return SS_NORMAL;
}
