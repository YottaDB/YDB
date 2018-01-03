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

#include "gtm_inet.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "min_max.h"
#include "gdsblkops.h"
#include "gdsbml.h"
#include "gdskill.h"
#include "copy.h"
#include "interlock.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "io.h"
#include "gtmsecshr.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "is_proc_alive.h"
#include "aswp.h"
#include "util.h"
#include "compswap.h"
#include "mutex.h"
#include "repl_instance.h"	/* needed for JNLDATA_BASE_OFF macro */
#include "mupipbckup.h"		/* needed for backup_block prototype */
#include "cert_blk.h"		/* for CERT_BLK_IF_NEEDED macro */
#include "relqueopi.h"		/* for INSQTI and INSQHI macros */
#include "caller_id.h"
#include "sec_shr_blk_build.h"
#include "add_inter.h"
#include "send_msg.h"	/* for send_msg prototype */
#include "secshr_db_clnup.h"
#include "gdsbgtr.h"
#include "memcoherency.h"
#include "shmpool.h"
#include "wbox_test_init.h"
#include "db_snapshot.h"
#include "muextr.h"
#include "mupip_reorg.h"
#include "op.h"
#include "dpgbldir.h"		/* for "get_next_gdr" */
#include "t_abort.h"
#include "have_crit.h"

#define FLUSH 1

GBLREF	gd_region		*gv_cur_region;		/* for the LOCK_HIST macro in the RELEASE_BUFF_UPDATE_LOCK macro */
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	jnlpool_addrs_ptr_t	jnlpool_head;
GBLREF	sgm_info		*first_sgm_info;	/* List of all regions (unsorted) in TP transaction */
GBLREF	sgm_info		*first_tp_si_by_ftok;	/* List of READ or UPDATED regions sorted on ftok order */
GBLREF	sgmnt_addrs 		*kip_csa;
GBLREF	uint4			process_id;
GBLREF	sgmnt_addrs		*cs_addrs;

#ifdef DEBUG
GBLREF	volatile boolean_t	in_wcs_recover; /* TRUE if in "wcs_recover" */
GBLREF	uint4			update_trans;
GBLREF	boolean_t		dse_running;
#endif

error_def(ERR_WCBLOCKED);

