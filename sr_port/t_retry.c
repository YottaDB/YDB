/****************************************************************
 *								*
 *	Copyright 2001, 2002  Fidelity Information Services, Inc*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#if defined(VMS) && defined(DEBUG)
#include <descrip.h>
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "cdb_sc.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "gdsbml.h"
#include "ccp.h"
#include "error.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h and cws_insert.h */
#include "tp.h"
#include "tp_frame.h"
#include "sleep_cnt.h"
#include "t_retry.h"
#include "format_targ_key.h"
#include "send_msg.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "cws_insert.h"
#include "wcs_mm_recover.h"
#include "wcs_sleep.h"
#include "have_crit.h"
#include "gdsbgtr.h"		/* for the BG_TRACE_PRO macros */
#include "wcs_backoff.h"
#include "tp_restart.h"
#include "gtm_ctype.h"		/* for ISALPHA_ASCII */
#include "anticipatory_freeze.h"
#include "wcs_recover.h"
#include "wbox_test_init.h"
#ifdef GTM_TRIGGER
#include "gtm_trigger_trc.h"
#endif
#ifdef UNIX
#include "gvcst_protos.h"
#include "gtmimagename.h"
#include "caller_id.h"
#endif
#ifdef DEBUG
#include "repl_msg.h"
#include "gtmsource.h"
#endif

/* In mu_reorg if we are in gvcst_bmp_mark_free, we actually have a valid gv_target. Find its root before the next iteration
 * in mu_reorg.
 */
#define WANT_REDO_ROOT_SEARCH								\
			(	(NULL != gv_target)					\
			     && (DIR_ROOT != gv_target->root)				\
			     && !redo_root_search_done					\
			     && !TREF(in_gvcst_redo_root_search)			\
			     && !mu_reorg_upgrd_dwngrd_in_prog				\
			     && (!TREF(in_gvcst_bmp_mark_free) || mu_reorg_process)	\
			)

GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	short			crash_count;
GBLREF	uint4			dollar_tlevel;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	tp_frame		*tp_pointer;
GBLREF	trans_num		start_tn;
GBLREF	unsigned char		cw_set_depth, cw_map_depth, t_fail_hist[CDB_MAX_TRIES];
GBLREF	boolean_t		mu_reorg_process;
GBLREF	boolean_t		mu_reorg_upgrd_dwngrd_in_prog;
GBLREF	unsigned int		t_tries;
GBLREF	uint4			t_err;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		is_dollar_incr;
GBLREF	uint4			update_trans;
#ifdef GTM_TRIGGER
GBLREF	boolean_t		skip_INVOKE_RESTART;
#endif

#ifdef DEBUG
GBLDEF	unsigned char		t_fail_hist_dbg[T_FAIL_HIST_DBG_SIZE];
GBLDEF	unsigned int		t_tries_dbg;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	boolean_t		mupip_jnl_recover;
#endif

#ifdef UNIX
GBLREF	boolean_t		is_updproc;
GBLREF	boolean_t		need_kip_incr;
GBLREF	sgmnt_addrs		*kip_csa;
GBLREF	jnlpool_addrs		jnlpool;
#endif

#ifdef UNIX
error_def(ERR_DBROLLEDBACK);
error_def(ERR_GVFAILCORE);
error_def(ERR_REPLONLNRLBK);
#endif
error_def(ERR_GBLOFLOW);
error_def(ERR_GVINCRFAIL);
error_def(ERR_GVIS);
error_def(ERR_GVPUTFAIL);
error_def(ERR_TPRETRY);

