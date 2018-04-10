/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "cdb_sc.h"
#include "error.h"
#include "iosp.h"		/* for declaration of SS_NORMAL */
#include "jnl.h"
#include <rtnhdr.h>
#include "mv_stent.h"
#include "stack_frame.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"		/* for ATOI */
#include "send_msg.h"
#include "op.h"
#include "io.h"
#include "targ_alloc.h"
#include "getzposition.h"
#include "wcs_recover.h"
#include "tp_unwind.h"
#include "wcs_backoff.h"
#include "rel_quant.h"
#include "wcs_mm_recover.h"
#include "tp_restart.h"
#include "repl_msg.h"		/* for gtmsource.h */
#include "gtmsource.h"		/* for jnlpool_addrs structure definition */
#include "wbox_test_init.h"
#include "gtmimagename.h"
#include "have_crit.h"
#include "anticipatory_freeze.h"
#include "format_targ_key.h"
#include "mupip_reorg_encrypt.h"
#include "process_reorg_encrypt_restart.h"
#ifdef GTM_TRIGGER
#include "gv_trigger.h"
#include "gtm_trigger.h"
#endif
#ifdef DEBUG
#include "caller_id.h"
#endif

GBLDEF	int4			n_pvtmods, n_blkmods;

GBLREF	uint4			dollar_tlevel;
GBLREF	uint4			dollar_trestart;
GBLREF	int			dollar_truth;
GBLREF	mval			dollar_zgbldir;
GBLREF	gd_addr			*gd_header;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	stack_frame		*frame_pointer;
GBLREF	tp_frame		*tp_pointer;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	tp_region		*tp_reg_list;	/* Chained list of regions used in this transaction not cleared on tp_restart */
GBLREF	mv_stent		*mv_chain;
GBLREF	unsigned char		*msp, *stackbase, *stacktop, t_fail_hist[CDB_MAX_TRIES];
GBLREF	sgm_info		*first_sgm_info;
GBLREF	unsigned int		t_tries;
GBLREF	int			process_id;
GBLREF	gd_region		*gv_cur_region;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	jnlpool_addrs_ptr_t	jnlpool_head;
GBLREF	boolean_t		caller_id_flag;
GBLREF	unsigned char		*tpstackbase, *tpstacktop;
GBLREF	trans_num		local_tn;	/* transaction number for THIS PROCESS */
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data		*cs_data;
GBLREF	symval			*curr_symval;
GBLREF	trans_num		tstart_local_tn;	/* copy of global variable "local_tn" at op_tstart time */
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	mstr			extnam_str;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	sgmnt_addrs		*cs_addrs_list;
GBLREF	boolean_t		is_updproc;
GBLREF	sgmnt_addrs		*reorg_encrypt_restart_csa;
#ifdef GTM_TRIGGER
GBLREF	int			tprestart_state;	/* When triggers restart, multiple states possible. See tp_restart.h */
GBLREF	mval			dollar_ztwormhole;	/* Previous value (mval) restored on restart */
GBLREF	mval			dollar_ztslate;
LITREF	mval			literal_null;
#endif

error_def(ERR_GVFAILCORE);
error_def(ERR_REPLONLNRLBK);
error_def(ERR_TLVLZERO);
error_def(ERR_TPFAIL);
error_def(ERR_TPRESTART);
error_def(ERR_TPRETRY);
error_def(ERR_TRESTLOC);
error_def(ERR_TRESTNOT);

CONDITION_HANDLER(tp_restart_ch)
{
	START_CH(TRUE);
	GTMTRIG_ONLY(DBGTRIGR((stderr, "tp_restart_ch: ERROR!! unwinding C frame to return. Error is %d, tprestart_state is %d\n",
			       arg, tprestart_state)));
	/* It is possible that dollar_tlevel at the time of the ESTABLISH_RET was higher than the current dollar_tlevel.
	 * This is because tp_unwind could have decreased dollar_tlevel. Even though dollar_tlevel before the UNWIND done
	 * below is not the same as that at ESTABLISH_RET time, the flow of control happens correctly so tp_restart eventually
	 * bubbles back to the op_tstart at $tlevel=1 and resumes execution. So treat this as an exception and adjust
	 * active_ch->dollar_tlevel so it is in sync with the current dollar_tlevel. This prevents an assert failure in UNWIND.
	 * START_CH would have done a active_ch-- so we need a active_ch[1] to get at the desired active_ch.
	 */
	assert(active_ch[1].dollar_tlevel >= dollar_tlevel);
	DEBUG_ONLY(active_ch[1].dollar_tlevel = dollar_tlevel;)
	UNWIND(NULL, NULL);
}

/* Note that adding a new rts_error in "tp_restart" might need a change to the INVOKE_RESTART macro in tp.h and
 * TPRESTART_ARG_CNT in errorsp.h. See comment in tp.h for INVOKE_RESTART macro for the details.
 */