/* secshr_db_clnup can be called with one of the following three values for "secshr_state"
 *
 * 	a) NORMAL_TERMINATION   --> We are called from the exit-handler for precautionary cleanup.
 * 				    We should NEVER be in the midst of a database update in this case.
 * 	b) COMMIT_INCOMPLETE    --> We are called from t_commit_cleanup.
 * 				    We should ALWAYS be in the midst of a database update in this case.
 *
 * If we are in the midst of a database update, then depending on where we are in the commit logic
 * 	we need to ROLL-BACK (undo the partial commit) or ROLL-FORWARD (complete the partial commit) the database update.
 * t_commit_cleanup handles the ROLL-BACK and secshr_db_clnup handles the ROLL-FORWARD
 *
 * For all error conditions in the database commit logic, t_commit_cleanup gets control first.
 * If then determines whether to do a ROLL-BACK or a ROLL-FORWARD.
 * If a ROLL-BACK needs to be done, then t_commit_cleanup handles it all by itself and we will not come here.
 * If a ROLL-FORWARD needs to be done, then t_commit_cleanup invokes secshr_db_clnup.
 * 	In this case, secshr_db_clnup will be called with a "secshr_state" value of "COMMIT_INCOMPLETE".
 *
 * Irrespective of whether we are in the midst of a database commit or not, t_commit_cleanup does not get control.
 * Since the process can POSSIBLY be in the midst of a database update while it was STOP/IDed,
 * 	the logic for determining whether it is a ROLL-BACK or a ROLL-FORWARD needs to also be in secshr_db_clnup.
 * If it is determined that a ROLL-FORWARD needs to be done, secshr_db_clnup takes care of it by itself.
 * But if a ROLL-BACK needs to be done, then secshr_db_clnup DOES NOT invoke t_commit_cleanup.
 * Instead it sets cnl->wc_blocked to TRUE thereby ensuring the next process that gets CRIT does a cache recovery
 * 	which will take care of doing more than the ROLL-BACK that t_commit_cleanup would have otherwise done.
 *
 * In order to understand how a ROLL-BACK or ROLL-FORWARD decision is made, the commit flow needs to be understood first.
 * The commit logic flow in t_end & tp_tend can be captured as follows.
 * Note that
 *	a) In t_end there is only one region in the "For each region" loop. And "si->update_trans" == "update_trans".
 *	b) All jnlpool steps below apply only if replication is turned ON.
 *	c) For MM, the jnl and db commits happen inside crit i.e. no phase2 outside crit like BG.
 *
 * (CMT01)  Get crit on all regions (UPDATED || NON-UPDATED)
 * (CMT02)  Get crit on jnlpool
 * (CMT03)  Reserve space in JNLPOOL for journal records (UPDATE_JPL_RSRV_WRITE_ADDR macro)
 *          For each UPDATED region
 *          {
 * (CMT04)      csd->trans_hist.early_tn = csd->trans_hist.curr_tn + 1;
 * (CMT05)      csa->t_commit_crit = T_COMMIT_CRIT_PHASE0;
 *          }
 *          For each UPDATED & JOURNALED region
 *          {
 * (CMT06)      Reserve space in JNLBUFF for journal records PHASE1 (UPDATE_JRS_RSRV_FREEADDR macro)
 *          }
 * (CMT06a) If (MM or (BG && jnlbuff-overflow)) Do Step CMT16 here instead of later
 * (CMT06b) If (Non-TP && MM) Do Step CMT17 here instead of later
 * (CMT07)  jnlpool_ctl->jnl_seqno++; jnlpool_ctl->strm_seqno[]++ if supplementary; (SET_JNL_SEQNO macro)
 *          For each (UPDATED || NON-UPDATED) region
 *          {
 *              If this is an UPDATED region
 *              {
 * (CMT08)          csa->t_commit_crit = T_COMMIT_CRIT_PHASE1; cnl->update_underway_tn = csd->trans_hist.curr_tn;
 * (CMT09)          If replication is ON, csd->reg_seqno = jnlpool_ctl->jnl_seqno + 1; csd->strm_reg_seqno[] = xxx
 *                  if (cw_set is non-NULL for this region)	// i.e. not duplicate set(s)
 *                  {
 *                      For each cw-se-element
 *                      {
 * (CMT10)                  Commit PHASE1 // bg_update_phase1 or mm_update
 * (CMT10a)                 If (BG && IS_BG_PHASE2_COMMIT_IN_CRIT(cse, mode)) Do Step CMT16 and CMT18 here instead of later
 *                      }
 *                  }
 * (CMT11)          si->update_trans |= UPDTRNS_TCOMMIT_STARTED_MASK;
 * (CMT12)          csd->trans_hist.curr_tn++;
 * (CMT13)          csa->t_commit_crit = T_COMMIT_CRIT_PHASE2;
 *              }
 * (CMT14)      Release crit on region
 *          }
 * (CMT15)  Release crit on jnlpool
 *          For each (UPDATED && JOURNALED) region
 *          {
 * (CMT16)      If (BG) Write journal records in JNLBUFF & JNLPOOL. PHASE2 (outside crit). Mark write complete in JNLBUFF.
 *          }
 * (CMT17)  If (BG) Mark journal record write complete in JNLPOOL.
 *          For each UPDATED region
 *          {
 *              if (cw_set is non-NULL for this region)	// i.e. not duplicate set(s)
 *              {
 * (CMT18)          Commit all cw-set-elements of this region PHASE2 (outside crit)	// bg_update_phase2
 *              }
 * (CMT19)      csa->t_commit_crit = FALSE;
 *          }
 *
 * If a transaction has proceeded to Step (CMT08) for at least one region, then "tp_update_underway" is set to TRUE
 * and the transaction cannot be rolled back but has to be committed. Otherwise the transaction is rolled back.
 * This is for the case where the process encounters an error in the midst of commit. In this case, we are guaranteed
 * a clean recovery by secshr_db_clnup() & wcs_recover(). Database integrity is guaranteed.
 * But if the process in the midst of commit is abnormally killed (e.g. kill -9), then "mutex_salvage",
 * "jnl_phase2_salvage" and "repl_phase2_salvage" try to cleanup various pieces of the leftover mess but
 * we are not guaranteed db integrity (i.e. CMT18 cannot be currently easily redone by a different process).
 */

