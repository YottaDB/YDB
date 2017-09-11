/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_signal.h"
#include "gtm_unistd.h"
#include "gtm_time.h"
#include "gtm_string.h"
#include "gtm_ipc.h"

#include <stddef.h> /* for OFFSETOF() macro */
#include <sys/shm.h>
#include <sys/types.h>

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
#include "gtm_multi_proc.h"
#include "relqop.h"
#include "interlock.h"
#include "add_inter.h"
#include "do_shmat.h"
#include "rel_quant.h"
#include "wcs_timer_start.h"
#include "gds_rundown.h"
#include "wcs_clean_dbsync.h"
#include "gtmcrypt.h"
#ifdef DEBUG
#include "is_proc_alive.h"
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
GBLREF	int			mur_forw_mp_hash_buckets;	/* # of buckets in "mur_shm_hdr->hash_bucket_array" */
GBLREF	mur_shm_hdr_t		*mur_shm_hdr;	/* Pointer to mur_forward-specific header in shared memory */
GBLREF	boolean_t		mur_forward_multi_proc_done;
GBLREF	uint4			process_id;

error_def(ERR_BLKCNTEDITFAIL);
error_def(ERR_FILENOTCREATE);
error_def(ERR_FORCEDHALT);
error_def(ERR_JNLREADEOF);
error_def(ERR_SYSCALL);

#define STUCK_TIME	(16 * MILLISECS_IN_SEC)

static	void	(* const extraction_routine[])() =
{
#define JNL_TABLE_ENTRY(rectype, extract_rtn, label, update, fixed_size, is_replicated)	extract_rtn,
#include "jnl_rec_table.h"
#undef JNL_TABLE_ENTRY
};

uint4	mur_forward(jnl_tm_t min_broken_time, seq_num min_broken_seqno, seq_num losttn_seqno)
{
	jnl_tm_t		adjusted_resolve_time;
	int			sts, max_procs;
	size_t			shm_size;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	skip_dbtriggers = TRUE;	/* do not want to invoke any triggers for updates done by journal recovery */
	/* In case of mupip journal -recover -backward or -rollback -backward, the forward phase replays the journal records
	 * and creates new journal records. If there is no space to write these journal records, "jnl_file_lost" will eventually
	 * get called. In this case, we want it to issue a runtime error (thereby terminating the journal recovery with an
	 * abnormal exit status, forcing the user to free up more space and reissue the journal recovery) and not turn
	 * journaling off (which would silently let recovery proceed and exit with normal status even though the db might
	 * have integ errors at that point). Use the error_on_jnl_file_lost feature to implement this error triggering.
	 */
	if (mur_options.update)
		TREF(error_on_jnl_file_lost) = JNL_FILE_LOST_ERRORS;
	murgbl.extr_buff = (char *)malloc(murgbl.max_extr_record_length);
	jgbl.dont_reset_gbl_jrec_time = jgbl.forw_phase_recovery = TRUE;
	assert(NULL == jgbl.mur_pini_addr_reset_fnptr);
	jgbl.mur_pini_addr_reset_fnptr = (pini_addr_reset_fnptr)mur_pini_addr_reset;
	mu_gv_stack_init();
	murgbl.consist_jnl_seqno = 0;
	/* Note down passed in values in murgbl global so "mur_forward_play_cur_jrec" function can see it as well */
	murgbl.min_broken_time = min_broken_time;
	murgbl.min_broken_seqno = min_broken_seqno;
	murgbl.losttn_seqno = losttn_seqno;
	/* We play multi-reg TP transactions as multiple single-region TP transactions until the tp_resolve_time.
	 * And then play it as a multi-reg TP transaction. The first phase can be parallelized whereas the second cannot.
	 * So try to play as much as possible using the first phase. Almost always, min_broken_time would be greater than
	 * tp_resolve_time so we can do the first phase until min_broken_time. But there are some cases where min_broken_time
	 * can be 0 (e.g. ZTP broken transactions are detected in mupip_recover.c). Those should be uncommon and so in those
	 * cases, revert to using tp_resolve_time as the transition point between phase1 and phase2.
	 */
	assert((min_broken_time >= jgbl.mur_tp_resolve_time) || !min_broken_time);
	adjusted_resolve_time = (!min_broken_time ? jgbl.mur_tp_resolve_time : min_broken_time);
	murgbl.adjusted_resolve_time = adjusted_resolve_time;	/* needed by "mur_forward_multi_proc" */
	DEBUG_ONLY(jgbl.mur_tp_resolve_time = adjusted_resolve_time);	/* An assert in tp_tend relies on this.
									 * Even in pro, this is a safe change to do but
									 * no one cares about jgbl.mur_tp_resolve_time in
									 * forward phase other than tp_tend so we do nothing.
									 */
	DEBUG_ONLY(murgbl.save_losttn_seqno = losttn_seqno); /* keep save_losttn_seqno in sync at start of mur_forward.
							      * an assert in mur_close_files later checks this did not change.
							      */
	assert(!mur_options.rollback || (murgbl.losttn_seqno <= murgbl.min_broken_seqno));
	max_procs = gtm_mupjnl_parallel;
	if (!max_procs || (max_procs > murgbl.reg_total))
		max_procs = murgbl.reg_total;
	mur_forw_mp_hash_buckets = getprime(murgbl.reg_total + 32);	/* Add 32 to get bigger prime # and in turn better hash */
	assert(mur_forw_mp_hash_buckets);
	shm_size = (size_t)(SIZEOF(mur_shm_hdr_t)
				+ (SIZEOF(que_ent) * mur_forw_mp_hash_buckets)
				+ (SIZEOF(shm_forw_multi_t) * murgbl.reg_total)
				+ (SIZEOF(shm_reg_ctl_t) * murgbl.reg_total));
	sts = gtm_multi_proc((gtm_multi_proc_fnptr_t)&mur_forward_multi_proc, max_procs, max_procs,
				murgbl.ret_array, (void *)mur_ctl, SIZEOF(reg_ctl_list),
				shm_size, (gtm_multi_proc_fnptr_t)&mur_forward_multi_proc_init,
				(gtm_multi_proc_fnptr_t)&mur_forward_multi_proc_finish);
	return (uint4)sts;
}