int tp_restart(int newlevel, boolean_t handle_errors_internally)
{
	unsigned char		*cp;
	unsigned char		*end, buff[MAX_ZWR_KEY_SZ];
	unsigned int		hist_index;
	tp_frame		*tf;
	mv_stent		*mvc;
	tp_region		*tr;
	mval			beganHere;
	sgmnt_addrs		*csa, *jpl_csa;
	int4			num_closed = 0;
	boolean_t		tp_tend_status;
	boolean_t		reset_clues_done = FALSE;
	mstr			gvname_mstr, reg_mstr;
	gd_region		*restart_reg, *reg;
	jnlpool_addrs_ptr_t	save_jnlpool, local_jnlpool;
	int			tprestart_rc, len;
	gv_namehead		*gvt;
	enum cdb_sc		status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	tprestart_rc = 0;
	/* Some callers of "tp_restart" want this function to return with an error code instead of issue an rts_error
	 * if there is an error inside tp_restart. The SIGNAL macro is supposed to reflect the error code in that
	 * case and the caller handles this error code accordingly after tp_restart returns.  For those callers,
	 * establish the "tp_restart_ch" condition handler to catch all errors. For the remaining callers, any errors
	 * inside tp_restart will invoke whatever parent condition handler is active at that point.
	 *
	 * The reason why a few callers prefer this inside-tprestart error handling is because they are already a
	 * condition/error handler (e.g. mdb_condition_handler) when they invoke tp_restart and do not want another
	 * rts_error to happen inside tp_restart and trigger any other condition/error handlers that can alter the
	 * flow of control elsewhere until this condition handler returns.
	 */
	if (handle_errors_internally)
	{	/* Currently, the only callers of tp_restart with handle_errors_internally set to TRUE are
		 * "mdb_condition_handler", "updproc.c" and "mupip_recover.c". All of those have SIGNAL set
		 * to ERR_TPRETRY so assert that. This is one way of protecting against a new caller of tp_restart
		 * inadvertently using a TRUE value for "handle_errors_internally". One reason for being paranoid
		 * about the TRUE usage is for example in gv_trigger.c, if tp_restart is incorrectly invoked with
		 * TRUE as the second paramter, it will result in indefinite number of cores on a broken database.
		 */
		assert(ERR_TPRETRY == SIGNAL);
		ESTABLISH_RET(tp_restart_ch, tprestart_rc);
	}
	assert(1 == newlevel);
	if (!dollar_tlevel)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TLVLZERO);
		return 0; /* for the compiler only -- never executed */
	}
#	ifdef GTM_TRIGGER
	DBGTRIGR((stderr, "tp_restart: Entry state: %d\n", tprestart_state));
	save_jnlpool = jnlpool;
	if (TPRESTART_STATE_NORMAL == tprestart_state)
	{	/* Only do if a normal invocation - otherwise we've already done this code for this TP restart */
#	endif
		/* Increment restart counts for each region in this transaction */
		for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
		{
			reg = tr->reg;
			if (reg->open)
			{
				csa = &FILE_INFO(reg)->s_addrs;
				switch (dollar_trestart)
				{
					case 0:
						INCR_GVSTATS_COUNTER(csa, csa->nl, n_tp_tot_retries_0, 1);
						break;
					case 1:
						INCR_GVSTATS_COUNTER(csa, csa->nl, n_tp_tot_retries_1, 1);
						break;
					case 2:
						INCR_GVSTATS_COUNTER(csa, csa->nl, n_tp_tot_retries_2, 1);
						break;
					case 3:
						INCR_GVSTATS_COUNTER(csa, csa->nl, n_tp_tot_retries_3, 1);
						break;
					default:
						INCR_GVSTATS_COUNTER(csa, csa->nl, n_tp_tot_retries_4, 1);
						break;
				}
			} else
			{
				assert(cdb_sc_needcrit == t_fail_hist[t_tries]);
				assert(!num_closed);	/* we can have at the most 1 region not opened in the whole tp_reg_list */
				num_closed++;
			}
		}
		status = t_fail_hist[t_tries];
		TREF(prev_t_tries) = t_tries;
		/* Even though rollback and recover operate standalone, there are certain kind of restarts that can still happen due
		 * to white box test cases. Assert accordingly.
		 */
		assert(!mupip_jnl_recover || WB_COMMIT_ERR_ENABLED ||
				(WBTEST_TP_HIST_CDB_SC_BLKMOD == gtm_white_box_test_case_number));
		if (TREF(tprestart_syslog_delta) && (((TREF(tp_restart_count))++ < TREF(tprestart_syslog_first))
			|| (0 == ((TREF(tp_restart_count) - TREF(tprestart_syslog_first)) % TREF(tprestart_syslog_delta)))))
		{
			gvt = TAREF1(tp_fail_hist, t_tries);
			if (NULL == gvt)
			{
				gvname_mstr.addr = (char *)gvname_unknown;
				gvname_mstr.len = gvname_unknown_len;
			} else if ((NULL != gvt->gd_csa) && (gvt->gd_csa->dir_tree == gvt))
			{
				gvname_mstr.addr = (char *)gvname_dirtree;
				gvname_mstr.len = gvname_dirtree_len;
			} else
			{
				if (NULL == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
					end = &buff[MAX_ZWR_KEY_SZ - 1];
				assert(buff[0] == '^');
				gvname_mstr.addr = (char*)(buff + 1);
				gvname_mstr.len = end - buff - 1;
			}
			caller_id_flag = FALSE;		/* don't want caller_id in the operator log */
			assert(0 == cdb_sc_normal);
			if (cdb_sc_normal == status)
				t_fail_hist[t_tries] = '0';	/* temporarily reset just for pretty printing */
			restart_reg = TAREF1(tp_fail_hist_reg, t_tries);
			if (NULL != restart_reg)
			{
				reg_mstr.len = restart_reg->dyn.addr->fname_len;
				reg_mstr.addr = (char *)restart_reg->dyn.addr->fname;
			} else
			{
				reg_mstr.len = 0;
				reg_mstr.addr = NULL;
			}
			if (IS_GTM_IMAGE)
				getzposition(TADR(tp_restart_entryref));
			else
			{
				(TREF(tp_restart_entryref)).mvtype = MV_STR;
				(TREF(tp_restart_entryref)).str.addr = NULL;
				(TREF(tp_restart_entryref)).str.len = 0;
			}
			if (cdb_sc_blkmod != status)
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(18) ERR_TPRESTART, 16, reg_mstr.len, reg_mstr.addr,
					t_tries + 1, t_fail_hist, TAREF1(t_fail_hist_blk, t_tries), gvname_mstr.len,
					gvname_mstr.addr, 0, 0, 0, tp_blkmod_nomod,
					(NULL != sgm_info_ptr) ? sgm_info_ptr->num_of_blks : 0,
					(NULL != sgm_info_ptr) ? sgm_info_ptr->cw_set_depth : 0, &local_tn,
					(TREF(tp_restart_entryref)).str.len, (TREF(tp_restart_entryref)).str.addr);
			} else
			{
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(18) ERR_TPRESTART, 16, reg_mstr.len, reg_mstr.addr,
					t_tries + 1, t_fail_hist, TAREF1(t_fail_hist_blk, t_tries), gvname_mstr.len,
					gvname_mstr.addr, n_pvtmods, n_blkmods, TREF(blkmod_fail_level), TREF(blkmod_fail_type),
					sgm_info_ptr->num_of_blks, sgm_info_ptr->cw_set_depth, &local_tn,
					(TREF(tp_restart_entryref)).str.len, (TREF(tp_restart_entryref)).str.addr);
			}
			TAREF1(tp_fail_hist, t_tries) = NULL;
			TAREF1(tp_fail_hist_reg, t_tries) = NULL;
			if ('0' == t_fail_hist[t_tries])
				t_fail_hist[t_tries] = cdb_sc_normal;	/* get back to where it was */
			caller_id_flag = TRUE;
			n_pvtmods = n_blkmods = 0;
		}
		/* Should never come here with a normal restart code */
		assert(cdb_sc_normal != status);