void t_retry(enum cdb_sc failure)
{
	tp_frame		*tf;
	unsigned char		*end, buff[MAX_ZWR_KEY_SZ];
	short			tl;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
#	ifdef DEBUG
	unsigned int		tries;
#	endif
	boolean_t		skip_invoke_restart;
	boolean_t		redo_root_search_done = FALSE;
	unsigned int		local_t_tries;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef GTM_TRIGGER
	skip_invoke_restart = skip_INVOKE_RESTART;	/* note down global value in local variable */
	GTMTRIG_ONLY(DBGTRIGR((stderr, "t_retry: entered\n")));
#	else
	skip_invoke_restart = FALSE;	/* no triggers so set local variable to default state */
#	endif
	/* We expect t_retry to be invoked with an abnormal failure code. mupip reorg is the only exception and can pass
	 * cdb_sc_normal failure code in case it finds a global variable existed at start of reorg, but not when it came
	 * into mu_reorg and did a gvcst_search. It cannot confirm this unless it holds crit for which it has to wait
	 * until the final retry which is why we accept this way of invoking t_retry. Assert accordingly.
	 */
	assert((cdb_sc_normal != failure) || mu_reorg_process);
	t_fail_hist[t_tries] = (unsigned char)failure;
	if (mu_reorg_process)
		CWS_RESET;
	DEBUG_ONLY(TREF(donot_commit) = FALSE;)
	csa = cs_addrs;
	cnl = csa ? csa->nl : NULL;	/* making sure we do not try to dereference a NULL pointer */
	if (!dollar_tlevel)
	{
#		ifdef DEBUG
		if (0 == t_tries)
			t_tries_dbg = 0;
		assert(ARRAYSIZE(t_fail_hist_dbg) > t_tries_dbg);
		t_fail_hist_dbg[t_tries_dbg++] = (unsigned char)failure;
		TRACE_TRANS_RESTART(failure);
#		endif
#		ifdef UNIX
		if (cdb_sc_instancefreeze == failure)
		{
			assert(REPL_ALLOWED(csa->hdr)); /* otherwise, a cdb_sc_instancefreeze retry would not have been signalled */
			WAIT_FOR_REPL_INST_UNFREEZE(csa);
		}
#		endif
		/* Even though rollback and recover operate standalone, there are certain kind of restarts that can still happen
		 * either due to whitebox test cases or stomping on our own buffers causing cdb_sc_lostcr/cdb_sc_rmisalign. Assert
		 * accordingly
		 */
		assert(!mupip_jnl_recover || WB_COMMIT_ERR_ENABLED || (CDB_STAGNATE > t_tries));
		SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, cnl, failure);	/* set wc_blocked if cache related status */
		TREF(prev_t_tries) = t_tries;
		TREF(rlbk_during_redo_root) = FALSE;
		switch(t_tries)
		{
			case 0:
				INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_retries_0, 1);
				break;
			case 1:
				INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_retries_1, 1);
				break;
			case 2:
				INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_retries_2, 1);
				break;
			default:
				assert(3 == t_tries);
				INCR_GVSTATS_COUNTER(csa, cnl, n_nontp_retries_3, 1);
				break;
		}
		if (csa->critical)
			crash_count = csa->critical->crashcnt;
		/* If the restart code is something that should not increment t_tries, handle that by decrementing t_tries
		 * for these special codes just before incrementing it unconditionally. Note that this should be done ONLY IF
		 * t_tries is CDB_STAGNATE or higher and not for lower values as otherwise it can cause livelocks (e.g.
		 * because cnl->wc_blocked is set to TRUE, it is possible we end up restarting with cdb_sc_helpedout
		 * without even doing a cache-recovery (due to the fast path in t_end that does not invoke grab_crit in case
		 * of read-only transactions). In this case, not incrementing t_tries causes us to eternally retry
		 * the transaction with no one eventually grabbing crit and doing the cache-recovery).
		 */
		assert(CDB_STAGNATE >= t_tries);
		if (CDB_STAGNATE <= t_tries)
		{
			assert(cdb_sc_bkupss_statemod != failure); /* backup and snapshot state change cannot happen in
								    * final retry as they need crit which is held by us */
			/* The following type of restarts can happen in the final retry.
			 * (a) cdb_sc_jnlstatemod : This is expected because csa->jnl_state is noted from csd->jnl_state only
			 *     if they are different INSIDE crit. Therefore it is possible that in the final retry one might start
			 *     with a stale value of csa->jnl_state which is noticed only in t_end just before commit as a
			 *     result of which we would restart. Such a restart is okay (instead of the checking for jnl state
			 *     change during the beginning of final retry) since jnl state changes are considered infrequent that
			 *     too in the final retry.
			 * (b) cdb_sc_jnlclose : journaling might get turned off in the final retry INSIDE crit while trying to
			 *     flush journal buffer or during extending the journal file (due to possible disk issues) in which
			 *     case we come here with t_tries = CDB_STAGNATE.
			 * (c) cdb_sc_helpedout : cnl->wc_blocked being TRUE as well as file extension in MM (both of which is
			 *     caused due to another process) can happen in final retry with failure status set to cdb_sc_helpedout
			 * (d) cdb_sc_needcrit : See GTM-7004 for how this is possible and why only a max of one such restart
			 *     per non-TP transaction is possible.
			 * (e) cdb_sc_onln_rlbk[1,2] : See comment below as to why we allow online rollback related restarts even
			 *     in the final retry.
			 * (f) cdb_sc_instancefreeze : Instance freeze detected while crit held.
			 * (g) cdb_sc_gvtrootmod2 : Similar to (e).
			 */
			if ((cdb_sc_jnlstatemod == failure) || (cdb_sc_jnlclose == failure) || (cdb_sc_helpedout == failure)
					|| (cdb_sc_needcrit == failure) || (cdb_sc_onln_rlbk1 == failure)
					|| (cdb_sc_onln_rlbk2 == failure) || (cdb_sc_instancefreeze == failure)
					|| (cdb_sc_gvtrootmod2 == failure))
			{
				/* t_tries should never be greater than t_tries_dbg. The only exception is if this is DSE or online
				 * rollback operates with t_tries = CDB_STAGNATE and restarts if wc_blocked is set outside crit.
				 * But that's possible only if white box test cases to induce Phase 1 and Phase 2 errors are set.
				 * So, assert accordingly.
				 */
				assert((t_tries <= t_tries_dbg) UNIX_ONLY(|| (csa->hold_onto_crit && WB_COMMIT_ERR_ENABLED)));
				/* Assert that the same kind of restart code can never occur more than once once we go to the
				 * final retry. The only exception is cdb_sc_helpedout which can happen due to other processes
				 * setting cnl->wc_blocked to TRUE without holding crit.
				 */
				assert(failure == t_fail_hist_dbg[t_tries_dbg - 1]);
				DEBUG_ONLY(
					for (tries = CDB_STAGNATE; tries < t_tries_dbg - 1; tries++)
						assert((t_fail_hist_dbg[tries] != failure) || (cdb_sc_helpedout == failure));
				)
				t_tries = CDB_STAGNATE - 1;
			}
		}
		if (CDB_STAGNATE <= ++t_tries)
		{
			DEBUG_ONLY(TREF(ok_to_call_wcs_recover) = TRUE;)
			if (!csa->hold_onto_crit)
				grab_crit(gv_cur_region);
			else if (cnl->wc_blocked)
			{	/* Possible ONLY for online rollback or DSE that grabs crit during startup and never grabs again.
				 * In such cases grab_crit (such as above) is skipped. As a result wcs_recover is also skipped.
				 * To avoid this, do wcs_recover if wc_blocked is TRUE. But, that's possible only if white box test
				 * cases to induce Phase 1 and Phase 2 errors are set. So, assert accordingly.
				 */
				assert(WB_COMMIT_ERR_ENABLED);
				wcs_recover(gv_cur_region);
				/* Note that if a concurrent Phase 2 error sets wc_blocked to TRUE anytime between now
				 * and the wc_blocked check at the top of t_end, we'll end up restarting and doing wcs_recover
				 * on the next restart.
				 */
			}
#			ifdef UNIX
			if (MISMATCH_ROOT_CYCLES(csa, cnl))
			{	/* We came in to handle a different restart code in the penultimate retry and grab_crit before going
				 * to final retry. As part of grabbing crit, we detected an online rollback. Although we could treat
				 * this as just an online rollback restart and handle it by syncing cycles, but by doing so, we will
				 * loose the information that an online rollback happened when we go back to gvcst_{put,kill}. This
				 * is usually fine except when we are in implicit TP (due to triggers). In case of implicit TP,
				 * gvcst_{put,kill} has specific code to handle online rollback differently than other restart codes
				 * Because of this reason, we don't want to sync cycles but instead continue with the final retry.
				 * t_end/tp_tend/tp_hist will notice the cycle mismatch and will restart (once more) in final retry
				 * with the appropriate cdb_sc code which gvcst_put/gvcst_kill will intercept and act accordingly.
				 * Even if we are not syncing cycles, we need to do other basic cleanup to ensure the final retry
				 * proceeds smoothly.
				 */
				RESET_ALL_GVT_CLUES;
				cw_set_depth = 0;
				cw_map_depth = 0;
				if (WANT_REDO_ROOT_SEARCH)
				{
					gvcst_redo_root_search();
					redo_root_search_done = TRUE;
				}
			}
#			endif
			assert(csa->now_crit);
			DEBUG_ONLY(TREF(ok_to_call_wcs_recover) = FALSE;)
			csd = cs_data;
			if (CDB_STAGNATE == t_tries)
			{
				if (csd->freeze && update_trans)
				{	/* Final retry on an update transaction and region is frozen.
					 * Wait for it to be unfrozen and only then grab crit.
					 */
					GRAB_UNFROZEN_CRIT(gv_cur_region, csa, csd);
				}
			} else
			{
				assert((failure != cdb_sc_helpedout) && (failure != cdb_sc_jnlclose)
					&& (failure != cdb_sc_jnlstatemod) && (failure != cdb_sc_bkupss_statemod)
					&& (failure != cdb_sc_inhibitkills));
				local_t_tries = t_tries;
				if (!csa->hold_onto_crit)
				{
					rel_crit(gv_cur_region);
					t_tries = 0;
				}
				if (NULL == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
					end = &buff[MAX_ZWR_KEY_SZ - 1];
				if (cdb_sc_gbloflow == failure)
				{
					send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_GBLOFLOW, 0, ERR_GVIS, 2, end - buff, buff);
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_GBLOFLOW, 0, ERR_GVIS, 2, end - buff, buff);
				}
				if (IS_DOLLAR_INCREMENT)
				{
					assert(ERR_GVPUTFAIL == t_err);
					t_err = ERR_GVINCRFAIL;	/* print more specific error message */
				}
				UNIX_ONLY(send_msg_csa(CSA_ARG(csa) VARLSTCNT(9) t_err, 2, local_t_tries, t_fail_hist,
						   ERR_GVIS, 2, end-buff, buff, ERR_GVFAILCORE));
#ifdef DEBUG
				/* Core is not needed. We intentionally create this error. */
				if (!gtm_white_box_test_case_enabled)
#endif
				UNIX_ONLY(gtm_fork_n_core());
				VMS_ONLY(send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) t_err, 2, local_t_tries, t_fail_hist,
						   ERR_GVIS, 2, end-buff, buff));
				rts_error_csa(CSA_ARG(csa) VARLSTCNT(8) t_err, 2, local_t_tries, t_fail_hist, ERR_GVIS, 2, end-buff,
						buff);
			}
		}
		CHECK_MM_DBFILEXT_REMAP_IF_NEEDED(csa, gv_cur_region);
		if ((cdb_sc_blockflush == failure) && !CCP_SEGMENT_STATE(cnl, CCST_MASK_HAVE_DIRTY_BUFFERS))
		{
			assert(csa->hdr->clustered);
			CCP_FID_MSG(gv_cur_region, CCTR_FLUSHLK);
			ccp_userwait(gv_cur_region, CCST_MASK_HAVE_DIRTY_BUFFERS, 0, cnl->ccp_cycle);
		}
		cw_set_depth = 0;
		cw_map_depth = 0;
		/* In case triggers are supported, make sure we start with latest copy of file header's db_trigger_cycle
		 * to avoid unnecessary cdb_sc_triggermod type of restarts.
		 */
		GTMTRIG_ONLY(csa->db_trigger_cycle = csa->hdr->db_trigger_cycle);
		GTMTRIG_ONLY(DBGTRIGR((stderr, "t_retry: csa->db_trigger_cycle updated to %d\n", csa->db_trigger_cycle)));
		start_tn = csa->ti->curr_tn;
		/* Note: If gv_target was NULL before the start of a transaction and the only operations done inside the transaction
		 * are trigger deletions causing bitmap free operations which got restarted due to a concurrent update, we can
		 * reach here with gv_target being NULL.
		 */
		if (NULL != gv_target)
			gv_target->clue.end = 0;