int	mur_forward_multi_proc_init(reg_ctl_list *rctl)
{
	multi_proc_shm_hdr_t	*mp_hdr;	/* Pointer to "multi_proc_shm_hdr_t" structure in shared memory */
	shm_forw_multi_t	*ptr, *ptr_top;
	que_ent_ptr_t		que_head;
	shm_reg_ctl_t		*shm_rctl;
	reg_ctl_list		*rctl_top;

	/* Note: "rctl" is unused. But cannot avoid passing it since "gtm_multi_proc" expects something */
	mp_hdr = multi_proc_shm_hdr;
	mur_shm_hdr = (mur_shm_hdr_t *)((sm_uc_ptr_t)mp_hdr->shm_ret_array + (SIZEOF(void *) * mp_hdr->ntasks));
	mur_shm_hdr->hash_bucket_start = (que_ent_ptr_t)(mur_shm_hdr + 1);
	assert(mur_forw_mp_hash_buckets == getprime(murgbl.reg_total + 32));
	mur_shm_hdr->shm_forw_multi_start = (shm_forw_multi_t *)(mur_shm_hdr->hash_bucket_start + mur_forw_mp_hash_buckets);
	que_head = (que_ent_ptr_t)&mur_shm_hdr->forw_multi_free;
	for (ptr = &mur_shm_hdr->shm_forw_multi_start[0], ptr_top = ptr + murgbl.reg_total; ptr < ptr_top; ptr++)
		insqt((que_ent_ptr_t)&ptr->free_chain, que_head);
	shm_rctl = (shm_reg_ctl_t *)ptr;
	mur_shm_hdr->shm_rctl_start = shm_rctl;
	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++, shm_rctl++)
		shm_rctl->jnlext_shmid = INVALID_SHMID;
	return 0;
}