#		ifdef DEBUG
		TAREF1(tp_restart_failhist_arry, (TREF(tp_restart_failhist_indx))++) = status;
		if (FAIL_HIST_ARRAY_SIZE <= TREF(tp_restart_failhist_indx))
			TREF(tp_restart_failhist_indx) = 0;
		TRACE_TRANS_RESTART(status);
#		endif
		/* the following code is similar, but not identical, to code in t_retry,
		 * which should also be maintained in parallel
		 */
		switch (status)
		{
			case cdb_sc_helpedout:
				assert(IS_FINAL_RETRY_CODE(status));
				csa = sgm_info_ptr->tp_csa;
				if (dba_bg == csa->hdr->acc_meth)
				{
					if (!csa->now_crit)
					{	/* The following grab/rel crit logic is purely to ensure that wcs_recover
						 * gets called if needed. This is because we saw wc_blocked to be TRUE in
						 * tp_tend and decided to restart.
						 */
						assert(!csa->hold_onto_crit);
						grab_crit(sgm_info_ptr->gv_cur_region);
						rel_crit(sgm_info_ptr->gv_cur_region);
					} else
					{	/* Some non-crit holding process set wc_blocked to TRUE causing us to
						 * restart even though we held crit. Most likely phase2 commit or a
						 * process in wcs_wtstart encountered an error. In any case, need to run
						 * cache-recovery to fix the shared memory structures. Since we hold crit,
						 * so no need to grab/rel crit. Call wcs_recover right away.
						 */
						assert(!csa->hold_onto_crit || jgbl.onlnrlbk);
						DEBUG_ONLY(TREF(ok_to_call_wcs_recover) = TRUE);
						wcs_recover(sgm_info_ptr->gv_cur_region);
						DEBUG_ONLY(TREF(ok_to_call_wcs_recover) = FALSE);
					}
				} else
				{
					assert(dba_mm == csa->hdr->acc_meth);
					wcs_recover(sgm_info_ptr->gv_cur_region);
				}
				if (CDB_STAGNATE > t_tries)
				{
					t_tries++;
					break;
				}
#				ifdef DEBUG
				if (0 <= TREF(tp_restart_dont_counts))	/* skip increment on negative from TPNOTACID */
					(TREF(tp_restart_dont_counts))++;
#				endif
				/* WARNING - fallthrough !!! */
			case cdb_sc_needcrit:
				/* Here when a final (4th) attempt has failed with a need for crit in some routine. The
				 * assumption is that the previous attempt failed somewhere before transaction end
				 * therefore tp_reg_list did not have a complete list of regions necessary to complete the
				 * transaction and therefore not all the regions have been locked down. The new region (by
				 * virtue of it having now been referenced) has been added to tp_reg_list so all we need
				 * now is a retry.
				 */
				assert(IS_FINAL_RETRY_CODE(status));
				assert(CDB_STAGNATE == t_tries);
				for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
				{	/* regions might not have been opened if we t_retried in gvcst_init(). dont
					 * rel_crit in that case.
					 */
					reg = tr->reg;
					if (reg->open)
					{
						DEBUG_ONLY(csa = &FILE_INFO(reg)->s_addrs;)
						assert(!csa->hold_onto_crit);
						rel_crit(reg);  /* to ensure deadlock safe order, release all regions
								 * before retry */
					}
				}
#				ifdef DEBUG
					/* The journal pool crit lock is currently obtained only inside commit logic at
					 * which point we will never signal a cdb_sc_needcrit restart code.
					 * So no need to verify if we need to release crit there. Assert this though.
					 */
					if (jnlpool_head)
					{	/* at least one jnlpool setup */
						for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
						{
							reg = tr->reg;
							if (reg->open)
							{
								csa = &FILE_INFO(reg)->s_addrs;
								assert(csa);
								assert(jnlpool);
								if (csa && csa->jnlpool && (csa->jnlpool != jnlpool))
									jnlpool = csa->jnlpool;
								if (jnlpool && jnlpool->jnlpool_dummy_reg
									&& jnlpool->jnlpool_dummy_reg->open)
								{
									jpl_csa = &FILE_INFO(jnlpool->jnlpool_dummy_reg)->s_addrs;
									assert(!jpl_csa->now_crit);
								}
							}
						}
					}
#				endif
				/* If retry due to M-locks, sleep so needed locks have a chance to get released */
				break;
			case cdb_sc_reorg_encrypt:
				assert(IS_FINAL_RETRY_CODE(status));
				/* Even though the failure code is "cdb_sc_reorg_encrypt", it is possible
				 * "process_reorg_encrypt_restart" was already called from "t_qread" which then
				 * returned "cdb_sc_reorg_encrypt" that ended up in tp_restart. "reorg_encrypt_restart_csa" would
				 * have been reset to NULL after opening the new keys in that case so no need to call it again.
				 */
				if (NULL != reorg_encrypt_restart_csa)
				{
#					ifdef DEBUG
					/* Assert "reorg_encrypt_restart_csa" is present in the tp_reg_list AND all regions
					 * are either locked down or not (i.e. reorg_encrypt_restart_csa->now_crit is a good
					 * indicator of whether we need to release crit on all regions or not before doing
					 * the "process_reorg_encrypt_restart" invocation.
					 */
					for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
					{
						reg = tr->reg;
						csa = (sgmnt_addrs *)&FILE_INFO(reg)->s_addrs;
						assert(csa->now_crit == reorg_encrypt_restart_csa->now_crit);
						if (csa == reorg_encrypt_restart_csa)
							break;
					}
					assert(NULL != tr);
#					endif
					if (!reorg_encrypt_restart_csa->now_crit)
					{
						process_reorg_encrypt_restart();
						assert(NULL == reorg_encrypt_restart_csa);
					} else
					{	/* We are holding crit and need to do "process_reorg_encrypt_restart" (possible
						 * if we noticed the "cdb_sc_reorg_encrypt" restart in the 2nd retry in "tp_tend".
						 * In this case, we need to release crit to avoid heavyweight encryption handle
						 * open operations inside crit. And need to reobtain crit afterwards. The
						 * existing function "tp_crit_all_regions" does exactly that (by invoking
						 * "grab_crit_encr_cycle_sync").
						 */
						for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
							rel_crit(tr->reg);
						process_reorg_encrypt_restart();
						assert(NULL == reorg_encrypt_restart_csa);
						tp_tend_status = tp_crit_all_regions();
						assert(FALSE != tp_tend_status);
					}
				}
				/* WARNING - fallthrough !!! */
			/* Journaling might get turned off in the final retry INSIDE crit while trying to flush journal buffer or
			 * during extending the journal file (due to possible disk issues) in which case we will come here with
			 * t_tries = CDB_STAGNATE and failure status set to cdb_sc_jnlclose
			 */
			case cdb_sc_jnlclose:
			/* cdb_sc_jnlstatemod is expected in final retry because csa->jnl_state is noted from csd->jnl_state only
			 * if they are different INSIDE crit. Therefore it is possible that in the final retry one might start with
			 * a stale value of csa->jnl_state which will be noticed only in t_end just before commit as a result of
			 * which we would restart. Such a restart is okay (instead of the checking for jnl state change during the
			 * beginning of final retry) since jnl state changes are considered infrequent that too in the final retry
			 */
			case cdb_sc_jnlstatemod:
			/* cdb_sc_onln_rlbk[1,2] are possible in the final retry. See comment below that explains why. So, decrement
			 * t_tries to account for later increment
			 */
			case cdb_sc_onln_rlbk1:
			case cdb_sc_onln_rlbk2:
			case cdb_sc_instancefreeze:
			case cdb_sc_gvtrootmod2:
			case cdb_sc_gvtrootnonzero:
			case cdb_sc_optrestart:
			/* Note: No additional action needed for "cdb_sc_phase2waitfail" since if we are in the final retry,
			 * we would have set wc_blocked (as part of the SET_WC_BLOCKED_FINAL_RETRY_IF_NEEDED call done
			 * in t_retry/tp_tend/op_tcommit) and the "tp_crit_all_regions" call done below will call
			 * "grab_crit" & "wcs_recover" which will fix the phase2-commit/non-zero-"cr->in_tend" issue.
			 */
			case cdb_sc_phase2waitfail:
			/* cdb_sc_wcs_recover is possible in final retry in TP (see comment in "tp_hist" */
			case cdb_sc_wcs_recover:
				assert(IS_FINAL_RETRY_CODE(status));
				if (CDB_STAGNATE <= t_tries)
				{
					t_tries--;
#					ifdef DEBUG
					if (0 <= TREF(tp_restart_dont_counts))	/* skip increment on negative from TPNOTACID */
						(TREF(tp_restart_dont_counts))++;
#					endif
				}
				if (cdb_sc_instancefreeze == status)
					WAIT_FOR_REPL_INST_UNFREEZE_NOCSA;
				/* fall through */
			default:
				if (CDB_STAGNATE < ++t_tries)
				{
					assert(!IS_FINAL_RETRY_CODE(status));
					hist_index = t_tries;
					t_tries = 0;
					assert(0 != have_crit(CRIT_HAVE_ANY_REG)); /* we should still be holding crit */
					assert(gtm_white_box_test_case_enabled
					    && (WBTEST_TP_HIST_CDB_SC_BLKMOD == gtm_white_box_test_case_number));
					send_msg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_TPFAIL, 2, hist_index, t_fail_hist,
							ERR_GVFAILCORE);
					/* Generate core only if not triggering this codepath using white-box tests */
					DEBUG_ONLY(
						if (!gtm_white_box_test_case_enabled
						    || (WBTEST_TP_HIST_CDB_SC_BLKMOD != gtm_white_box_test_case_number))
					)
							gtm_fork_n_core();
					if (save_jnlpool != jnlpool)
						jnlpool = save_jnlpool;
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TPFAIL, 2, hist_index, t_fail_hist);
					return 0; /* for the compiler only -- never executed */
				} else
				{
					/* Yield the CPU so that the restarting process does not block the crit holder
					 * 	and/or other processes (referencing potentially non-intersecting database
					 * 	regions) in case they are waiting for the CPU. This is done using the
					 * 	rel_quant function.
					 * As of this writing, this operates only between the 2nd and 3rd tries;
					 * The 2nd is fast with the assumption of coincidental conflict in an attempt
					 * 	to take advantage of the buffer state created by the 1st try.
					 * The next to last try is not followed by a rel_quant as it may leave the buffers
					 * 	locked, to reduce live lock and deadlock issues.
					 * With only 4 tries that leaves only the "middle" for rel_quant.
					 * This seems like a legitimate rel_quant
					 */
					if ((CDB_STAGNATE - 1) == t_tries)
						rel_quant();
				}
		}
		assert(NULL == reorg_encrypt_restart_csa);
		if ((CDB_STAGNATE <= t_tries))
		{	/* If there are any regions that haven't yet been opened, open them before attempting for crit on
			 * all. Since we don't hold any crit locks now, we can rest assured this cannot cause a deadlock.
			 * The only exception (to holding crit) currently is mupip journal rollback/recovery if online when
			 * we will be holding crit on all regions but in that case we should NOT have restarted in the first
			 * place as all the concurrent GT.M processes will be hung waiting for crit. The only exception is
			 * if some process set wc_blocked outside crit and we restarted to do cache recovery (which is asserted
			 * above)
			 */
			if (num_closed)
			{
				for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
				{	/* to open region use gv_init_reg() instead of gvcst_init() since that does extra
					 * manipulations with gv_keysize, gv_currkey and gv_altkey.
					 */
					reg = tr->reg;
					if (!reg->open)
					{
						gv_init_reg(reg, NULL);
						assert(reg->open);
					}
				}
				DBG_CHECK_TP_REG_LIST_SORTING(tp_reg_list);
			}
			DEBUG_ONLY(TREF(ok_to_call_wcs_recover) = TRUE);
			if (!jgbl.onlnrlbk)
				tp_tend_status = tp_crit_all_regions();	/* grab crits on all regions */
			/* else online rollback in which case we already hold crit on all regions */
			/* It is possible we came into tp_restart to handle a different restart code but as part of the final
			 * retry detected an Online Rollback. If so, we shouldn't go into the final retry with this out-of-sync
			 * state as tp_tend will detect this out-of-sync during validation causing restart in the final
			 * retry resulting in TPFAIL. So, check for a concurrent Online Rollback and handle it if needed. It is
			 * enough to check any region as Online Rollback will increment the cycle field ON all the regions
			 * unconditionally
			 */
			if (cs_addrs_list && MISMATCH_ONLN_RLBK_CYCLES(cs_addrs_list, cs_addrs_list->nl))
			{	/* We came in to handle a different restart code in the penultimate retry and grab_crit before going
				 * to final retry. As part of grabbing crit, we detected an online rollback. Although we could treat
				 * this as just an online rollback restart and handle it by syncing cycles, but by doing so, we will
				 * lose the information that an online rollback happened when we go back to gvcst_{put,kill}. This
				 * is usually fine except when we are in implicit TP (due to triggers). In case of implicit TP,
				 * gvcst_{put,kill} has specific code to handle online rollback differently than other restart codes
				 * Because of this reason, we don't want to sync cycles but instead continue with the final retry.
				 * t_end/tp_tend/tp_hist will notice the cycle mismatch and will restart (once more) in final retry
				 * with the appropriate cdb_sc code which gvcst_put/gvcst_kill will intercept and act accordingly.
				 * Even if we are not syncing cycles, we need to reset the clues and root block numbers so that we
				 * proceeds smoothly in the final retry.
				 */
				RESET_ALL_GVT_CLUES;
				reset_clues_done = TRUE;
			}
			DEBUG_ONLY(TREF(ok_to_call_wcs_recover) = FALSE);
			assert(FALSE != tp_tend_status);
			/* pick up all MM extension information */
			for (tr = tp_reg_list; NULL != tr; tr = tr->fPtr)
			{
				reg = tr->reg;
				if (dba_mm == REG_ACC_METH(reg))
				{
					TP_CHANGE_REG_IF_NEEDED(reg);
					MM_DBFILEXT_REMAP_IF_NEEDED(cs_addrs, gv_cur_region);
				}
				csa = (sgmnt_addrs *)&FILE_INFO(reg)->s_addrs;
				if (MISMATCH_ROOT_CYCLES(csa, csa->nl) && !reset_clues_done)
				{
					RESET_ALL_GVT_CLUES;
					reset_clues_done = TRUE;
				}
			}
		}
