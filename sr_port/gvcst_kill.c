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

#include <stddef.h>
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_inet.h"	/* Required for gtmsource.h */

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "jnl.h"
#include "copy.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "interlock.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#ifdef GTM_TRIGGER
# include "gv_trigger.h"
# include "gtm_trigger.h"
# include "gv_trigger_protos.h"
# include "mv_stent.h"
# include "stringpool.h"
#endif
#include "tp_frame.h"
#include "tp_restart.h"

/* Include prototypes */
#include "gvcst_kill_blk.h"
#include "t_qread.h"
#include "t_end.h"
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_expand_free_subtree.h"
#include "gvcst_protos.h"	/* for gvcst_kill,gvcst_search prototype */
#include "rc_cpt_ops.h"
#include "add_inter.h"
#include "sleep_cnt.h"
#include "wcs_sleep.h"
#include "wbox_test_init.h"
#include "memcoherency.h"
#include "util.h"
#include "op.h"			/* for op_tstart prototype */
#include "format_targ_key.h"	/* for format_targ_key prototype */
#include "tp_set_sgm.h"		/* for tp_set_sgm prototype */
#include "op_tcommit.h"		/* for op_tcommit prototype */
#include "have_crit.h"
#include "error.h"
#include "gtmimagename.h" /* needed for spanning nodes */

GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey, *gv_altkey;
GBLREF	int4			gv_keysize;
GBLREF	gv_namehead		*gv_target;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	kill_set		*kill_set_tail;
GBLREF	uint4			dollar_tlevel;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	unsigned char		cw_set_depth;
GBLREF	unsigned int		t_tries;
GBLREF	boolean_t		need_kip_incr;
GBLREF	uint4			update_trans;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	sgmnt_addrs		*kip_csa;
GBLREF	boolean_t		skip_dbtriggers;	/* see gbldefs.c for description of this global */
GBLREF	stack_frame		*frame_pointer;
GBLREF	boolean_t		gv_play_duplicate_kills;
#ifdef GTM_TRIGGER
GBLREF	int			tprestart_state;
GBLREF	int4			gtm_trigger_depth;
GBLREF	int4			tstart_trigger_depth;
GBLREF	boolean_t		skip_INVOKE_RESTART;
GBLREF	boolean_t		ztwormhole_used;	/* TRUE if $ztwormhole was used by trigger code */
GBLREF	mval			dollar_ztwormhole;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
#endif
#ifdef DEBUG
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	uint4			update_array_size; /* needed for the ENSURE_UPDATE_ARRAY_SPACE macro */
GBLREF	jnl_gbls_t		jgbl;
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
#endif
UNIX_ONLY(GBLREF	boolean_t 		span_nodes_disallowed;)

error_def(ERR_DBROLLEDBACK);
error_def(ERR_TPRETRY);
error_def(ERR_GVKILLFAIL);

#ifdef GTM_TRIGGER
LITREF	mval	literal_null;
LITREF	mval	*fndata_table[2][2];
#endif
LITREF	mval	literal_batch;

#define SKIP_ASSERT_TRUE	TRUE
#define SKIP_ASSERT_FALSE	FALSE

#define	GOTO_RETRY(SKIP_ASSERT)					\
{								\
	assert((CDB_STAGNATE > t_tries) || SKIP_ASSERT);	\
	goto retry;						\
}

DEFINE_NSB_CONDITION_HANDLER(gvcst_kill_ch)

void	gvcst_kill(boolean_t do_subtree)
{
	boolean_t	spanstat;
	boolean_t	sn_tpwrapped;
	boolean_t	est_first_pass;
	int		oldend;
	int		save_dollar_tlevel;

	DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel);
	if (do_subtree)
	{	/* If we're killing the whole subtree, that includes any spanning nodes. No need to do anything special */
		gvcst_kill2(TRUE, NULL, FALSE);
		assert(save_dollar_tlevel == dollar_tlevel);
		return;
	} else
	{	/* Attempt to zkill node, but abort if we might have a spanning node */
		spanstat = 0;
		gvcst_kill2(FALSE, &spanstat, FALSE);
		assert(save_dollar_tlevel == dollar_tlevel);
		if (!spanstat)
			return;
	}
	VMS_ONLY(assert(FALSE));
#	ifdef UNIX
	RTS_ERROR_IF_SN_DISALLOWED;
	oldend = gv_currkey->end;
	/* Almost certainly have a spanning node to zkill. So start a TP transaction to deal with it. */
	if (!dollar_tlevel)
	{
		sn_tpwrapped = TRUE;
		op_tstart((IMPLICIT_TSTART + IMPLICIT_TRIGGER_TSTART), TRUE, &literal_batch, 0);
		assert(!donot_INVOKE_MUMTSTART);
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = TRUE);
		ESTABLISH_NORET(gvcst_kill_ch, est_first_pass);
		GVCST_ROOT_SEARCH_AND_PREP(est_first_pass);
	} else
		sn_tpwrapped = FALSE;
	/* Fire any triggers FIRST, then proceed with the kill. If we started a lcl_implicit transaction in first gvcst_kill
	 * triggers were rolled back. So if span_status indicates TRLBKTRIG, do them again.
	 * Otherwise, skip triggers because they were either kept or didn't happen.
	 * What if new triggers added between above gvcst_kill and now? Should cause a restart because trigger cycle changes?
	 */
	if (sn_tpwrapped)
		gvcst_kill2(FALSE, NULL, FALSE); /* zkill primary dummy node <--- jnling + trigs happen here */
	/* kill any existing hidden subscripts */
	APPEND_HIDDEN_SUB(gv_currkey); /* append "0211" to gv_currkey */
	gvcst_kill2(FALSE, NULL, TRUE);
	RESTORE_CURRKEY(gv_currkey, oldend);
	if (sn_tpwrapped)
	{
		op_tcommit();
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = FALSE);
		REVERT; /* remove our condition handler */
	}
	assert(save_dollar_tlevel == dollar_tlevel);