int	mur_forward_multi_proc(reg_ctl_list *rctl)
{
	boolean_t		multi_proc, this_reg_stuck, release_latch, ok_to_play;
	boolean_t		cancelled_dbsync_timer;
	reg_ctl_list		*rctl_top, *prev_rctl;
	jnl_ctl_list		*jctl;
	gd_region		*reg;
	sgmnt_addrs		*csa;
	seq_num 		rec_token_seq;
	jnl_tm_t		rec_time;
	enum broken_type	recstat;
	jnl_record		*rec;
	enum jnl_record_type	rectype;
	char			errstr[256];
	int			i, rctl_index, save_errno, num_procs_stuck, num_reg_stuck;
	uint4			status, regcnt_stuck, num_partners, start_hrtbt_cntr;
	boolean_t		stuck;
	forw_multi_struct	*forw_multi;
	shm_forw_multi_t	*sfm;
	multi_struct 		*multi;
	jnl_tm_t		adjusted_resolve_time;
	shm_reg_ctl_t		*shm_rctl_start, *shm_rctl, *first_shm_rctl;
	size_t			shm_size, reccnt, copy_size;
	int4			*size_ptr;
	char			*shmPtr; /* not using "shm_ptr" since it is already used in an AIX include file */
	int			shmid;
	multi_proc_shm_hdr_t	*mp_hdr;	/* Pointer to "multi_proc_shm_hdr_t" structure in shared memory */

	status = 0;
	/* Although we made sure the # of tasks is the same as the # of processes forked off (in the "gtm_multi_proc"
	 * invocation in "mur_forward"), it is possible one of the forked process finishes one invocation of
	 * "mur_forward_multi_proc" before even another forked process gets assigned one task in "gtm_multi_proc_helper".
	 * In this case, we would be invoked more than once. But the first invocation would have done all the needed stuff
	 * so return for later invocations.
	 */
	if (mur_forward_multi_proc_done)
		return 0;
	mur_forward_multi_proc_done = TRUE;
	/* Note: "rctl" is unused. But cannot avoid passing it since "gtm_multi_proc" expects something */
	prev_rctl = NULL;
	rctl_start = NULL;
	adjusted_resolve_time = murgbl.adjusted_resolve_time;
	assert(0 == murgbl.regcnt_remaining);
	multi_proc = multi_proc_in_use;	/* cache value in "local" to speed up access inside loops below */
	if (multi_proc)
	{
		mp_hdr = multi_proc_shm_hdr;
		shm_rctl_start = mur_shm_hdr->shm_rctl_start;
		if (jgbl.onlnrlbk)
		{
			for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++)
			{
				assert(rctl->csa->hold_onto_crit);	/* would have been set in parent process */
				rctl->csa->hold_onto_crit = FALSE;	/* reset since we don't own this region */
				assert(rctl->csa->now_crit);		/* would have been set in parent process */
				rctl->csa->now_crit = FALSE;		/* reset since we don't own this region */
			}
		}
	}
	first_shm_rctl = NULL;
	/* Phase1 of forward recovery starts */
	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++)
	{
		/* Check if "rctl" is available for us or if some other concurrent process has taken it */
		if (multi_proc)
		{
			rctl_index = rctl - &mur_ctl[0];
			shm_rctl = &shm_rctl_start[rctl_index];
			if (shm_rctl->owning_pid)
			{
				assert(process_id != shm_rctl->owning_pid);
				continue;
			}
			GRAB_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
			assert(release_latch);
			for ( ; rctl < rctl_top; rctl++, shm_rctl++)
			{
				if (shm_rctl->owning_pid)
				{
					assert(process_id != shm_rctl->owning_pid);
					continue;
				}
				shm_rctl->owning_pid = process_id;	/* Declare ownership */
				rctl->this_pid_is_owner = TRUE;
				if (jgbl.onlnrlbk)
				{	/* This is an online rollback and crit was grabbed on all regions by the parent rollback
					 * process. But this child process now owns this region and does the actual rollback on
					 * this region so borrow crit for the duration of this child process.
					 */
					csa = rctl->csa;
					csa->hold_onto_crit = TRUE;
					csa->now_crit = TRUE;
					assert(csa->nl->in_crit == mp_hdr->parent_pid);
					csa->nl->in_crit = process_id;
					assert(csa->nl->onln_rlbk_pid == mp_hdr->parent_pid);
					csa->nl->onln_rlbk_pid = process_id;
				}
				if (NULL == first_shm_rctl)
					first_shm_rctl = shm_rctl;
				break;
			}
			REL_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
			if (rctl >= rctl_top)
			{
				assert(rctl == rctl_top);
				break;
			}
			/* Set key to print this rctl'ss region-name as prefix in case this forked off process prints any output */
			MUR_SET_MULTI_PROC_KEY(rctl, multi_proc_key);
#			ifdef MUR_DEBUG
			fprintf(stderr, "pid = %d : Owns region %s\n", process_id, multi_proc_key);
#			endif
		} else
			rctl->this_pid_is_owner = TRUE;
		if (mur_options.forward)
		{
			assert(NULL == rctl->jctl_turn_around);
			jctl = rctl->jctl = rctl->jctl_head;
			assert(jctl->reg_ctl == rctl);
			jctl->rec_offset = JNL_HDR_LEN;
			jnl_fence_ctl.fence_list = JNL_FENCE_LIST_END; /* initialized to reflect journaling is not enabled */
			if (mur_options.rollback)
				jgbl.mur_jrec_seqno = jctl->jfh->start_seqno;
		} else
		{
			jctl = rctl->jctl = (NULL == rctl->jctl_turn_around) ? rctl->jctl_head : rctl->jctl_turn_around;
			assert(jctl->reg_ctl == rctl);
			jctl->rec_offset = jctl->turn_around_offset;
			jgbl.mur_jrec_seqno = jctl->turn_around_seqno;
			assert((NULL != rctl->jctl_turn_around) || (0 == jctl->rec_offset));
		}
		if (mur_options.rollback)
		{
			if (murgbl.consist_jnl_seqno < jgbl.mur_jrec_seqno)
			{
				/* Assert that murgbl.losttn_seqno is never lesser than jgbl.mur_jrec_seqno (the turnaround
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
		}
		if (mur_options.update || mur_options.extr[GOOD_TN])
		{
			reg = rctl->gd;
			gv_cur_region = reg;
			tp_change_reg();	/* Note : sets cs_addrs to non-NULL value even if gv_cur_region->open is FALSE
						 * (cs_data could still be NULL).
						 */
			if (NULL == rctl->csa)
			{
				assert(!rctl->db_present);
				assert(!rctl->gd->open);
				rctl->csa = cs_addrs;
				rctl->csa->miscptr = rctl;
			} else
			{
				assert(rctl->csa == cs_addrs);
				assert((reg_ctl_list *)rctl->csa->miscptr == rctl);	/* set in "mur_open_files" and maybe
											 * updated in "mur_sort_files".
											 */
			}
			assert(rctl->csd == cs_data);
			rctl->sgm_info_ptr = cs_addrs->sgm_info_ptr;
			assert(!reg->open || (NULL != cs_addrs->dir_tree));
			gv_target = cs_addrs->dir_tree;
		}
		jctl->after_end_of_data = FALSE;
		status = mur_next(jctl, jctl->rec_offset);
		assert(ERR_JNLREADEOF != status);	/* cannot get EOF at start of forward processing */
		if (SS_NORMAL != status)
			goto finish;
		PRINT_VERBOSE_STAT(jctl, "mur_forward:at the start");
		rctl->process_losttn = FALSE;
		/* Any multi-region TP transaction will be processed as multiple single-region TP transactions up
		 * until the tp-resolve-time is reached. From then on, they will be treated as one multi-region TP
		 * transaction. This is needed for proper lost-tn determination (any multi-region transaction that
		 * gets played in a region AFTER it has already encountered a broken tn should treat this as a lost tn).
		 */
		do
		{
			if (multi_proc && IS_FORCED_MULTI_PROC_EXIT(mp_hdr))
			{	/* We are at a logical point. So exit if signaled by parent */
				status = ERR_FORCEDHALT;
				goto finish;
			}
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
			if (rec_time >= adjusted_resolve_time)
				break;	/* Records after this adjusted resolve_time will be processed below in phase2 */
			/* Note: Since we do hashtable token processing only for records from tp_resolve_time onwards,
			 * it is possible that if we encounter any broken transactions here we wont know they are broken
			 * but will play them as is. That is unavoidable. Specify -SINCE_TIME (for -BACKWARD rollback/recover)
			 * and -VERIFY (for -FORWARD rollback/recover) to control tp_resolve_time (and in turn more
			 * effective broken tn determination).
			 */
			status = mur_forward_play_cur_jrec(rctl);
			if (SS_NORMAL != status)
				break;
			status = mur_next_rec(&jctl);
		} while (SS_NORMAL == status);
		CHECK_IF_EOF_REACHED(rctl, status); /* sets rctl->forw_eof_seen if needed; resets "status" to SS_NORMAL */
		if (SS_NORMAL != status)
		{	/* ERR_FILENOTCREATE is possible from "mur_cre_file_extfmt" OR	ERR_FORCEDHALT is possible
			 * from "mur_forward_play_cur_jrec" OR SYSTEM-E-ENO2 from a ERR_RENAMEFAIL is possible from
			 * a "rename_file_if_exists" call. No other errors are known to occur here. Assert accordingly.
			 */
			assert((ERR_FILENOTCREATE == status) || (ERR_FORCEDHALT == status) || (ENOENT == status));
			goto finish;
		}
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
	if (multi_proc)
		multi_proc_key = NULL;	/* reset key until it can be set to rctl's region-name again */
	/* Note that it is possible for rctl_start to be NULL at this point. That is there is no journal record in any region
	 * AFTER the calculated tp-resolve-time. This is possible if for example -AFTER_TIME was used and has a time later
	 * than any journal record in all journal files. If rctl_start is NULL, prev_rctl should also be NULL and vice versa.
	 */
	if (NULL != rctl_start)
	{
		assert(NULL != prev_rctl);
		prev_rctl->next_rctl = rctl_start;
		rctl_start->prev_rctl = prev_rctl;
	}
	rctl = rctl_start;
	regcnt_stuck = 0; /* # of regions we are stuck in waiting for other regions to resolve a multi-region TP transaction */
	assert((NULL == rctl) || (NULL == rctl->forw_multi));
	gv_cur_region = NULL;	/* clear out any previous value to ensure gv_cur_region/cs_addrs/cs_data
				 * all get set in sync by the MUR_CHANGE_REG macro below.
				 */
	/* Phase2 of forward recovery starts */
	while (NULL != rctl)
	{	/* while there is at least one region remaining with unprocessed journal records */
		assert(NULL != rctl_start);
		assert(0 < murgbl.regcnt_remaining);
		if (NULL != rctl->forw_multi)
		{	/* This region's current journal record is part of a TP transaction waiting for other regions */
			regcnt_stuck++;
			assert(regcnt_stuck <= murgbl.regcnt_remaining);
			if (regcnt_stuck == murgbl.regcnt_remaining)
			{
				assertpro(multi_proc_in_use); /* Else : Out-of-design situation. Stuck in ALL regions. */
				/* Check one last time if all regions are stuck waiting for another process to resolve the
				 * multi-region TP transaction. If so, wait in a sleep loop. If not, we can proceed.
				 */
				rctl = rctl_start;
				TIMEOUT_INIT(stuck, STUCK_TIME);
				do
				{
					if (IS_FORCED_MULTI_PROC_EXIT(mp_hdr))
					{	/* We are at a logical point. So exit if signaled by parent */
						status = ERR_FORCEDHALT;
						TIMEOUT_DONE(stuck);
						goto finish;
					}
					forw_multi = rctl->forw_multi;
					assert(NULL != forw_multi);
					sfm = forw_multi->shm_forw_multi;
					assert(NULL != sfm);
					assert(sfm->num_reg_seen_forward <= sfm->num_reg_seen_backward);
#					ifdef MUR_DEBUG
					fprintf(stderr, "Pid = %d : Line %d : token = %llu : forward = %d : backward = %d\n",
						process_id, __LINE__, (long long int)sfm->token,
						sfm->num_reg_seen_forward, sfm->num_reg_seen_backward);
#					endif
					if (sfm->num_reg_seen_forward == sfm->num_reg_seen_backward)
					{	/* We are no longer stuck in this region */
						assert(!forw_multi->no_longer_stuck);
						forw_multi->no_longer_stuck = TRUE;
						TIMEOUT_DONE(stuck);
						break;
					}
					rctl = rctl->next_rctl;	/* Move on to the next available region */
					assert(NULL != rctl);
					if (rctl == rctl_start)
					{	/* We went through all regions once and are still stuck.
						 * Sleep until at leat TWO heartbeats have elapsed after which check for deadlock.
						 * Do this only in the child process that owns the FIRST region in the region list.
						 * This way we don't have contention for the GRAB_MULTI_PROC_LATCH from
						 * all children at more or less the same time.
						 */
						if ((rctl == mur_ctl) && stuck)
						{	/* Check if all processes are stuck for a while. If so assertpro */
							GRAB_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
							assert(release_latch);
							shm_rctl_start = mur_shm_hdr->shm_rctl_start;
							num_reg_stuck = 0;
							for (i = 0; i < murgbl.reg_total; i++)
							{
								shm_rctl = &shm_rctl_start[i];
								sfm = shm_rctl->shm_forw_multi;
								if (NULL != sfm)
								{
									if (sfm->num_reg_seen_forward != sfm->num_reg_seen_backward)
										num_reg_stuck++;
								}
							}
							REL_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
							/* If everyone is stuck at this point, it is an out-of-design situation */
							assertpro(num_reg_stuck < murgbl.reg_total);
							TIMEOUT_DONE(stuck);
							TIMEOUT_INIT(stuck, STUCK_TIME);
						} else
						{	/* Sleep and recheck if any region we are stuck in got resolved.
							 * To minimize time spent sleeping, we just yield our timeslice.
							 */
							rel_quant();
							continue;
						}
					}
				} while (TRUE);
			} else
			{
				rctl = rctl->next_rctl;	/* Move on to the next available region */
				assert(NULL != rctl);
				continue;
			}
		}
		regcnt_stuck = 0;	/* restart the counter now that we found at least one non-stuck region */
		MUR_CHANGE_REG(rctl);
		jctl = rctl->jctl;
		this_reg_stuck = FALSE;
		for ( status = SS_NORMAL; SS_NORMAL == status; )
		{
			if (multi_proc && IS_FORCED_MULTI_PROC_EXIT(mp_hdr))
			{	/* We are at a logical point. So exit if signaled by parent */
				status = ERR_FORCEDHALT;
				goto finish;
			}
			assert(jctl == rctl->jctl);
			rec = rctl->mur_desc->jnlrec;
			rec_time = rec->prefix.time;
			if (rec_time > mur_options.before_time)
				break;	/* Records after -BEFORE_TIME do not go to extract or losttrans or brkntrans files */
			assert((rec_time >= adjusted_resolve_time) || (mur_options.notncheck && !mur_options.verify));
			assert((0 == mur_options.after_time) || (mur_options.forward && !rctl->db_updated));
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
					assert(IS_SET_KILL_ZKILL_ZTWORM_LGTRIG_ZTRIG(rectype));
					assert(&rec->jrec_set_kill.num_participants == &rec->jrec_ztworm.num_participants);
					assert(&rec->jrec_set_kill.num_participants == &rec->jrec_lgtrig.num_participants);
					num_partners = rec->jrec_set_kill.num_participants;
					assert(0 < num_partners);
					if (1 < num_partners)
					{
						this_reg_stuck = TRUE;
						assert(&rec->jrec_set_kill.update_num == &rec->jrec_ztworm.update_num);
						assert(&rec->jrec_set_kill.update_num == &rec->jrec_lgtrig.update_num);
					}
				}
			}
			if (this_reg_stuck)
			{
				rec_token_seq = GET_JNL_SEQNO(rec);
				MUR_FORW_TOKEN_LOOKUP(forw_multi, rec_token_seq, rec_time);
				if (NULL != forw_multi)
				{	/* This token has already been seen in another region in forward processing.
					 * Add current region as well. If all regions have been resolved, then play
					 * the entire transaction maintaining the exact same order of updates within.
					 */
					if (!forw_multi->no_longer_stuck)
						MUR_FORW_TOKEN_ONE_MORE_REG(forw_multi, rctl);
				} else
				{	/* First time we are seeing this token in forward processing. Check if this
					 * has already been determined to be a broken transaction.
					 */
					recstat = GOOD_TN;
					multi = NULL;
					if (IS_REC_POSSIBLY_BROKEN(rec_time, rec_token_seq))
					{
						multi = MUR_TOKEN_LOOKUP(rec_token_seq, rec_time, TPFENCE);
						if ((NULL != multi) && (0 < multi->partner))
							recstat = BROKEN_TN;
					}
					MUR_FORW_TOKEN_ADD(forw_multi, rec_token_seq, rec_time, rctl, num_partners,
								recstat, multi);
				}
				/* Check that "tabent" field has been initialized above (by either the MUR_FORW_TOKEN_LOOKUP
				 * or MUR_FORW_TOKEN_ADD macros). This is relied upon by "mur_forward_play_multireg_tp" below.
				 */
				assert(NULL != forw_multi->u.tabent);
				assert(forw_multi->num_reg_seen_forward <= forw_multi->num_reg_seen_backward);
				if (multi_proc)
				{
					sfm = forw_multi->shm_forw_multi;
					ok_to_play = (NULL == sfm) || (sfm->num_reg_seen_forward == sfm->num_reg_seen_backward);
				} else
					ok_to_play = (forw_multi->num_reg_seen_forward == forw_multi->num_reg_seen_backward);
				assert(ok_to_play || !forw_multi->no_longer_stuck);
				if (ok_to_play )
				{	/* We have enough information to proceed with playing this multi-region TP in
					 * forward processing (even if we might not have seen all needed regions). Now play it.
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
				{
					assert(ERR_FILENOTCREATE == status);
					goto finish;
				}
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
				if (ERR_JNLREADEOF == status)
					status = SS_NORMAL;
			}
			assert(rctl->deleted_from_unprocessed_list);
		}
		assert(SS_NORMAL == status);
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
		if (!rctl->this_pid_is_owner)
		{
			assert(multi_proc_in_use);
			continue;	/* in a parallel processing environment, process only regions we own */
		}
		if (multi_proc)
		{	/* Set key to print this rctl's region-name as prefix in case this forked off process prints any output */
			MUR_SET_MULTI_PROC_KEY(rctl, multi_proc_key);
		}
		PRINT_VERBOSE_STAT(rctl->jctl, "mur_forward:at the end");
		assert(!mur_options.rollback || (0 != murgbl.consist_jnl_seqno));
		assert(mur_options.rollback || (0 == murgbl.consist_jnl_seqno));
		assert(!dollar_tlevel);	/* In case it applied a broken TUPD */
		assert(murgbl.ok_to_update_db || !rctl->db_updated);
		rctl->mur_plst = NULL;	/* reset now that simulation of GT.M updates is done */
		/* Ensure mur_block_count_correct is called if updates allowed */
		if (murgbl.ok_to_update_db && (SS_NORMAL != mur_block_count_correct(rctl)))
		{
			gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(4) ERR_BLKCNTEDITFAIL, 2, DB_LEN_STR(rctl->gd));
			murgbl.wrn_count++;
		}
	}
finish:
	if (multi_proc)
		multi_proc_key = NULL;	/* reset key until it can be set to rctl's region-name again */
	if ((SS_NORMAL == status) && mur_options.show)
		mur_output_show();
	if (NULL != first_shm_rctl)
	{	/* Transfer needed process-private information to shared memory so parent process can later inherit this. */
		first_shm_rctl->err_cnt = murgbl.err_cnt;
		first_shm_rctl->wrn_count = murgbl.wrn_count;
		first_shm_rctl->consist_jnl_seqno = murgbl.consist_jnl_seqno;
		/* If extract files were created by this process for one or more regions, then copy that information to
		 * shared memory so parent process can use this information to do a merge sort.
		 */
		shm_rctl = mur_shm_hdr->shm_rctl_start;
		for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++, shm_rctl++)
		{
			assert(multi_proc_in_use);
			if (!rctl->this_pid_is_owner)
				continue;	/* in a parallel processing environment, process only regions we own */
			/* Cancel any flush/dbsync timers by this child process for this region. This is because the
			 * child is not going to go through exit handling code (no gds_rundown etc.). And we need to
			 * clear up csa->nl->wcs_timers. (normally done by gds_rundown).
			 */
			if (NULL != rctl->csa)	/* rctl->csa can be NULL in case of "mupip journal -extract" etc. */
				CANCEL_DB_TIMERS(rctl->gd, rctl->csa, cancelled_dbsync_timer);
			reccnt = 0;
			for (size_ptr = &rctl->jnlext_multi_list_size[0], recstat = 0;
								recstat < TOT_EXTR_TYPES;
									recstat++, size_ptr++)
			{	/* Assert "extr_file_created" information is in sync between rctl and shm_rctl.
				 * This was done at the end of "mur_cre_file_extfmt".
				 */
				assert(shm_rctl->extr_file_created[recstat] == rctl->extr_file_created[recstat]);
				/* Assert that if *size_ptr is non-zero, then we better have created an extract file.
				 * Note that the converse is not true. It is possible we created a file for example to
				 * write an INCTN record but decided to not write anything because it was not a -detail
				 * type of extract. So *sizeptr could be 0 even though we created the extract file.
				 */
				assert(!*size_ptr || rctl->extr_file_created[recstat]);
				shm_rctl->jnlext_list_size[recstat] = *size_ptr;
				reccnt += *size_ptr;
			}
			assert(INVALID_SHMID == shm_rctl->jnlext_shmid);
			shm_size = reccnt * SIZEOF(jnlext_multi_t);
			/* If we are quitting because of an abnormal status OR a forced signal to terminate
			 * OR if the parent is dead (kill -9) don't bother creating shmid to communicate back with parent.
			 */
			if (mp_hdr->parent_pid != getppid())
			{
				SET_FORCED_MULTI_PROC_EXIT;	/* Also signal sibling children to stop processing */
				if (SS_NORMAL != status)
					status = ERR_FORCEDHALT;
			}
			if ((SS_NORMAL == status) && shm_size)
			{
				shmid = shmget(IPC_PRIVATE, shm_size, 0600 | IPC_CREAT);
				if (-1 == shmid)
				{
					save_errno = errno;
					SNPRINTF(errstr, SIZEOF(errstr),
						"shmget() : shmsize=0x%llx", shm_size);
					MUR_SET_MULTI_PROC_KEY(rctl, multi_proc_key);	/* to print region name prefix */
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8)
								ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
				}
				shmPtr = (char *)do_shmat(shmid, 0, 0);
				if (-1 == (sm_long_t)shmPtr)
				{
					save_errno = errno;
					SNPRINTF(errstr, SIZEOF(errstr),
						"shmat() : shmid=%d shmsize=0x%llx", shmid, shm_size);
					MUR_SET_MULTI_PROC_KEY(rctl, multi_proc_key);	/* to print region name prefix */
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8)
								ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
				}
				shm_rctl->jnlext_shmid = shmid;
				shm_rctl->jnlext_shm_size = shm_size;
				for (size_ptr = &rctl->jnlext_multi_list_size[0], recstat = 0;
									recstat < TOT_EXTR_TYPES;
										recstat++, size_ptr++)
				{
					shm_size = *size_ptr;
					if (shm_size)
					{
						copy_size = copy_list_to_buf(rctl->jnlext_multi_list[recstat],
												(int4)shm_size, shmPtr);
						assert(copy_size == (shm_size * SIZEOF(jnlext_multi_t)));
						shmPtr += copy_size;
					}
				}
			}
		}
	}
	mur_close_file_extfmt(IN_MUR_CLOSE_FILES_FALSE);	/* Need to flush buffered extract/losttrans/brokentrans files */
	return (int)status;
}