#	ifdef GTM_TRIGGER
	} else
		status = LAST_RESTART_CODE;
	DBGTRIGR((stderr, "tp_restart: past initial normal state processing\n"));
#	endif
	/* The below code to determine the rollback point depends on tp_frame sized blocks being pushed on the TP
	 * stack. If ever other sized blocks are pushed on, a different method will need to be found.
	 */
	assert(0 == ((tpstackbase - (unsigned char *)tp_pointer) % SIZEOF(tp_frame))); /* Simple check for above condition */
	tf = (tp_frame *)(tpstackbase - SIZEOF(tp_frame));
	assert(NULL != tf);
	assert(tpstacktop < (unsigned char *)tf);
#	ifdef GTM_TRIGGER
	if (TPRESTART_STATE_NORMAL == tprestart_state)
	{	/* Only if normal tp_restart call - else we've already done this for this tp_restart */
#	endif
		/* Before we get too far unwound here, if this is a nonrestartable transaction,
		 * let's record where we are for the message later on.
		 */
		if (FALSE == tf->restartable && IS_MCODE_RUNNING)
			getzposition(TADR(tp_restart_entryref));
		/* Do a rollback type cleanup (invalidate gv_target clues of read as well as updated blocks) */
		tp_clean_up(TP_RESTART);
		/* Note: At this point, we are ready to begin the next retry. While we can sync the trigger cycles now to avoid
		 * further restarts, we don't need to because tp_set_sgm (done for each region that is updated in a TP transaction)
		 * does the syncing anyways.
		 */
		assert(cdb_sc_normal != status);
		switch (status)
		{
			case cdb_sc_gvtrootmod2:	/* restarted due to MUPIP REORG moving root blocks */
				RESET_ALL_GVT_CLUES;
				break;
			case cdb_sc_onln_rlbk1:		/* restarted due to online rollback */
			case cdb_sc_onln_rlbk2:
				RESET_ALL_GVT_CLUES;
				if (IS_MCODE_RUNNING && (cdb_sc_onln_rlbk2 == status))
					(TREF(dollar_zonlnrlbk))++;
				break;
			default:
				break;
		}
#	ifdef GTM_TRIGGER
	}
	if (TPRESTART_STATE_TPUNW >= tprestart_state)
	{	/* Either this is a normal tp_restart call or we ran into a trigger base frame while "tp_unwind"
		 * was running which required M and C stack unwinding before we could proceed so this call is
		 * being restarted.
		 */
#	endif
		/* Note that this form of "tp_unwind" will not only unwind the TP stack but also most if not all of
		 * the M stackframe and mv_stent chain as well.
		 */
		GTMTRIG_ONLY(DBGTRIGR((stderr, "tp_restart: Beginning state 0/1 processing (state %d)\n", tprestart_state)));
		tp_unwind(newlevel, RESTART_INVOCATION, &tprestart_rc);
		assert(dollar_tlevel == newlevel);	/* tp_unwind would have set this */
		assert(tf == tp_pointer);	/* Needs to be true for now. Revisit when can restart to other than newlevel == 1 */
		assert(NULL == tf->old_tp_frame);	/* this is indeed the outermost TSTART */
		gd_header = tf->gd_header;
		gv_target = tf->orig_gv_target;
		gv_cur_region = tf->gd_reg;
		TP_CHANGE_REG(gv_cur_region);
		assert(NULL != tf->orig_key);
		COPY_KEY(gv_currkey, tf->orig_key);
		DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
		assert(-1 != tf->extnam_str.len);
		len = tf->extnam_str.len;
		if (len)
		{
			assert(TREF(gv_extname_size) >= len);
			memcpy(extnam_str.addr, (TREF(gv_tporig_extnam_str)).addr, len);
		}
		extnam_str.len = len;
#		ifdef GTM_TRIGGER
		/* Maintenance of SSF_NORET_VIA_MUMTSTART stack frame flag:
		 * - Set by gtm_trigger when trigger base frame is created. Purpose to prevent MUM_TSTART from restarting
		 *   a frame making a call-in to a trigger (flag is checked in MUM_TSTART macro) because the mpc in the
		 *   stack frame is not the return point to the frame, which is only available in the C stack.
		 * - Both TP restart and error handling unwinds can use MUM_TSTART to restart frame.
		 * - TP restart changes the mpc to the proper address (where TSTART was done) before invoking MUM_TSTART. We allow
		 *   this by shutting the SSF_NORET_VIA_MUMTSTART flag off when mpc is changed.
		 * - For TSTARTs done implcitly by triggers, MUM_TSTART would break things so we do not turn off the flag
		 *   for that type.
		 */
		if (!tf->implicit_tstart)
		{	/* SSF_NORET_VIA_MUMTSTART validation:
			 * - This is not a trigger-initiated implicit TSTART.
			 * - If the flag is is not on, no further checks. Turning off flag is unconditional for best performance.
			 * - If flag is on, verify the address in the stack frame is in fact being modified so it points to
			 *   a TSTART instead of the (currently) trigger call point.
			 */
			assert(!(tf->fp->flags & SSF_NORET_VIA_MUMTSTART) || (tf->fp->mpc != tf->restart_pc));
			tf->fp->flags &= SSF_NORET_VIA_MUMTSTART_OFF;
			DBGTRIGR((stderr, "tp_restart: Removing SSF_NORET_VIA_MUMTSTART in frame 0x"lvaddr"\n", tf->fp));
		}
#		endif
		tf->fp->mpc = tf->restart_pc;
		tf->fp->ctxt = tf->restart_ctxt;
#		ifdef GTM_TRIGGER
	} else
		assert(TPRESTART_STATE_MSTKUNW == tprestart_state);
	/* From here on, all states run */