#		ifdef UNIX
		if ((cdb_sc_onln_rlbk1 == failure) || (cdb_sc_onln_rlbk2 == failure))
		{	/* restarted due to online rollback */
			if (!redo_root_search_done)
				RESET_ALL_GVT_CLUES;
			if (!TREF(in_gvcst_bmp_mark_free) || mu_reorg_process)
			{	/* Handle cleanup beyond just resetting clues */
				if (cdb_sc_onln_rlbk2 == failure)
				{
					if (IS_MCODE_RUNNING || TREF(issue_DBROLLEDBACK_anyways))
					{	/* We are in Non-TP and an online rollback too the database to a prior state. If we
						 * are in M code OR the caller has asked us to issue the DBROLLEDBACK rts_error
						 * unconditionally (MUPIP LOAD for eg.), then issue the DBROLLEDBACK. If this is M
						 * code we also increment $ZONLNRLBK ISV and do other necessary cleanup before
						 * issuing the rts_error. Instead of checking for M code, do the cleanup anyways
						 */
						assert(!is_updproc);
						(TREF(dollar_zonlnrlbk))++;
						/* Since "only_reset_clues_if_onln_rlbk" is FALSE, we are NOT in the second phase of
						 * KILL. So, assert that kip_csa is still NULL
						 */
						assert(NULL == kip_csa);
						rts_error_csa(CSA_ARG(csa) VARLSTCNT(1) ERR_DBROLLEDBACK);
					}
				}
				assert(!redo_root_search_done);
				if (WANT_REDO_ROOT_SEARCH)
					gvcst_redo_root_search();
				if (is_updproc)
					rts_error_csa(CSA_ARG(csa) VARLSTCNT(1) ERR_REPLONLNRLBK);
			}
#			ifdef DEBUG
			else
			{	/* Detected ONLINE ROLLBACK during second phase of KILLs in which case we don't want to do increment
				 * $ZONLNRLBK or SYNC cycles. Instead we will stop the second phase of the KILLs and return to the
				 * caller to continue with the next transaction at which point we will detect ONLINE ROLLBACK again
				 * and take the appropriate action.
				 * Note: as long as we are in Non-TP, kip_csa will be NULL in second phase of KILL. Only exception
				 * is if we started out as TP and did KILLs and after the commit, invoked gvcst_bmp_mark_free to
				 * complete the second phase of the KILL. So, assert accordingly.
				 */
				assert((NULL != kip_csa) || ((NULL != sgm_info_ptr) && (NULL != sgm_info_ptr->kip_csa)));
				/* Note: DECR_KIP done by gvcst_kill (in case of Non-TP) or op_tcommit (in case of TP) takes care
				 * of resetting kip_csa and decrementing cs_data->kill_in_prog. So, we don't need to do it here
				 * explicitly.
				 */
			}
#			endif
		}
		if (cdb_sc_gvtrootmod == failure)	/* failure signaled by gvcst_kill */
		{	/* If "gvcst_redo_root_search" has not yet been invoked in t_retry, do that now */
			assert(NULL != gv_target);
			if (!redo_root_search_done && (NULL != gv_target) && (DIR_ROOT != gv_target->root))
				gvcst_redo_root_search();
		}
		if (cdb_sc_gvtrootmod2 == failure)
		{
			if (!redo_root_search_done)
				RESET_ALL_GVT_CLUES;
			/* It is possible for a read-only transaction to release crit after detecting gvtrootmod2, during which time
			 * yet another root block could have moved. In that case, the MISMATCH_ROOT_CYCLES check would have
			 * already done the redo_root_search.
			 */
			assert(!redo_root_search_done || !update_trans);
			if (WANT_REDO_ROOT_SEARCH)
			{	/* Note: An online rollback can occur DURING gvcst_redo_root_search, which can remove gbls from db,
				 * leading to gv_target->root being 0, even though failure code is not cdb_sc_onln_rlbk2
				 */
				gvcst_redo_root_search();
			}
		}