#	endif
}

void	gvcst_kill2(boolean_t do_subtree, boolean_t *span_status, boolean_t killing_chunks)
{
	block_id		gvt_root;
	boolean_t		clue, flush_cache;
	boolean_t		next_fenced_was_null, write_logical_jnlrecs, jnl_format_done;
	boolean_t		left_extra, right_extra;
	boolean_t		want_root_search = FALSE, is_dummy, succeeded, key_exists;
	rec_hdr_ptr_t		rp;
	uint4			lcl_onln_rlbkd_cycle;
	int			data_len, cur_val_offset;
	unsigned short		rec_size;
	cw_set_element		*tp_cse;
	enum cdb_sc		cdb_status;
	int			lev, end, target_key_size;
	uint4			prev_update_trans, actual_update;
	jnl_format_buffer	*jfb, *ztworm_jfb;
	jnl_action_code		operation;
	kill_set		kill_set_head, *ks, *temp_ks;
	node_local_ptr_t	cnl;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	srch_blk_status		*bh, *left, *right;
	srch_hist		*gvt_hist, *alt_hist, *dir_hist;
	srch_rec_status		*left_rec_stat, local_srch_rec;
	uint4			segment_update_array_size;
	unsigned char		*base;
	int			lcl_dollar_tlevel, rc;
	uint4			nodeflags;
	gv_namehead		*save_targ;
	sgm_info		*si;
#	ifdef GTM_TRIGGER
	mint			dlr_data;
	boolean_t		is_tpwrap;
	boolean_t		lcl_implicit_tstart;	/* local copy of the global variable "implicit_tstart" */
	gtm_trigger_parms	trigparms;
	gvt_trigger_t		*gvt_trigger;
	gvtr_invoke_parms_t	gvtr_parms;
	int			gtm_trig_status, idx;
	unsigned char		*save_msp;
	mv_stent		*save_mv_chain;
	mval			*ztold_mval = NULL, ztvalue_new, ztworm_val;
#	endif
#	ifdef DEBUG
	boolean_t		is_mm, root_search_done = FALSE;
	uint4			dbg_research_cnt;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	csa = cs_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	DEBUG_ONLY(is_mm = (dba_mm == csd->acc_meth));
	GTMTRIG_ONLY(
		TRIG_CHECK_REPLSTATE_MATCHES_EXPLICIT_UPDATE(gv_cur_region, csa);
		if (IS_EXPLICIT_UPDATE)
		{	/* This is an explicit update. Set ztwormhole_used to FALSE. Note that we initialize this only at the
			 * beginning of the transaction and not at the beginning of each try/retry. If the application used
			 * $ztwormhole in any retsarting try of the transaction, we consider it necessary to write the
			 * TZTWORM/UZTWORM record even though it was not used in the succeeding/committing try.
			 */
			ztwormhole_used = FALSE;
		}
	)
	JNLPOOL_INIT_IF_NEEDED(csa, csd, cnl);
	if (!dollar_tlevel)
	{
		kill_set_head.next_kill_set = NULL;
		if (jnl_fence_ctl.level)	/* next_fenced_was_null is reliable only if we are in ZTransaction */
			next_fenced_was_null = (NULL == csa->next_fenced) ? TRUE : FALSE;
		/* In case of non-TP explicit updates that invoke triggers the kills happen inside of TP. If those kills
		 * dont cause any actual update, we need prev_update_trans set appropriately so update_trans can be reset.
		 */
		GTMTRIG_ONLY(prev_update_trans = 0);
	} else
		prev_update_trans = sgm_info_ptr->update_trans;
	assert(('\0' != gv_currkey->base[0]) && gv_currkey->end);
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	T_BEGIN_SETORKILL_NONTP_OR_TP(ERR_GVKILLFAIL);
	assert(NULL != update_array);
	assert(NULL != update_array_ptr);
	assert(0 != update_array_size);
	assert(update_array + update_array_size >= update_array_ptr);
	GTMTRIG_ONLY(
		lcl_implicit_tstart = FALSE;
		trigparms.ztvalue_new = NULL;
	)
	operation = (do_subtree ? JNL_KILL : JNL_ZKILL);
	for (;;)
	{
		actual_update = 0;
#		ifdef GTM_TRIGGER
		gvtr_parms.num_triggers_invoked = 0;	/* clear any leftover value */
		is_tpwrap = FALSE;
		/* No trigger ^#t reads needed if skip_dbtriggers is TRUE (e.g. mupip load etc.) */
		if (!skip_dbtriggers)
		{
			GVTR_INIT_AND_TPWRAP_IF_NEEDED(csa, csd, gv_target, gvt_trigger, lcl_implicit_tstart, is_tpwrap,
							ERR_GVKILLFAIL);
			assert(gvt_trigger == gv_target->gvt_trigger);
		}
		/* finish off any pending root search from previous retry */
		REDO_ROOT_SEARCH_IF_NEEDED(want_root_search, cdb_status);
		if (cdb_sc_normal != cdb_status)
		{	/* gvcst_root_search invoked from REDO_ROOT_SEARCH_IF_NEEDED ended up with a restart situation but did not
			 * actually invoke t_retry. Instead, it returned control back to us asking us to restart.
			 */
			GOTO_RETRY(SKIP_ASSERT_TRUE); /* cannot enable assert (which has an assert about t_tries < CDB_STAGNATE)
						       * because it is possible for us to get cdb_sc_gvtrootmod2 restart when
						       * t_tries == CDB_STAGNATE.
						       */
		}
#		endif
		/* Need to reinitialize gvt_hist & alt_hist for each try as it might have got set to a value in the previous
		 * retry that is inappropriate for this try (e.g. gvt_root value changed between tries).
		 */
		gvt_hist = &gv_target->hist;
		gvt_root = gv_target->root;	/* refetch root in case it changed due to retries in this for loop */
		if (!gvt_root)
		{
			assert(gv_play_duplicate_kills);
			/* No GVT for this global. So nothing to kill. But since we have to play the duplicate kill
			 * (asserted above) we cannot return at this point. There are two cases.
			 * (1) If we hold crit on the region at this point, then there is no way a concurrent update
			 *	could create a GVT for this global. So no ned of any extra history recordkeeping.
			 * (2) If we dont hold crit though, this scenario is possible. Handle this by searching for
			 *	the global name in the directory tree and verifying that it is still missing.
			 *	a) If YES, then pass this history on to t_end/tp_hist as part of the duplicate kill play action.
			 *	   This way we make sure no other process concurrently creates the GVT while we process this
			 *	   duplicate kill transaction.
			 *	b) If NO, then some other process has created a GVT for this global after we started this
			 *	   transaction. In this case we will have to first get the non-zero root block number of this
			 *	   GVT and then use it in the rest of this function. Use the function "gvcst_redo_root_search"
			 *	   for this purpose. It is possible that the root block is set to 0 even after the call to the
			 *	   above function (because of a concurrent KILL of this GVT after we saw a non-zero value but
			 *	   before the "gvcst_redo_root_search" invocation). In this case, we are back to square one.
			 *	   So we redo the logic in this entire comment again each time going through t_retry.
			 *	   In the 3rd retry, we will get crit and that time we will break out of this block of code
			 *	   and move on to the real kill.
			 */
			alt_hist = NULL;
			if (csa->now_crit)
			{	/* Case (1) : Clear history in case this gets passed to t_end/tp_hist later below */
				gvt_hist->h[0].blk_num = 0;
			} else
			{	/* Case (2) : Search for global name in directory tree */
				save_targ = gv_target;
				SET_GV_ALTKEY_TO_GBLNAME_FROM_GV_CURRKEY;	/* set up gv_altkey to be just the gblname */
				gv_target = csa->dir_tree;
				dir_hist = &gv_target->hist;
				cdb_status = gvcst_search(gv_altkey, NULL);
				RESET_GV_TARGET_LCL(save_targ);
				if (cdb_sc_normal != cdb_status)
				{	/* Retry the transaction. But reset directory tree clue as it is suspect at this point.
					 * Needed as t_retry only resets clue of gv_target which is not the directory tree anymore.
					 */
					csa->dir_tree->clue.end = 0;
					GOTO_RETRY(SKIP_ASSERT_FALSE);
				}
				if ((gv_altkey->end + 1) == dir_hist->h[0].curr_rec.match)
				{	/* Case (2b) : GVT now exists for this global */
					cdb_status = cdb_sc_gvtrootmod;
					GOTO_RETRY(SKIP_ASSERT_FALSE);
				} else
				{	/* Case (2a) : GVT does not exist for this global */
					gvt_hist = dir_hist;	/* validate directory tree history in t_end/tp_hist */
				}
			}
		} else
			alt_hist = gv_target->alt_hist;
		DEBUG_ONLY(dbg_research_cnt = 0;)
		jnl_format_done = FALSE;
		write_logical_jnlrecs = JNL_WRITE_LOGICAL_RECS(csa);
#		ifdef GTM_TRIGGER
		dlr_data = 0;
		/* No trigger invocation needed if skip_dbtriggers is TRUE (e.g. mupip load etc.).
		 * If gvt_root is 0 (possible only if gv_play_duplicate_kills is TRUE) we want to only journal the
		 * kill but not touch the database or invoke triggers. So skip triggers in that case too.
		 */
		if (!skip_dbtriggers && (NULL != gvt_trigger) && gvt_root && !killing_chunks)
		{
			PUSH_ZTOLDMVAL_ON_M_STACK(ztold_mval, save_msp, save_mv_chain);
			/* Determine $ZTOLDVAL & $ZTDATA to fill in trigparms */
			dlr_data = DG_DATAGET; /* tell dataget we want full info regarding descendants */
			cdb_status = gvcst_dataget(&dlr_data, ztold_mval);
			if (cdb_sc_normal != cdb_status)
				GOTO_RETRY(SKIP_ASSERT_FALSE);
			assert((11 >= dlr_data) && (1 >= (dlr_data % 10)));
			/* Invoke triggers for KILL as long as $data is nonzero (1 or 10 or 11).
			 * Invoke triggers for ZKILL only if $data is 1 or 11 (for 10 case, ZKILL is a no-op).
			 */
			if (do_subtree ? dlr_data : (dlr_data & 1))
			{	/* Either node or its descendants exists. Invoke KILL triggers for this node.
				 * But first write journal records (ZTWORM and/or KILL) for the triggering nupdate.
				 * "ztworm_jfb", "jfb" and "jnl_format_done" are set by the below macro.
				 */
				JNL_FORMAT_ZTWORM_IF_NEEDED(csa, write_logical_jnlrecs,
						operation, gv_currkey, NULL, ztworm_jfb, jfb, jnl_format_done);
				/* Initialize trigger parms that dont depend on the context of the matching trigger */
				trigparms.ztoldval_new = ztold_mval;
				trigparms.ztdata_new = fndata_table[dlr_data / 10][dlr_data & 1];
				if (NULL == trigparms.ztvalue_new)
				{	/* Do not pass literal_null directly since $ztval can be modified inside trigger
					 * code and literal_null is in read-only segment so will not be modifiable.
					 * Hence the need for a dummy local variable mval "ztvalue_new" in the C stack.
					 */
					ztvalue_new = literal_null;
					trigparms.ztvalue_new = &ztvalue_new;
				}
				gvtr_parms.gvtr_cmd = do_subtree ? GVTR_CMDTYPE_KILL : GVTR_CMDTYPE_ZKILL;
				gvtr_parms.gvt_trigger = gvt_trigger;
				/* Now that we have filled in minimal information, let "gvtr_match_n_invoke" do the rest */
				gtm_trig_status = gvtr_match_n_invoke(&trigparms, &gvtr_parms);
				assert((0 == gtm_trig_status) || (ERR_TPRETRY == gtm_trig_status));
				if (ERR_TPRETRY == gtm_trig_status)
				{	/* A restart has been signaled that we need to handle or complete the handling of.
					 * This restart could have occurred reading the trigger in which case no
					 * tp_restart() has yet been done or it could have occurred in trigger code in
					 * which case we need to finish the incomplete tp_restart. In both cases this
					 * must be an implicitly TP wrapped transaction. Our action is to complete the
					 * necessary tp_restart() logic (t_retry is already completed so should be skipped)
					 * and then re-do the gvcst_kill logic.
					 */
					assert(lcl_implicit_tstart || *span_status);
					cdb_status = cdb_sc_normal;	/* signal "retry:" to avoid t_retry call */
					assert(CDB_STAGNATE >= t_tries);
					GOTO_RETRY(SKIP_ASSERT_TRUE);;	/* Cannot check assert because above assert is >= t_tries */
				}
				REMOVE_ZTWORM_JFB_IF_NEEDED(ztworm_jfb, jfb, sgm_info_ptr);
			}
			/* else : we dont invoke any KILL/ZTKILL type triggers for a node whose $data is 0 */
			POP_MVALS_FROM_M_STACK_IF_NEEDED(ztold_mval, save_msp, save_mv_chain);
		}
#		endif
		assert(csd == cs_data);	/* assert csd is in sync with cs_data even if there were MM db file extensions */
		assert(csd == csa->hdr);
		si = sgm_info_ptr;	/* Has to be AFTER the GVTR_INIT_AND_TPWRAP_IF_NEEDED macro in case that sets
					 * sgm_info_ptr to a non-NULL value (if a non-TP transaction is tp wrapped for triggers).
					 */
		assert(t_tries < CDB_STAGNATE || csa->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
		if (!dollar_tlevel)
		{
			CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
			kill_set_tail = &kill_set_head;
			for (ks = &kill_set_head;  NULL != ks;  ks = ks->next_kill_set)
				ks->used = 0;
		} else
		{
			segment_update_array_size = UA_NON_BM_SIZE(csd);
			ENSURE_UPDATE_ARRAY_SPACE(segment_update_array_size);
		}
		clue = (0 != gv_target->clue.end);
research:
		if (gvt_root)
		{
#if defined(DEBUG) && defined(UNIX)
			if (gtm_white_box_test_case_enabled && (WBTEST_ANTIFREEZE_GVKILLFAIL == gtm_white_box_test_case_number))
			{
				cdb_status = cdb_sc_blknumerr;
				/* Skip assert inside GOTO_RETRY macro as the WBTEST_ANTIFREEZE_GVKILLFAIL white-box testcase
				 * intentionally triggers a GVKILLFAIL error.
				 */
				GOTO_RETRY(SKIP_ASSERT_TRUE);
			}
#endif
			if (cdb_sc_normal != (cdb_status = gvcst_search(gv_currkey, NULL)))
				GOTO_RETRY(SKIP_ASSERT_FALSE);
			assert(gv_altkey->top == gv_currkey->top);
			assert(gv_altkey->top == gv_keysize);
			end = gv_currkey->end;
			assert(end < gv_currkey->top);
			memcpy(gv_altkey, gv_currkey, SIZEOF(gv_key) + end);
			base = &gv_altkey->base[0];
			if (do_subtree)
			{
				base[end - 1] = 1;
				assert(KEY_DELIMITER == base[end]);
				base[++end] = KEY_DELIMITER;
			} else
			{
#				ifdef UNIX
				target_key_size = gv_currkey->end + 1;
				bh = &gv_target->hist.h[0];
				key_exists = (target_key_size == bh->curr_rec.match);
				if (key_exists)
				{	/* check for spanning node dummy value: a single zero byte */
					rp = (rec_hdr_ptr_t)((sm_uc_ptr_t)bh->buffaddr + bh->curr_rec.offset);
					GET_USHORT(rec_size, &rp->rsiz);
					cur_val_offset = SIZEOF(rec_hdr) + target_key_size - EVAL_CMPC((rec_hdr_ptr_t)rp);
					data_len = rec_size - cur_val_offset;
					is_dummy = IS_SN_DUMMY(data_len, (sm_uc_ptr_t)rp + cur_val_offset);
					if (is_dummy && (NULL != span_status) && !(span_nodes_disallowed && csd->span_node_absent))
					{
						need_kip_incr = FALSE;
						if (!dollar_tlevel)
						{
							update_trans = 0;
							succeeded = ((trans_num)0 != t_end(gvt_hist, NULL, TN_NOT_SPECIFIED));
							if (!succeeded)
							{	/* see other t_end */
								assert((NULL == kip_csa) && (csd == cs_data));
								update_trans = UPDTRNS_DB_UPDATED_MASK;
								continue;
							}
							*span_status = TRUE;
							return;
						} else
						{
							cdb_status = tp_hist(NULL);
							if (cdb_sc_normal != cdb_status)
								GOTO_RETRY(SKIP_ASSERT_FALSE);
							*span_status = TRUE;
#							ifdef GTM_TRIGGER
							if (lcl_implicit_tstart)
							{	/* Rollback triggers */
								OP_TROLLBACK(-1);
								return;
							}
#							endif
							/* do not return in case of entering with TP, still set span_status */
						}
					}
				}
#				endif
				if (killing_chunks)
				{	/* Second call of gvcst_kill2 within TP transaction in gvcst_kill
					 * Kill all hidden subscripts...
					 */
					base[end - 3] = STR_SUB_MAXVAL;
					base[end - 2] = STR_SUB_MAXVAL;
					base[end - 1] = KEY_DELIMITER;
					base[end - 0] = KEY_DELIMITER;
				} else
				{
					base[end] = 1;
					base[++end] = KEY_DELIMITER;
					base[++end] = KEY_DELIMITER;
				}
			}
			gv_altkey->end = end;
			if (cdb_sc_normal != (cdb_status = gvcst_search(gv_altkey, alt_hist)))
				GOTO_RETRY(SKIP_ASSERT_FALSE);
			if (alt_hist->depth != gvt_hist->depth)
			{
				cdb_status = cdb_sc_badlvl;
				GOTO_RETRY(SKIP_ASSERT_FALSE);
			}
			right_extra = FALSE;
			left_extra = TRUE;
			for (lev = 0; 0 != gvt_hist->h[lev].blk_num; ++lev)
			{
				left = &gvt_hist->h[lev];
				right = &alt_hist->h[lev];
				assert(0 != right->blk_num);
				left_rec_stat = left_extra ? &left->prev_rec : &left->curr_rec;
				if (left->blk_num == right->blk_num)
				{
					cdb_status = gvcst_kill_blk(left, lev, gv_currkey, *left_rec_stat, right->curr_rec,
									right_extra, &tp_cse);
					assert(!dollar_tlevel || (NULL == tp_cse) || (left->cse == tp_cse));
					assert( dollar_tlevel || (NULL == tp_cse));
					if (tp_cse)
						actual_update = UPDTRNS_DB_UPDATED_MASK;
					if (cdb_sc_normal == cdb_status)
						break;
					gv_target->clue.end = 0; /* If need to go up from leaf (or higher), history wont be valid */
					if (clue)
					{	/* Clue history valid only for data block, need to re-search */
						clue = FALSE;
						DEBUG_ONLY(dbg_research_cnt++;)
						goto research;
					}
					if (cdb_sc_delete_parent != cdb_status)
						GOTO_RETRY(SKIP_ASSERT_FALSE);
					left_extra = right_extra
						   = TRUE;
				} else
				{
					gv_target->clue.end = 0; /* If more than one block involved, history will not be valid */
					if (clue)
					{	/* Clue history valid only for data block, need to re-search */
						clue = FALSE;
						DEBUG_ONLY(dbg_research_cnt++;)
						goto research;
					}
					local_srch_rec.offset = ((blk_hdr_ptr_t)left->buffaddr)->bsiz;
					local_srch_rec.match = 0;
					cdb_status = gvcst_kill_blk(left, lev, gv_currkey, *left_rec_stat,
										local_srch_rec, FALSE, &tp_cse);
					assert(!dollar_tlevel || (NULL == tp_cse) || (left->cse == tp_cse));
					assert( dollar_tlevel || (NULL == tp_cse));
					if (tp_cse)
						actual_update = UPDTRNS_DB_UPDATED_MASK;
					if (cdb_sc_normal == cdb_status)
						left_extra = FALSE;
					else if (cdb_sc_delete_parent == cdb_status)
					{
						left_extra = TRUE;
						cdb_status = cdb_sc_normal;
					} else
						GOTO_RETRY(SKIP_ASSERT_FALSE);
					local_srch_rec.offset = local_srch_rec.match
							      = 0;
					cdb_status = gvcst_kill_blk(right, lev, gv_altkey, local_srch_rec, right->curr_rec,
									right_extra, &tp_cse);
					assert(!dollar_tlevel || (NULL == tp_cse) || (right->cse == tp_cse));
					assert( dollar_tlevel || (NULL == tp_cse));
					if (tp_cse)
						actual_update = UPDTRNS_DB_UPDATED_MASK;
					if (cdb_sc_normal == cdb_status)
						right_extra = FALSE;
					else if (cdb_sc_delete_parent == cdb_status)
					{
						right_extra = TRUE;
						cdb_status = cdb_sc_normal;
					} else
						GOTO_RETRY(SKIP_ASSERT_FALSE);
				}
			}
		}
		if (!gv_play_duplicate_kills)
		{	/* Determine whether the kill is going to update the db or not. If not, skip the kill altogether.
			 * The variable "actual_update" is set accordingly below.
			 */
			if (!dollar_tlevel)
			{
				assert(!jnl_format_done);
				assert(0 == actual_update); /* for non-TP, tp_cse is NULL even if cw_set_depth is non-zero */
				if (0 != cw_set_depth)
					actual_update = UPDTRNS_DB_UPDATED_MASK;
				/* Reset update_trans (to potentially non-zero value) in case it got set to 0 in previous retry. */
				update_trans = actual_update;
			} else
			{
#				ifdef GTM_TRIGGER
				if (!actual_update) /* possible only if the node we are attempting to KILL does not exist now */
				{	/* Note that it is possible that the node existed at the time of the "gvcst_dataget" but
					 * got killed later when we did the gvcst_search (right after the "research:" label). This
					 * is possible if any triggers invoked in between KILLed the node and/or all its
					 * descendants. We still want to consider this case as an actual update to the database
					 * as far as journaling is concerned (this is because we have already formatted the
					 * KILL journal record) so set actual_update to UPDTRNS_DB_UPDATED_MASK in this case.
					 * Note that it is possible that the node does not exist now due to a restartable
					 * situation (instead of due to a KILL inside trigger code). In that case, it is safe to
					 * set actual_update to UPDTRNS_DB_UPDATED_MASK (even though we did not do any update to
					 * the database) since we will be restarting anyways. For ZKILL, check if dlr_data was
					 * 1 or 11 and for KILL, check if it was 1, 10 or 11.
					 */
#						ifdef DEBUG
						if (!gvtr_parms.num_triggers_invoked && (do_subtree ? dlr_data : (dlr_data & 1)))
						{	/* Triggers were not invoked but still the node that existed a few
							 * steps above does not exist now. This is a restartable situation.
							 * Assert that.
							 */
							assert(!skip_dbtriggers);
							TREF(donot_commit) |= DONOTCOMMIT_GVCST_KILL_ZERO_TRIGGERS;
						}
#						endif
					if (do_subtree ? dlr_data : (dlr_data & 1))
						actual_update = UPDTRNS_DB_UPDATED_MASK;
				}
#				endif
				NON_GTMTRIG_ONLY(assert(!jnl_format_done));
				assert(!actual_update || si->cw_set_depth
						GTMTRIG_ONLY(|| gvtr_parms.num_triggers_invoked || TREF(donot_commit)));
				assert(!(prev_update_trans & ~UPDTRNS_VALID_MASK));
				if (!actual_update)
					si->update_trans = prev_update_trans;	/* restore status prior to redundant KILL */
			}
		} else
		{	/* Since "gv_play_duplicate_kills" is set, irrespective of whether the node/subtree to be
			 * killed exists or not in the db, consider this as a db-updating kill.
			 */
			if (!dollar_tlevel)
			{
				assert(!jnl_format_done);
				assert(0 == actual_update); /* for non-TP, tp_cse is NULL even if cw_set_depth is non-zero */
				/* See comment above IS_OK_TO_INVOKE_GVCST_KILL macro for why the below assert is the way it is */
				assert(!jgbl.forw_phase_recovery || cw_set_depth || (JS_IS_DUPLICATE & jgbl.mur_jrec_nodeflags)
						|| jgbl.mur_options_forward);
				if (0 != cw_set_depth)
					actual_update = UPDTRNS_DB_UPDATED_MASK;
				/* Set update_trans to TRUE unconditionally since we want to play duplicate kills */
				update_trans = UPDTRNS_DB_UPDATED_MASK;
			} else
			{
				/* See comment above IS_OK_TO_INVOKE_GVCST_KILL macro for why the below assert is the way it is */
				/*assert(!jgbl.forw_phase_recovery || actual_update || (JS_IS_DUPLICATE & jgbl.mur_jrec_nodeflags)
						|| jgbl.mur_options_forward);*/
				assert(si->update_trans);
			}
		}
		if (write_logical_jnlrecs && (actual_update || gv_play_duplicate_kills) && !killing_chunks)
		{	/* Maintain journal records only if the kill actually resulted in a database update OR if
			 * "gv_play_duplicate_kills" is TRUE. In the latter case, even though no db blocks will be touched,
			 * it will still increment the db curr_tn and write jnl records.
			 *
			 * skip_dbtriggers is set to TRUE for trigger unsupporting platforms. So, nodeflags will be set to skip
			 * triggers on secondary. This ensures that updates happening in primary (trigger unsupporting platform)
			 * is treated in the same order in the secondary (trigger supporting platform) irrespective of whether
			 * the secondary has defined triggers or not for the global that is being updated.
			 */
			if (!dollar_tlevel)
			{
				nodeflags = 0;
				if (skip_dbtriggers)
					nodeflags |= JS_SKIP_TRIGGERS_MASK;
				if (!actual_update)
				{
					assert(gv_play_duplicate_kills);
					nodeflags |= JS_IS_DUPLICATE;
				}
				assert(!jnl_format_done);
				jfb = jnl_format(operation, gv_currkey, NULL, nodeflags);
				assert(NULL != jfb);
			} else if (!jnl_format_done)
			{
				nodeflags = 0;
				if (skip_dbtriggers)
					nodeflags |= JS_SKIP_TRIGGERS_MASK;
				if (!actual_update)
				{
					assert(gv_play_duplicate_kills);
					nodeflags |= JS_IS_DUPLICATE;
				}
#				ifdef GTM_TRIGGER
				/* Do not replicate implicit updates */
				assert(tstart_trigger_depth <= gtm_trigger_depth);
				if (gtm_trigger_depth > tstart_trigger_depth)
				{
					/* Ensure that JS_SKIP_TRIGGERS_MASK and JS_NOT_REPLICATED_MASK are mutually exclusive. */
					assert(!(nodeflags & JS_SKIP_TRIGGERS_MASK));
					nodeflags |= JS_NOT_REPLICATED_MASK;
				}
#				endif
				/* Write KILL journal record */
				jfb = jnl_format(operation, gv_currkey, NULL, nodeflags);
				assert(NULL != jfb);
			}
		}
		flush_cache = FALSE;
		if (!dollar_tlevel)
		{
			if ((0 != csd->dsid) && (0 < kill_set_head.used) && (gvt_hist->h[1].blk_num != alt_hist->h[1].blk_num))
			{	/* multi-level delete */
				rc_cpt_inval();
				flush_cache = TRUE;
			}
			if (0 < kill_set_head.used)		/* increase kill_in_prog */
			{
				need_kip_incr = TRUE;
				if (!csa->now_crit)	/* Do not sleep while holding crit */
					WAIT_ON_INHIBIT_KILLS(cnl, MAXWAIT2KILL);
			}
			if ((trans_num)0 == t_end(gvt_hist, alt_hist, TN_NOT_SPECIFIED))
			{
				assert(csd == cs_data); /* To ensure they are the same even if MM extensions happened in between */
				if (jnl_fence_ctl.level && next_fenced_was_null && actual_update && write_logical_jnlrecs)
				{	/* If ZTransaction and first KILL and the kill resulted in an update
					 * Note that "write_logical_jnlrecs" is used above instead of JNL_WRITE_LOGICAL_RECS(csa)
					 * since the value of the latter macro might have changed inside the call to t_end()
					 * (since jnl state changes could change the JNL_ENABLED check which is part of the macro).
					 */
					assert(NULL != csa->next_fenced);
					assert(jnl_fence_ctl.fence_list == csa);
					jnl_fence_ctl.fence_list = csa->next_fenced;
					csa->next_fenced = NULL;
				}
				need_kip_incr = FALSE;
				assert(NULL == kip_csa);
				/* We could have entered gvcst_kill trying to kill a global that does not exist but later due to a
				 * concurrent set, we are about to retry. In such a case, update_trans could have been set to 0
				 * (from actual_update above). Reset update_trans to non-zero for the next retry as we expect the
				 * kill to happen in this retry.
				 */
				update_trans = UPDTRNS_DB_UPDATED_MASK;
				continue;
			}
			assert(csd == cs_data); /* To ensure they are the same even if MM extensions happened in between */
		} else
                {
                        cdb_status = tp_hist(alt_hist);
                        if (cdb_sc_normal != cdb_status)
				GOTO_RETRY(SKIP_ASSERT_FALSE);
                }
		/* Note down $tlevel (used later) before it is potentially changed by op_tcommit below */
		lcl_dollar_tlevel = dollar_tlevel;
#		ifdef GTM_TRIGGER
		if (lcl_implicit_tstart)
		{
			assert(gvt_root);
			GVTR_OP_TCOMMIT(cdb_status);
			if (cdb_sc_normal != cdb_status)
				GOTO_RETRY(SKIP_ASSERT_TRUE);
		}
#		endif
		if (!killing_chunks)
			INCR_GVSTATS_COUNTER(csa, cnl, n_kill, 1);
		if (gvt_root && (0 != gv_target->clue.end))
		{	/* If clue is still valid, then the deletion was confined to a single block */
			assert(gvt_hist->h[0].blk_num == alt_hist->h[0].blk_num);
			/* In this case, the "right hand" key (which was searched via gv_altkey) was the last search
			 * and should become the clue.  Furthermore, the curr.match from this last search should be
			 * the history's curr.match.  However, this record will have been shuffled to the position of
			 * the "left hand" key, and therefore, the original curr.offset should be left untouched.
			 */
			gvt_hist->h[0].curr_rec.match = alt_hist->h[0].curr_rec.match;
			COPY_CURRKEY_TO_GVTARGET_CLUE(gv_target, gv_altkey);
		}
		NON_GTMTRIG_ONLY(assert(lcl_dollar_tlevel == dollar_tlevel));
		if (!lcl_dollar_tlevel)
		{
			assert(!dollar_tlevel);
			assert(0 < kill_set_head.used || (NULL == kip_csa));
			if (0 < kill_set_head.used)     /* free subtree, decrease kill_in_prog */
			{	/* If csd->dsid is non-zero then some rc code was exercised before the changes
				 * to prevent pre-commit expansion of the kill subtree. Not clear on what to do now.
				 */
				assert(!csd->dsid);
				ENABLE_WBTEST_ABANDONEDKILL;
				gvcst_expand_free_subtree(&kill_set_head);
				assert(csd == cs_data); /* To ensure they are the same even if MM extensions happened in between */
				DECR_KIP(csd, csa, kip_csa);
			}
			assert(0 < kill_set_head.used || (NULL == kip_csa));
			for (ks = kill_set_head.next_kill_set;  NULL != ks;  ks = temp_ks)
			{
				temp_ks = ks->next_kill_set;
				free(ks);
			}
			assert(0 < kill_set_head.used || (NULL == kip_csa));
		}
		GTMTRIG_ONLY(assert(NULL == ztold_mval));
		return;
retry:
#		ifdef GTM_TRIGGER
		if (lcl_implicit_tstart)
		{
			assert(!skip_dbtriggers);
			assert(!skip_INVOKE_RESTART);
			assert((cdb_sc_normal != cdb_status) || (ERR_TPRETRY == gtm_trig_status));
			if (cdb_sc_normal != cdb_status)
				skip_INVOKE_RESTART = TRUE; /* causes t_retry to invoke only tp_restart without any rts_error */
			/* else: t_retry has already been done by gtm_trigger so no need to do it again for this try */
		}
#		endif
		assert((cdb_sc_normal != cdb_status) GTMTRIG_ONLY(|| lcl_implicit_tstart || *span_status));
		if (cdb_sc_normal != cdb_status)
		{
			GTMTRIG_ONLY(POP_MVALS_FROM_M_STACK_IF_NEEDED(ztold_mval, save_msp, save_mv_chain));
			t_retry(cdb_status);
			GTMTRIG_ONLY(skip_INVOKE_RESTART = FALSE);
		} else
		{	/* else: t_retry has already been done so no need to do that again but need to still invoke tp_restart
			 * to complete pending "tprestart_state" related work.
			 */
#			ifdef GTM_TRIGGER
			assert(ERR_TPRETRY == gtm_trig_status);
			TRIGGER_BASE_FRAME_UNWIND_IF_NOMANSLAND;
			POP_MVALS_FROM_M_STACK_IF_NEEDED(ztold_mval, save_msp, save_mv_chain);
			if (!lcl_implicit_tstart)
			{	/* We started an implicit transaction for spanning nodes in gvcst_kill. Invoke restart to return. */
				assert(*span_status && !skip_INVOKE_RESTART && (&gvcst_kill_ch == ctxt->ch));
				INVOKE_RESTART;
			}
#			endif
			rc = tp_restart(1, !TP_RESTART_HANDLES_ERRORS);
			assert(0 == rc GTMTRIG_ONLY(&& TPRESTART_STATE_NORMAL == tprestart_state));
		}
		assert(0 < t_tries);
#		ifdef GTM_TRIGGER
		if (lcl_implicit_tstart)
		{
			SET_WANT_ROOT_SEARCH(cdb_status, want_root_search);
			assert(!skip_INVOKE_RESTART); /* if set to TRUE above, should have been reset by t_retry */
		}
#		endif
		/* At this point, we can be in TP only if we implicitly did a tstart in gvcst_kill (as part of a trigger update).
		 * Assert that. Since the t_retry/tp_restart would have reset si->update_trans, we need to set it again.
		 * So reinvoke the T_BEGIN call only in case of TP. For non-TP, update_trans is unaffected by t_retry.
		 */
		assert(!dollar_tlevel GTMTRIG_ONLY(|| lcl_implicit_tstart));
		if (dollar_tlevel)
		{	/* op_ztrigger has similar code and should be maintained in parallel */
			tp_set_sgm();	/* set sgm_info_ptr & first_sgm_info for TP start */
			T_BEGIN_SETORKILL_NONTP_OR_TP(ERR_GVKILLFAIL);	/* set update_trans and t_err for wrapped TP */
		} else
		{	/* We could have entered gvcst_kill trying to kill a global that does not exist but later due to a
			 * concurrent set, came here for retry. In such a case, update_trans could have been set to 0
			 * (from actual_update). Reset update_trans to non-zero for the next retry as we expect the kill to
			 * happen in this retry.
			 */
			update_trans = UPDTRNS_DB_UPDATED_MASK;
		}
		assert(csd == cs_data); /* To ensure they are the same even if MM extensions happened in between */
	}
}