#	endif
	/* Make sure everything else added to the stack since the transaction started is unwound. Note this loop only
	 * has work to do if there were NO local vars to restore. Otherwise tp_unwind would have already unwound the
	 * stack for us.
	 */
	GTMTRIG_ONLY(DBGTRIGR((stderr, "tp_restart: beginning frame unwinds (state %d)\n", tprestart_state)));
	while (frame_pointer < tf->fp)
	{
#		ifdef GTM_TRIGGER
		if (SFT_TRIGR & frame_pointer->type)
		{	/* We have encountered a trigger base frame. We cannot unroll it because there are C frames
			 * associated with it so we must interrupt this tp_restart and return to gtm_trigger() so
			 * it can unroll the base frame and rethrow the error to properly unroll the C stack.
			 */
			tprestart_rc = ERR_TPRETRY;
			tprestart_state = TPRESTART_STATE_MSTKUNW;
			DBGTRIGR((stderr, "tp_restart: Encountered trigger base frame in M-stack unwind - rethrowing\n"));
			INVOKE_RESTART;
		}
#		endif
		op_unwind();
	}
	/* From here on, no further rethrows of tp_restart() - the final finishing touches */
	assert((msp <= stackbase) && (msp > stacktop));
	assert((mv_chain <= (mv_stent *)stackbase) && (mv_chain > (mv_stent *)stacktop));
	assert(MVST_TPHOLD == tf->mvc->mv_st_type);
	/* Our stack frames are unwound to the correct frame but there could be mv_stents pushed on after we last (re)started
	 * this transaction. We need to get rid of them to get back to the correct restart state.
	 */
	for (mvc = mv_chain; mvc < tf->mvc;)
	{	/* Make sure we don't unwind the MVST_TPHOLD for our target level */
		assert((MVST_TPHOLD != mvc->mv_st_type) || ((newlevel - 1) != mvc->mv_st_cont.mvs_tp_holder.tphold_tlevel));
		DBGEHND((stderr, "tp_restart: unwinding mv_stent addr 0x"lvaddr" type %d\n", mvc, mvc->mv_st_type));
		unw_mv_ent(mvc);
		mvc = (mv_stent *)(mvc->mv_st_next + (char *)mvc);
	}
	assert((void *)mvc < (void *)frame_pointer);
	assert(mvc == tf->mvc);
	assert(mvc->mv_st_cont.mvs_tp_holder.tphold_tlevel == (dollar_tlevel - 1));
	DBGEHND((stderr, "tp_restart: Resetting msp from 0x"lvaddr" to 0x"lvaddr" (diff=%d)\n",
		 msp, mvc, INTCAST((unsigned char *)mvc - msp)));
	mv_chain = mvc;
	msp = (unsigned char *)mvc;