/* This function is invoked once all parallel invocations of "mur_forward_multi_proc" are done.
 * This transfers information from shared memory (populated by parallel processes) into parent process private memory.
 * Note: This function is invoked even if no parallel invocations happen. In that case, it only does merge-sort of
 * journal extract/losttrans/brokentrans files if necessary.
 *
 * It returns 0 for normal exit. A non-zero value for an error.
 */
int	mur_forward_multi_proc_finish(reg_ctl_list *rctl)
{
	shm_reg_ctl_t		*shm_rctl;
	reg_ctl_list		*rctl_top;
	enum broken_type	recstat;
	sgmnt_addrs		*csa;

	if (multi_proc_in_use)
	{
		multi_proc_key = NULL;	/* reset key now that parallel invocations are done */
		DEBUG_ONLY(multi_proc_key_exception = TRUE;)	/* "multi_proc_in_use" is still TRUE even though all usage of
								 * multiple processes is done so set this dbg flag to avoid asserts.
								 * Keep this set until we return back to "gtm_multi_proc" (where
								 * "multi_proc_in_use" gets reset).
								 */
		assert(multi_proc_shm_hdr->parent_pid == process_id);	/* Only parent process should invoke this function */
		assert(NULL == rctl);
		shm_rctl = mur_shm_hdr->shm_rctl_start;
		for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++, shm_rctl++)
		{
			/* Compute murgbl.consist_jnl_seqno as MAX of individual process values */
			if (murgbl.consist_jnl_seqno < shm_rctl->consist_jnl_seqno)
				murgbl.consist_jnl_seqno = shm_rctl->consist_jnl_seqno;
			/* Compute murgbl.err_cnt as SUM of individual process values. The parallel process makes sure it
			 * updates only ONE owning region (even if it owns multiple regions) with its murgbl.err_cnt value.
			 */
			murgbl.err_cnt += shm_rctl->err_cnt;
			/* Compute murgbl.wrn_count as SUM of individual process values. The parallel process makes sure it
			 * updates only ONE owning region (even if it owns multiple regions) with its murgbl.wrn_count value.
			 */
			murgbl.wrn_count += shm_rctl->wrn_count;
			for (recstat = 0; recstat < TOT_EXTR_TYPES; recstat++)
			{
				assert(!rctl->extr_file_created[recstat]);
				rctl->extr_file_created[recstat] = shm_rctl->extr_file_created[recstat];
			}
			if (jgbl.onlnrlbk)
			{	/* The child process borrowed crit from the parent (in "mur_forward_multi_proc"). Reclaim it */
				csa = rctl->csa;
				assert(csa->hold_onto_crit);	/* would have been set at "mur_open_files" time */
				assert(csa->now_crit);		/* would have been set at "mur_open_files" time */
				assert(csa->nl->in_crit != process_id);
				assert(FALSE == is_proc_alive(shm_rctl->owning_pid, 0));
				assert(csa->nl->in_crit == shm_rctl->owning_pid);
				csa->nl->in_crit = process_id;	/* reset pid back to parent process now that owning child is done */
				assert(csa->nl->onln_rlbk_pid == shm_rctl->owning_pid);
				csa->nl->onln_rlbk_pid = process_id;
			}
		}
	}
	return mur_merge_sort_extfmt();
}