#		endif
	} else
	{	/* for TP, do the minimum; most of the logic is in tp_retry, because it is also invoked directly from t_commit */
		assert(failure == t_fail_hist[t_tries]);
		assert((NULL == csa) || (NULL != csa->hdr));	/* both csa and csa->hdr should be NULL or non-NULL. */
		if (NULL != csa)
		{
			SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED(csa, cnl, failure);
			TP_RETRY_ACCOUNTING(csa, cnl);
		} else /* csa can be NULL if cur_reg is not open yet (cdb_sc_needcrit) */
			assert((CDB_STAGNATE == t_tries) && (cdb_sc_needcrit == failure));
		if (NULL != gv_target)
		{
			if (cdb_sc_blkmod != failure)
				TP_TRACE_HIST(CR_BLKEMPTY, gv_target);
			gv_target->clue.end = 0;
		} else /* only known case of gv_target being NULL is if t_retry is done from gvcst_init. assert this below */
			assert((CDB_STAGNATE <= t_tries) && ((cdb_sc_needcrit == failure) || have_crit(CRIT_HAVE_ANY_REG)));
		if (!skip_invoke_restart)
		{
			GTMTRIG_ONLY(DBGTRIGR((stderr, "t_retry: invoking restart logic (INVOKE_RESTART)\n")));
			INVOKE_RESTART;
		} else	/* explicit trigger update caused implicit tp wrap so should return to caller without rts_error */
		{
			GTMTRIG_ONLY(DBGTRIGR((stderr, "t_retry: invoking tp_restart directly\n")));
			tp_restart(1, !TP_RESTART_HANDLES_ERRORS);
		}
	}
}