#	ifdef GTM_TRIGGER
	/* Revert $ZTWormhole to its previous value */
	DBGTRIGR((stderr, "tp_restart: Restoring $ZTWORMHOLE and NULLifying $ZTSLATE (state %d)\n", tprestart_state));
	memcpy(&dollar_ztwormhole, &mvc->mv_st_cont.mvs_tp_holder.ztwormhole_save, SIZEOF(mval));
	if (1 == newlevel)
		memcpy(&dollar_ztslate, &literal_null, SIZEOF(mval));	/* Zap $ZTSLate at (re)start of lvl 1 transaction */
#	endif
	assert(curr_symval == tf->sym);
	if (frame_pointer->flags & SFF_UNW_SYMVAL)
	{	/* A symval was popped in THIS stackframe by one of our last mv_stent unwinds which means
		 * l_symtab is fairly borked.
		 */
		assert(frame_pointer->l_symtab);	/* Would be NULL in replication processor */
		if ((unsigned char *)frame_pointer->l_symtab < msp)
		{	/* This condition is set up when a local routine is called which, since it is using the
			 * same code, uses the same l_symtab as the caller. But when an exclusive new is done in
			 * this frame, op_xnew creates a NEW symtab just for this frame. But when this code
			 * unwound back to the TSTART, we also unwound the l_symtab this frame was using. So here
			 * we verify this frame is a simple call frame from the previous and restore the use of its
			 * l_symtab if so. If not, assertpro. Note the outer SFF_UWN_SYMVAL check keeps  us from
			 * having non-existant l_symtab issues which is possible when we are MUPIP.
			 */
			if ((frame_pointer->rvector == frame_pointer->old_frame_pointer->rvector)
			    && (frame_pointer->vartab_ptr == frame_pointer->old_frame_pointer->vartab_ptr))
			{
				frame_pointer->l_symtab = frame_pointer->old_frame_pointer->l_symtab;
				frame_pointer->flags &= SFF_UNW_SYMVAL_OFF;	/* No need to clear symtab now */
			} else
				assertpro(FALSE);
		} else
		{	/* Otherwise the l_symtab needs to be cleared so its references get re-resolved to *this* symtab */
			memset(frame_pointer->l_symtab, 0, frame_pointer->vartab_len * SIZEOF(ht_ent_mname *));
			frame_pointer->flags &= SFF_UNW_SYMVAL_OFF;
		}
	}
	assert(tf == tp_pointer);
	assert(NULL == tf->old_tp_frame);
	dollar_truth = tf->dlr_t;
	dollar_zgbldir = tf->zgbldir;
	assert(0 != dollar_zgbldir.mvtype);
	GTMTRIG_ONLY(tprestart_state = TPRESTART_STATE_NORMAL);
	if (FALSE == tf->restartable)
	{	/* Transation is not restartable. Be sure to leave things in a state that the transaction could
		 * be continued if an error handler has a mind to do that.
		 */
		GTMTRIG_ONLY(DBGTRIGR((stderr, "tp_restart: Leaving tp_restart via TRESTNOT error - state reset to 0\n")));
		if (save_jnlpool != jnlpool)
			jnlpool = save_jnlpool;
		if (IS_MCODE_RUNNING)
		{
			getzposition(&beganHere);
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TRESTNOT);		/* Separate msgs so we get both */
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_TRESTLOC, 4, beganHere.str.len, beganHere.str.addr,
				(TREF(tp_restart_entryref)).str.len, (TREF(tp_restart_entryref)).str.addr);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_TRESTNOT, 0, ERR_TRESTLOC, 4,
					beganHere.str.len, beganHere.str.addr,
					(TREF(tp_restart_entryref)).str.len, (TREF(tp_restart_entryref)).str.addr);
		} else
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_TRESTNOT);
		return 0; /* for the compiler only -- never executed */
	}
	++dollar_trestart;