void mur_shm_forw_token_add(forw_multi_struct *forw_multi, reg_ctl_list *rctl, boolean_t is_new)
{
	shm_forw_multi_t	*sfm;
	que_ent_ptr_t		hash_start, hash_elem, free_elem;
	gtm_uint64_t		hash;
	boolean_t		found, release_latch;
	int			hash_index, num_reg_seen_forward;
	shm_reg_ctl_t		*shm_rctl_start, *shm_rctl;
	int			rctl_index;

	assert(multi_proc_in_use);	/* or else we should not have been called */
	if (mur_options.rollback || (!mur_options.update && !jgbl.mur_extract))
	{	/* In case of ROLLBACK, no need to maintain shm_forw_multi in this case. Each of the children processes know
		 * for sure if this is a GOOD_TN or BROKEN_TN or LOST_TN even without communicating amongst each other.
		 * This is because murgbl.losttn_seqno marks the boundary between GOOD_TN and LOST_TN. And BROKEN_TN is known
		 * in backward processing phase itself.
		 * In case of SHOW or VERIFY (except EXTRACT), there is no need to do this processing since we are
		 * interested only in GOOD_TN or BROKEN_TN, not LOST_TN. So don't maintain shm_forw_multi in that case too.
		 */
		return;
	}
	if (is_new)
	{
		GRAB_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
		assert(release_latch);
		/* Check if token is already present in shared memory */
		COMPUTE_HASH_INT8(&forw_multi->token, hash);
		assert(mur_forw_mp_hash_buckets);
		hash_index = hash % mur_forw_mp_hash_buckets;
		hash_start = &mur_shm_hdr->hash_bucket_start[hash_index];
		hash_elem = hash_start;
		found = FALSE;
		do
		{
			hash_elem = (que_ent_ptr_t)((sm_uc_ptr_t)hash_elem + hash_elem->fl);
			if (hash_elem == hash_start)
				break;
			assert(0 == OFFSETOF(shm_forw_multi_t, free_chain));
			assert(SIZEOF(sfm->free_chain) == OFFSETOF(shm_forw_multi_t, same_hash_chain));
			sfm = (shm_forw_multi_t *)((sm_uc_ptr_t)hash_elem - SIZEOF(sfm->free_chain));
			if ((sfm->token == forw_multi->token) && (!mur_options.rollback || (forw_multi->time == sfm->time)))
			{
				found = TRUE;
				break;
			}
		} while (TRUE);
		if (!found)
		{	/* Allocate a "shm_forw_multi_t" structure */
#			ifdef MUR_DEBUG
			verify_queue((que_head_ptr_t)&mur_shm_hdr->forw_multi_free);
			verify_queue((que_head_ptr_t)hash_start);
#			endif
			free_elem = remqh(&mur_shm_hdr->forw_multi_free);
			assert(NULL != free_elem); /* we have allocated enough shm_forw_multi_t structs to cover the # of regions */
			sfm = (shm_forw_multi_t *)free_elem;
			sfm->token = forw_multi->token;
#			ifdef MUR_DEBUG
			fprintf(stderr, "Pid = %d : Allocating : token = %llu\n", process_id, (long long int)sfm->token);
#			endif
			sfm->time = forw_multi->time;
			sfm->recstat = forw_multi->recstat;
			sfm->num_reg_total = forw_multi->num_reg_total;
			sfm->num_reg_seen_backward = forw_multi->num_reg_seen_backward;
			assert(1 == forw_multi->num_reg_seen_forward);
			sfm->num_reg_seen_forward = 0;	/* will be incremented at end of this function */
			sfm->num_procs = 1;
			sfm->hash_index = hash_index;
			insqt((que_ent_ptr_t)&sfm->same_hash_chain, hash_start);
		} else
		{	/* Cannot use sfm->num_reg_seen_forward++ even if inside grab_latch because of a later INCR_CNT
			 * done below outside any grab_latch window (for the "!is_new" case).
			 */
			sfm->num_procs++;
		}
		forw_multi->shm_forw_multi = sfm;
		REL_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
	} else
		sfm = forw_multi->shm_forw_multi;
#	ifdef MUR_DEBUG
	fprintf(stderr, "Pid = %d : Incrementing : token = %llu : forward = %d\n",
		process_id, (long long int)sfm->token, sfm->num_reg_seen_forward);
#	endif
	assert(sfm->token == forw_multi->token);
	assert(sfm->time == forw_multi->time);
	assert(sfm->num_reg_total == forw_multi->num_reg_total);
	assert(sfm->num_reg_seen_backward == forw_multi->num_reg_seen_backward);
	shm_rctl_start = mur_shm_hdr->shm_rctl_start;
	rctl_index = rctl - &mur_ctl[0];
	shm_rctl = &shm_rctl_start[rctl_index];
	assert(process_id == shm_rctl->owning_pid);
	assert(NULL == shm_rctl->shm_forw_multi);
	shm_rctl->shm_forw_multi = sfm;
	/* Adjust shm value of "recstat" based on individual region's perspective. Note that if shm value is LOST_TN
	 * and individual region's value is GOOD_TN, then LOST_TN should prevail. Note that we don't hold a lock when doing
	 * this update to sfm->recstat (a shared memory location). This is because if any update happens to sfm->recstat,
	 * it will be for a GOOD_TN -> LOST_TN transition and it is okay if multiple processes do this overwrite at the same time.
	 */
	assert((BROKEN_TN != forw_multi->recstat) || (BROKEN_TN == sfm->recstat));
	assert((BROKEN_TN != sfm->recstat) || (BROKEN_TN == forw_multi->recstat));
	MUR_SHM_FORW_MULTI_RECSTAT_UPDATE_IF_NEEDED(sfm, rctl);
	/* Update sfm->num_reg_seen_forward LAST. This is because once this becomes == sfm->num_reg_seen_backward, all processes
	 * will start playing forward this multi-region TP transaction parallely across multiple regions and they need to see
	 * the exact same value of sfm->shm_forw_multi->recstat. Doing this INCR_CNT anywhere before could cause some processes
	 * to see this transaction as GOOD_TN and some others to see it as LOST_TN causing correctness issues in recovery/rollback.
	 */
	INCR_CNT(&sfm->num_reg_seen_forward, &sfm->mur_latch);
	assert(sfm->num_reg_seen_forward <= sfm->num_reg_seen_backward);
	return;
}