void secshr_db_clnup(enum secshr_db_state secshr_state)
{
	boolean_t		is_bg, is_exiting;
	boolean_t		update_underway;
	cache_que_heads_ptr_t	cache_state;
	cache_rec_ptr_t		cr;
	char			*wcblocked_ptr;
	gd_addr			*gd_hdr;
	gd_region		*reg, *reg_top, *save_gv_cur_region, *repl_reg = NULL;
	gtm_uint64_t		argarray[SECSHR_ACCOUNTING_MAX_ARGS];
	int			numargs;
	jbuf_rsrv_struct_t	*jrs;
	jnl_buffer_ptr_t	jbp;
	node_local_ptr_t	cnl;
	sm_uc_ptr_t		blk_ptr;
	blk_hdr_ptr_t		blk_hdr_ptr;
	jnlpool_addrs_ptr_t	save_jnlpool, update_jnlpool = NULL;
	jnlpool_ctl_ptr_t	jpl;
	pid_t			pid;
	sgm_info		*si;
	sgmnt_addrs		*csa, *repl_csa;
	sgmnt_data_ptr_t	csd;
#	ifdef DEBUG
	sgm_info		*jnlpool_si;
	sgmnt_addrs		*jnlpool_csa;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	save_gv_cur_region = gv_cur_region;	/* save it for use at function end in case code in between changes this variable */
	assert((NORMAL_TERMINATION == secshr_state) || (COMMIT_INCOMPLETE == secshr_state));
	is_exiting = (NORMAL_TERMINATION == secshr_state);
	save_jnlpool = jnlpool;
	update_underway = FALSE;
	if (dollar_tlevel)
	{	/* Determine update_underway for TP transaction. A similar check is done in t_commit_cleanup as well.
		 * Regions are committed in the ftok order using "first_tp_si_by_ftok". Also crit is released on each
		 * region as the commit completes. Take that into account while determining if update is underway.
		 */
		for (si = first_tp_si_by_ftok; NULL != si; si = si->next_tp_si_by_ftok)
		{
			csa = si->tp_csa;
			assert(NULL != csa);
			if (NULL == csa->nl)
			{	/* Possible for example if "mur_close_files" was called in "mupip_exit_handler" (which does
				 * a "gds_rundown" of all open regions) BEFORE "secshr_db_clnup" gets called. In that case,
				 * even if "si" and "csa" are accessible, db shared memory is not and so we should not
				 * attempt to finish any commits that might seem unfinished in case this process got a SIG-15
				 * and handled it as part of "deferred_signal_handler" in "t_end"/"tp_tend".
				 */
				continue;
			}
			assert(csa->regcnt);
			if (NULL != si->first_cw_set)
			{
				if (csa->jnlpool && (jnlpool != csa->jnlpool))
				{
					assert(!update_jnlpool || (!csa->jnlpool || (update_jnlpool == csa->jnlpool)));
#					ifdef DEBUG
					jnlpool_si = si;
					jnlpool_csa = csa;
#					endif
					jnlpool = csa->jnlpool;
				}
				if (!update_jnlpool && REPL_ALLOWED(csa))
					update_jnlpool = jnlpool;
				if ( T_UPDATE_UNDERWAY(csa))
				{
					update_underway = TRUE;
					break;
				}
			}
			if (UPDTRNS_TCOMMIT_STARTED_MASK & si->update_trans)
			{	/* Two possibilities.
				 *	(a) case of duplicate set not creating any cw-sets but updating db curr_tn++.
				 *	(b) Have completed commit for this region and have released crit on this region.
				 *		(in a potentially multi-region TP transaction).
				 * In either case, update is underway and the transaction cannot be rolled back.
				 */
				update_underway = TRUE;
				break;
			}
		}
		if (!update_jnlpool)
			update_jnlpool = jnlpool;
	} else if ((NULL != save_gv_cur_region) && save_gv_cur_region->open && (NULL != cs_addrs))
	{	/* Determine update_underway for non-TP transaction */
		assert(!(cs_addrs->now_crit && (UPDTRNS_TCOMMIT_STARTED_MASK & update_trans))
									|| T_UPDATE_UNDERWAY(cs_addrs));
		if (T_UPDATE_UNDERWAY(cs_addrs))
			update_underway = TRUE;
		update_jnlpool = (cs_addrs && cs_addrs->jnlpool) ? cs_addrs->jnlpool : jnlpool;
	}
	assert(is_exiting || update_underway);
	if (update_underway)
	{	/* Now that we know we were in the midst of commit (non-TP or TP) when we encountered an error
		 * (i.e. "is_exiting" == FALSE) OR when we decided to exit (i.e. "is_exiting" == TRUE)
		 * finish the commit first. Need to go through regions in ftok order to avoid crit deadlocks.
		 */
		repl_reg = update_jnlpool ? update_jnlpool->jnlpool_dummy_reg : NULL;
		repl_csa = ((NULL != repl_reg) && repl_reg->open) ? REG2CSA(repl_reg) : NULL;
		if (dollar_tlevel)
			for (si = first_tp_si_by_ftok; NULL != si; si = si->next_tp_si_by_ftok)
				secshr_finish_CMT08_to_CMT14(si->tp_csa, update_jnlpool); /* Roll forward steps CMT08 thru CMT14 */
		else
		{
			csa = cs_addrs;
			secshr_finish_CMT08_to_CMT14(csa, update_jnlpool);	/* Roll forward steps CMT08 thru CMT14 */
		}
		if ((NULL != repl_csa) && repl_csa->now_crit)
			secshr_rel_crit(repl_reg, IS_EXITING_FALSE, IS_REPL_REG_TRUE);	/* Step CMT15 */
		/* In case of DSE, csa->hold_onto_crit is TRUE and so it is possible the above "secshr_rel_crit"
		 * call did not release crit. Assert accordingly below.
		 */
		assert((0 == have_crit(CRIT_HAVE_ANY_REG)) || dse_running);
		if (dollar_tlevel)
		{
			for (si = first_tp_si_by_ftok; NULL != si; si = si->next_tp_si_by_ftok)
			{
				csa = si->tp_csa;
				jrs = si->jbuf_rsrv_ptr;
				if (NEED_TO_FINISH_JNL_PHASE2(jrs))
					FINISH_JNL_PHASE2_IN_JNLBUFF(csa, jrs);	/* Roll forward CMT16 */
			}
		} else
		{
			jrs = TREF(nontp_jbuf_rsrv);
			if (NEED_TO_FINISH_JNL_PHASE2(jrs))
				FINISH_JNL_PHASE2_IN_JNLBUFF(csa, jrs);	/* Roll forward CMT16 */
		}
		if (NULL != repl_csa)
			FINISH_JNL_PHASE2_IN_JNLPOOL_IF_NEEDED(TRUE, update_jnlpool);	/* Roll forward CMT17 */
		/* Roll forward Step CMT18 and CMT19 */
		if (dollar_tlevel)
			for (si = first_tp_si_by_ftok; NULL != si; si = si->next_tp_si_by_ftok)
				secshr_finish_CMT18_to_CMT19(si->tp_csa);	/* Roll forward Step CMT18 */
		else
			secshr_finish_CMT18_to_CMT19(csa);		/* Roll forward Step CMT18 */
	}
	if (save_jnlpool != jnlpool)
		jnlpool = save_jnlpool;
	if (is_exiting)
	{
		for (gd_hdr = get_next_gdr(NULL); NULL != gd_hdr; gd_hdr = get_next_gdr(gd_hdr))
		{
			for (reg = gd_hdr->regions, reg_top = reg + gd_hdr->n_regions;  reg < reg_top;  reg++)
			{
				if (!reg->open || reg->was_open)
					continue;
				if (!IS_REG_BG_OR_MM(reg))
					continue;
				csa = REG2CSA(reg);
				if (NULL == csa)
					continue;
				assert((reg->read_only && !csa->read_write) || (!reg->read_only && csa->read_write));
				SECSHR_SET_CSD_CNL_ISBG(csa, csd, cnl, is_bg);	/* sets csd/cnl/is_bg */
				numargs = 0;
				if (csa->now_crit)
				{
					numargs = 0;
					SECSHR_ACCOUNTING(numargs, argarray, __LINE__);
					SECSHR_ACCOUNTING(numargs, argarray, sac_secshr_db_clnup);
					SECSHR_ACCOUNTING(numargs, argarray, process_id);
					SECSHR_ACCOUNTING(numargs, argarray, secshr_state);
					SECSHR_ACCOUNTING(numargs, argarray, csd->trans_hist.curr_tn);
					secshr_send_DBCLNUPINFO_msg(csa, numargs, argarray);
				}
				/* If we hold any latches in the node_local area, release them. Note we do not check
				 * db_latch here because it is never used by the compare and swap logic but rather
				 * the aswp logic. Since it is only used for the 3 state cache record lock and
				 * separate recovery exists for it, we do not do anything with it here.
				 */
				RELEASE_LATCH_IF_OWNER(&cnl->wc_var_lock);
				/* Note: In case of "kill -9", cnl->in_wtstart, cnl->wcs_timers & cnl->ref_cnt will stay uncleaned.
				 * a) cnl->in_wtstart, "wcs_flu" will eventually set cnl->wc_blocked and force cache recovery
				 *    which will clear it.
				 * b) cnl->wcs_timers decides if a new flush timer is started. If the 2 processes that have
				 *    pending flush timers are killed abnormally, no new processes will start flush timers for
				 *    the lifetime of the database shared memory. Need a recovery scheme for it.
				 * c) cnl->ref_cnt is mostly for debug purposes (to know an approximate # of processes attached
				 *    to the database and so it is okay if it is not accurate.
				 * if it finds */
				if ((csa->in_wtstart) && (0 < cnl->in_wtstart))
				{
					DECR_CNT(&cnl->in_wtstart, &cnl->wc_var_lock);
					assert(0 < cnl->intent_wtstart);
					if (0 < cnl->intent_wtstart)
						DECR_CNT(&cnl->intent_wtstart, &cnl->wc_var_lock);
				}
				csa->in_wtstart = FALSE;	/* Let wcs_wtstart run for exit processing */
				if (cnl->wcsflu_pid == process_id)
					cnl->wcsflu_pid = 0;
				if (is_bg)
				{
					assert(cnl->sec_size);
					cache_state = csa->acc_meth.bg.cache_state;
					RELEASE_LATCH_IF_OWNER(&cache_state->cacheq_active.latch);
#					ifdef DEBUG
					if (gtm_white_box_test_case_enabled && (reg == gd_hdr->regions)
						&& (WBTEST_SIGTSTP_IN_T_QREAD == gtm_white_box_test_case_number))
					{
						assert((NULL != TREF(block_now_locked))
							&& ((TREF(block_now_locked))->r_epid == process_id));
					}
#					endif
					if (NULL != (cr = TREF(block_now_locked)))	/* done by region to ensure access */
					{	/* The following is potentially thread-specific rather than process-specific */
						TREF(block_now_locked) = NULL;
						RELEASE_LATCH_IF_OWNER(&cr->rip_latch);
						if ((cr->r_epid == process_id) && (0 == cr->dirty) && (0 == cr->in_cw_set))
						{	/* increment cycle for blk number changes (for tp_hist) */
							cr->cycle++;
							cr->blk = CR_BLKEMPTY;
							/* ensure no bt points to this cr for empty blk */
							assert(0 == cr->bt_index);
							/* Don't mess with ownership the I/O may not yet be cancelled.
							 * Ownership will be cleared by whoever gets stuck waiting for the buffer.
							 */
						}
					}
				}
				assert(!T_UPDATE_UNDERWAY(csa));
				if (csa->now_crit || csa->t_commit_crit)
				{
					numargs = 0;
					SECSHR_ACCOUNTING(numargs, argarray, __LINE__);
					SECSHR_ACCOUNTING(numargs, argarray, sac_secshr_db_clnup);
					SECSHR_ACCOUNTING(numargs, argarray, csa->now_crit);
					SECSHR_ACCOUNTING(numargs, argarray, csa->t_commit_crit);
					SECSHR_ACCOUNTING(numargs, argarray, csd->trans_hist.early_tn);
					SECSHR_ACCOUNTING(numargs, argarray, csd->trans_hist.curr_tn);
					SECSHR_ACCOUNTING(numargs, argarray, dollar_tlevel);
					secshr_send_DBCLNUPINFO_msg(csa, numargs, argarray);
				}
				/* The process is exiting. If any kills are in progress (bitmap freeup phase of kill), mark
				 * kill_in_prog as abandoned. Non-TP and TP maintain kill_in_prog information in different
				 * structures so access them appropriately. Note that even for a TP transaction, the bitmap
				 * freeup happens as a non-TP transaction so checking dollar_tlevel is not enough to determine
				 * if we are in TP or non-TP. first_sgm_info is guaranteed to be non-NULL in the case of a TP
				 * transaction that is temporarily running its bitmap freeup phase as a non-TP transaction.
				 * And for true non-TP transactions, first_sgm_info is guaranteed to be NULL. So we use this
				 * for the determination.
				 */
				if (NULL != first_sgm_info)
				{
					si = csa->sgm_info_ptr;
					/* Note that it is possible "si" is NULL in case of a GTM-E-MEMORY error in gvcst_init.
					 * Handle that accordingly in the code below.
					 */
					/* Since the kill process cannot be completed, we need to decerement KIP count
					 * and increment the abandoned_kills count.
					 */
					if (si && (NULL != si->kill_set_head) && (NULL != si->kip_csa))
					{
						assert(csa == si->kip_csa);
						DECR_KIP(csd, csa, si->kip_csa);
						INCR_ABANDONED_KILLS(csd, csa);
					} else
						assert((NULL == si) || (NULL == si->kill_set_head) || (NULL == si->kip_csa));
				} else if (!dollar_tlevel)
				{
					if ((NULL != kip_csa) && (csa == kip_csa))
					{
						assert(0 < kip_csa->hdr->kill_in_prog);
						DECR_KIP(csd, csa, kip_csa);
						INCR_ABANDONED_KILLS(csd, csa);
					}
				}
				if (JNL_ENABLED(csd))
				{
					jbp = csa->jnl->jnl_buff;
					RELEASE_LATCH_IF_OWNER(&jbp->fsync_in_prog_latch);
					if (jbp->io_in_prog_latch.u.parts.latch_pid == process_id)
						RELEASE_SWAPLOCK(&jbp->io_in_prog_latch);
					if (jbp->blocked == process_id)
					{
						assert(csa->now_crit);
						jbp->blocked = 0;
					}
				}
				if (csa->freeze && (csd->freeze == process_id) && !csa->persistent_freeze)
				{
					csd->image_count = 0;
					csd->freeze = 0;
				}
				assert(!csa->t_commit_crit);
				if (is_bg && (csa->wbuf_dqd || csa->now_crit || csa->t_commit_crit))
				{	/* if csa->wbuf_dqd == TRUE, most likely failed during REMQHI in wcs_wtstart
					 * 	or db_csh_get.  cache corruption is suspected so set wc_blocked.
					 * if csa->now_crit is TRUE, someone else should clean the cache, so set wc_blocked.
					 * if csa->t_commit_crit is TRUE, even if csa->now_crit is FALSE, we might need cache
					 * 	cleanup (e.g. cleanup of orphaned cnl->wcs_phase2_commit_pidcnt counter in case
					 * 	a process gets shot in the midst of DECR_WCS_PHASE2_COMMIT_PIDCNT macro before
					 * 	decrementing the shared counter but after committing the transaction otherwise)
					 * 	so set wc_blocked. This case is folded into phase2 cleanup case below.
					 */
					SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
					if (csa->now_crit)
					{
						wcblocked_ptr = WCBLOCKED_NOW_CRIT_LIT;
						BG_TRACE_PRO_ANY(csa, wcb_secshr_db_clnup_now_crit);
					} else if (csa->wbuf_dqd)
					{
						wcblocked_ptr = WCBLOCKED_WBUF_DQD_LIT;
						BG_TRACE_PRO_ANY(csa, wcb_secshr_db_clnup_wbuf_dqd);
					} else
					{
						wcblocked_ptr = WCBLOCKED_PHASE2_CLNUP_LIT;
						BG_TRACE_PRO_ANY(csa, wcb_secshr_db_clnup_phase2_clnup);
					}
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_STR(wcblocked_ptr),
						process_id, &csd->trans_hist.curr_tn, DB_LEN_STR(reg));
				}
				csa->wbuf_dqd = 0;		/* We can clear the flag now */
				if (csa->wcs_pidcnt_incremented)
					DECR_WCS_PHASE2_COMMIT_PIDCNT(csa, cnl);
				if (csa->now_crit)
				{
					if (csd->trans_hist.curr_tn == csd->trans_hist.early_tn - 1)
					{
						assert(WBTEST_ENABLED(WBTEST_JNL_CREATE_FAIL)
							|| WBTEST_ENABLED(WBTEST_SLEEP_IN_WCS_WTSTART) || in_wcs_recover);
						csd->trans_hist.early_tn = csd->trans_hist.curr_tn;
					}
					assert(csd->trans_hist.early_tn == csd->trans_hist.curr_tn);
					secshr_rel_crit(csa->region, IS_EXITING_TRUE, IS_REPL_REG_FALSE);
				}
				if (NULL != csa->shmpool_buffer)
				{
					if ((pid = csa->shmpool_buffer->shmpool_crit_latch.u.parts.latch_pid) == process_id)
					{	/* Tiz our lock. Force recovery to run and release */
						csa->shmpool_buffer->shmpool_blocked = TRUE;
						BG_TRACE_PRO_ANY(csa, shmpool_blkd_by_sdc);
						SET_LATCH_GLOBAL(&csa->shmpool_buffer->shmpool_crit_latch, LOCK_AVAILABLE);
						DEBUG_LATCH(util_out_print("Latch cleaned up", FLUSH));
					} else if ((0 != pid) && (FALSE == is_proc_alive(pid, 0)))
					{	/* Attempt to make it our lock so we can set blocked */
						if (COMPSWAP_LOCK(&csa->shmpool_buffer->shmpool_crit_latch, pid, 0, process_id, 0))
						{	/* Now our lock .. set blocked and release.  */
							csa->shmpool_buffer->shmpool_blocked = TRUE;
							BG_TRACE_PRO_ANY(csa, shmpool_blkd_by_sdc);
							DEBUG_LATCH(util_out_print("Orphaned latch cleaned up", TRUE));
							COMPSWAP_UNLOCK(&csa->shmpool_buffer->shmpool_crit_latch, process_id,
									0, LOCK_AVAILABLE, 0);
						} /* Else someone else took care of it */
					}
				}
				/* All releases done now. Double check latch is really cleared */
				assert(NULL != csa->critical);
				/* as long as csa->hold_onto_crit is FALSE, we should have released crit if we held it at entry */
				assert(!csa->now_crit || csa->hold_onto_crit);
				/* Note: Do not release crit if we still hold it. As we want the next process to grab crit to invoke
				 * "mutex_salvage" (to cleanup stuff) in case we terminate while holding crit. Hence the below line
				 * is commented out.
				 *
				 * RELEASE_LATCH_IF_OWNER(&csa->critical->semaphore);
				 */
				RELEASE_LATCH_IF_OWNER(&csa->critical->crashcnt_latch);
				RELEASE_LATCH_IF_OWNER(&csa->critical->prochead.latch);
				RELEASE_LATCH_IF_OWNER(&csa->critical->freehead.latch);
			}	/* For all regions */
		}	/* For all glds */
		if (jnlpool_head)
		{
			save_jnlpool = jnlpool;
			for (jnlpool = jnlpool_head; jnlpool; jnlpool = jnlpool->next)
			{
				if ((NULL != (repl_reg = jnlpool->jnlpool_dummy_reg)) && repl_reg->open)	/* assignment */
				{
					repl_csa = REG2CSA(repl_reg);
					assert(repl_csa);
					if (repl_csa->now_crit)
						secshr_rel_crit(repl_reg, IS_EXITING_TRUE, IS_REPL_REG_TRUE);
				}
			}
			jnlpool = save_jnlpool;
		}
		/* It is possible we are exiting while in the middle of a transaction (e.g. called through "deferred_signal_handler"
		 * in the DEFERRED_EXIT_HANDLING_CHECK macro). Since exit handling code can start new non-TP transactions
		 * (e.g. for statsdb rundown you need to kill ^%YGS node, for recording mprof stats you need to set a global node)
		 * clean up the effects of any in-progress transaction before the "t_begin" call happens.
		 */
		if (!dollar_tlevel)
		{
			assert(save_gv_cur_region == gv_cur_region);
			if ((NULL != save_gv_cur_region) && save_gv_cur_region->open && IS_REG_BG_OR_MM(save_gv_cur_region))
				t_abort(save_gv_cur_region, &FILE_INFO(save_gv_cur_region)->s_addrs);
		} else
			OP_TROLLBACK(0);
	}
	return;
}