#	ifdef DEBUG
	if (0 > TREF(tp_restart_dont_counts))	/* negative is a flag from TPNOTACID - do increment here */
	{
		TREF(tp_restart_dont_counts) = -TREF(tp_restart_dont_counts);
		(TREF(tp_restart_dont_counts))++;
	}
	assert(0 <= TREF(tp_restart_dont_counts));	/* NOTE: should tp_restart_dont_counts wrap, asserts will fail */
	assert(dollar_trestart >= TREF(tp_restart_dont_counts));
	assert(MAX_TRESTARTS > (dollar_trestart - TREF(tp_restart_dont_counts))); /* a magic number limit for restarts */
#	endif
	if (!dollar_trestart)		/* in case of a wrap */
		dollar_trestart--;
	/* Now that we are done with all the cleanup related to this restart, issue rts_error if we are update process.
	 * updproc_ch knows to handle this SIGNAL.
	 */
	assert(cdb_sc_normal != status);
	if (is_updproc && ((cdb_sc_onln_rlbk1 == status) || (cdb_sc_onln_rlbk2 == status)))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_REPLONLNRLBK);
	if (handle_errors_internally)
		REVERT;
	TREF(expand_prev_key) = FALSE;	/* in case we did a "t_retry" in the middle of "gvcst_zprevious2" */
	GTMTRIG_ONLY(DBGTRIGR((stderr, "tp_restart: completed\n")));
	if (save_jnlpool != jnlpool)
		jnlpool = save_jnlpool;
	return 0;
}