void mur_shm_forw_token_remove(reg_ctl_list *rctl)
{
	forw_multi_struct	*forw_multi;
	int			num_procs_left;
	shm_forw_multi_t	*sfm;
	que_ent_ptr_t		que_ent;
	shm_reg_ctl_t		*shm_rctl_start, *shm_rctl;
	int			rctl_index;
	boolean_t		release_latch;
#	ifdef MUR_DEBUG
	int			num_procs_start;
#	endif

	assert(multi_proc_in_use);	/* or else we should not have been called */
	forw_multi = rctl->forw_multi;
	sfm = forw_multi->shm_forw_multi;
	if (NULL == sfm)
	{	/* We did not maintain shm_forw_multi for reasons commented in "mur_shm_forw_token_add". No need for any cleanup */
		return;
	}
	shm_rctl_start = mur_shm_hdr->shm_rctl_start;
	rctl_index = rctl - &mur_ctl[0];
	shm_rctl = &shm_rctl_start[rctl_index];
	assert(process_id == shm_rctl->owning_pid);
	assert(sfm == shm_rctl->shm_forw_multi);
	shm_rctl->shm_forw_multi = NULL;
	forw_multi->num_reg_seen_forward--;
	if (0 == forw_multi->num_reg_seen_forward)
	{	/* This process is done with removing all of its regions for this multi-region TP transaction */
#		ifdef MUR_DEBUG
		num_procs_start = sfm->num_procs;
#		endif
		num_procs_left = DECR_CNT(&sfm->num_procs, &sfm->mur_latch);
#		ifdef MUR_DEBUG
		fprintf(stderr, "Pid = %d : Freeing : token = %llu : num_procs_start = %d : num_procs_left = %d\n",
			process_id, (long long int)sfm->token, num_procs_start, num_procs_left);
#		endif
		if (!num_procs_left)
		{	/* This is the last process to remove all of its region for this multi-region TP transaction.
			 * Clean up the shm_forw_multi structure from shared memory.
			 */
			GRAB_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
			assert(release_latch);
			assert(0 == sfm->num_procs);
			que_ent = &sfm->same_hash_chain;
			assert(que_ent->fl);
			que_ent = (que_ent_ptr_t)((sm_uc_ptr_t)que_ent + que_ent->fl);
			remqt(que_ent);
#			ifdef MUR_DEBUG
			fprintf(stderr, "Pid = %d : Freeing : token = %llu\n", process_id, (long long int)sfm->token);
			verify_queue((que_head_ptr_t)&mur_shm_hdr->forw_multi_free);
#			endif
			insqt(&sfm->free_chain, &mur_shm_hdr->forw_multi_free);
#			ifdef MUR_DEBUG
			verify_queue((que_head_ptr_t)&sfm->free_chain);
#			endif
			REL_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
		}
	}
	return;
}
