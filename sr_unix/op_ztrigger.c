/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"

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
#  include "gv_trigger.h"
#  include "gtm_trigger.h"
#  include "gv_trigger_protos.h"
#  include "mv_stent.h"
#  include "stringpool.h"
#  include "trigger.h"
#endif
#include "tp_frame.h"
#include "tp_restart.h"
#include "t_end.h"
#include "t_retry.h"
#include "t_begin.h"
#include "rc_cpt_ops.h"
#include "add_inter.h"
#include "sleep_cnt.h"
#include "wcs_sleep.h"
#include "util.h"
#include "op.h"			/* for op_tstart prototype */
#include "format_targ_key.h"	/* for format_targ_key prototype */
#include "tp_set_sgm.h"		/* for tp_set_sgm prototype */
#include "op_tcommit.h"		/* for op_tcommit prototype */
#include "have_crit.h"
#include "gvcst_protos.h"
#include "gtmimagename.h"

LITREF	mval	literal_null;

#ifdef DEBUG
GBLREF char			*update_array, *update_array_ptr;
GBLREF uint4			update_array_size; /* needed for the ENSURE_UPDATE_ARRAY_SPACE macro */
#endif
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey, *gv_altkey;
GBLREF	int4			gv_keysize;
GBLREF	gv_namehead		*gv_target;
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	uint4			dollar_tlevel;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	unsigned char		cw_set_depth;
GBLREF	unsigned int		t_tries;
GBLREF	unsigned char		t_fail_hist[CDB_MAX_TRIES];
GBLREF	boolean_t		need_kip_incr;
GBLREF	uint4			update_trans;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	sgmnt_addrs		*kip_csa;
GBLREF	boolean_t		skip_dbtriggers;	/* see gbldefs.c for description of this global */
GBLREF	int			tprestart_state;
GBLREF	stack_frame		*frame_pointer;
#ifdef GTM_TRIGGER
GBLREF	int4			gtm_trigger_depth;
GBLREF	int4			tstart_trigger_depth;
GBLREF	boolean_t		skip_INVOKE_RESTART;
GBLREF	boolean_t		ztwormhole_used;	/* TRUE if $ztwormhole was used by trigger code */
GBLREF	mval			dollar_ztwormhole;
#endif

error_def(ERR_DBROLLEDBACK);
error_def(ERR_GVZTRIGFAIL);
error_def(ERR_TPRETRY);
error_def(ERR_ZTRIGNOTRW);

#ifndef GTM_TRIGGER
error_def(ERR_UNIMPLOP);

void op_ztrigger(void)
{
	rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_UNIMPLOP);
}
#else
void op_ztrigger(void)
{
	node_local_ptr_t		cnl;
	sgmnt_addrs			*csa;
	sgmnt_data_ptr_t		csd;
	enum cdb_sc			cdb_status;
	jnl_format_buffer		*jfb, *ztworm_jfb;
	uint4				nodeflags;
	boolean_t			write_logical_jnlrecs, jnl_format_done;
	boolean_t			is_tpwrap;
	boolean_t			lcl_implicit_tstart;	/* local copy of the global variable "implicit_tstart" */
	boolean_t			want_root_search = FALSE;
	uint4				lcl_onln_rlbkd_cycle;
	gtm_trigger_parms		trigparms;
	gvt_trigger_t			*gvt_trigger;
	gvtr_invoke_parms_t		gvtr_parms;
	int				gtm_trig_status, rc;
	unsigned int			idx;
	unsigned char			*save_msp;
	mv_stent			*save_mv_chain;
#	ifdef DEBUG
	boolean_t			is_mm;
	GTMTRIG_ONLY(enum cdb_sc	save_cdb_status;)
#	endif
	DCL_THREADGBL_ACCESS;

	if (gv_cur_region->read_only)
		rts_error_csa(CSA_ARG(cs_addrs) VARLSTCNT(4) ERR_ZTRIGNOTRW, 2, REG_LEN_STR(gv_cur_region));
	SETUP_THREADGBL_ACCESS;
	csa = cs_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	save_msp = NULL;
	DEBUG_ONLY(is_mm = (dba_mm == csd->acc_meth));
	TRIG_CHECK_REPLSTATE_MATCHES_EXPLICIT_UPDATE(gv_cur_region, csa);
	if (IS_EXPLICIT_UPDATE)
	{	/* This is an explicit update. Set ztwormhole_used to FALSE. Note that we initialize this only at the
		 * beginning of the transaction and not at the beginning of each try/retry. If the application used
		 * $ztwormhole in any retsarting try of the transaction, we consider it necessary to write the
		 * TZTWORM/UZTWORM record even though it was not used in the succeeding/committing try.
		 */
		ztwormhole_used = FALSE;
	}
	JNLPOOL_INIT_IF_NEEDED(csa, csd, cnl);
	assert(('\0' != gv_currkey->base[0]) && gv_currkey->end);
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	T_BEGIN_SETORKILL_NONTP_OR_TP(ERR_GVZTRIGFAIL);
	lcl_implicit_tstart = FALSE;
	assert(NULL != update_array);
	assert(NULL != update_array_ptr);
	assert(0 != update_array_size);
	assert(update_array + update_array_size >= update_array_ptr);
	for (;;)
	{
		assert(csd == cs_data);	/* assert csd is in sync with cs_data even if there were MM db file extensions */
		assert(csd == csa->hdr);
		jnl_format_done = FALSE;
		write_logical_jnlrecs = JNL_WRITE_LOGICAL_RECS(csa);
		gvtr_parms.num_triggers_invoked = 0;	/* clear any leftover value */
		is_tpwrap = FALSE;
		if (!skip_dbtriggers) /* No trigger init needed if skip_dbtriggers is TRUE (e.g. mupip load etc.) */
		{
			GVTR_INIT_AND_TPWRAP_IF_NEEDED(csa, csd, gv_target, gvt_trigger, lcl_implicit_tstart, is_tpwrap,
						       ERR_GVZTRIGFAIL);
			assert(0 < dollar_tlevel);
			assert(gvt_trigger == gv_target->gvt_trigger);
			sgm_info_ptr->update_trans |= UPDTRNS_ZTRIGGER_MASK;
			if (NULL != gvt_trigger)
			{
				/* In other trigger types, a PUSH_ZTOLDMVAL_ON_M_STACK is used here to save ztoldval but since
				 * this trigger type doesn't have one, pushing that extra mval on the stack makes no sense.
				 * Instead, we just do the basics that the PUSH_ZTOLDMVAL_ON_M_STACK does to save a marker of
				 * msp and mv_chain to easily restore later rather than having to pop the values (more
				 * expensively)
				 */
				save_msp = msp;
				save_mv_chain = mv_chain;
				/* Invoke relevant trigger(s) regardless whether data exists or not (we don't even check) */
				JNL_FORMAT_ZTWORM_IF_NEEDED(csa, write_logical_jnlrecs,
							    JNL_ZTRIG, gv_currkey, NULL, ztworm_jfb, jfb, jnl_format_done);
				/* Initialize trigger parms that dont depend on the context of the matching trigger. All of
				 * these parms are initialized to NULL. This causes op_svget to report them as NULL strings
				 * whichi s all we need for ZTRIGGER. Note $ztvalue is not not updateable for this type
				 * of trigger.
				 */
				trigparms.ztoldval_new = NULL;
				trigparms.ztdata_new = NULL;
				trigparms.ztvalue_new = NULL;
				gvtr_parms.gvtr_cmd = GVTR_CMDTYPE_ZTRIGGER;
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
					 * and then re-do the op_ztrigger logic.
					 */
					assert(lcl_implicit_tstart);
					assert(CDB_STAGNATE >= t_tries);
					cdb_status = cdb_sc_normal;	/* signal "retry:" to avoid t_retry call */
					goto retry;
				}
				REMOVE_ZTWORM_JFB_IF_NEEDED(ztworm_jfb, jfb, sgm_info_ptr);
				/* Instead of POP_MVALS_FROM_M_STACK_IF_NEEDED, do a stripped-down version since we don't
				 * do anything with $ztoldval. This usually pops off 3 or more mvals saved by trigger processing.
				 */
				if (save_msp > msp)
					UNW_MV_STENT_TO(save_msp, save_mv_chain);
			}
		}
		/* finish off any pending root search from previous retry */
		REDO_ROOT_SEARCH_IF_NEEDED(want_root_search, cdb_status);
		if (cdb_sc_normal != cdb_status)
		{	/* gvcst_root_search invoked from REDO_ROOT_SEARCH_IF_NEEDED ended up with a restart situation but did not
			 * actually invoke t_retry. Instead, it returned control back to us asking us to restart.
			 */
			goto retry;
		}
		if (write_logical_jnlrecs)
		{	/* Only write jnl recs if we are in fact journaling..
			 * skip_dbtriggers is set to TRUE for trigger unsupporting platforms. So, nodeflags will be set to skip
			 * triggers on secondary. This ensures that updates happening in primary (trigger unsupporting platform)
			 * is treated in the same order in the secondary (trigger supporting platform) irrespective of whether
			 * the secondary has defined triggers or not for the global that is being updated.
			 */
			assert(dollar_tlevel);
			if (!jnl_format_done)
			{
				nodeflags = 0;
				if (skip_dbtriggers)
					nodeflags |= JS_SKIP_TRIGGERS_MASK;
				/* Do not replicate implicit updates */
				assert(tstart_trigger_depth <= gtm_trigger_depth);
				if (gtm_trigger_depth > tstart_trigger_depth)
				{
					/* Ensure that JS_SKIP_TRIGGERS_MASK and JS_NOT_REPLICATED_MASK are mutually exclusive. */
					assert(!(nodeflags & JS_SKIP_TRIGGERS_MASK));
					nodeflags |= JS_NOT_REPLICATED_MASK;
				}
				/* Write ZTRIGGER journal record */
				jfb = jnl_format(JNL_ZTRIG, gv_currkey, NULL, nodeflags);
				assert(NULL != jfb);
			}
		}
		/* If we started this transaction, finish it now verifying it completed successfully */
		if (lcl_implicit_tstart)
		{
			GVTR_OP_TCOMMIT(cdb_status);
			if (cdb_sc_normal != cdb_status)
				goto retry;
		}
		INCR_GVSTATS_COUNTER(cs_addrs, cs_addrs->nl, n_ztrigger, 1);
		return;
	  retry:
		if (lcl_implicit_tstart)
		{
			assert(!skip_dbtriggers);
			assert(!skip_INVOKE_RESTART);
			assert((cdb_sc_normal != cdb_status) || (ERR_TPRETRY == gtm_trig_status));
			if (cdb_sc_normal != cdb_status)
				skip_INVOKE_RESTART = TRUE; /* causes t_retry to invoke only tp_restart * without any rts_error */
			/* else: t_retry has already been done by gtm_trigger so no need to do it again for this try */
		}
		assert((cdb_sc_normal != cdb_status) || lcl_implicit_tstart);
		if (cdb_sc_normal != cdb_status)
		{	/* See comment above about POP_MVALS_FROM_M_STACK_IF_NEEDED */
			if ((NULL != save_msp) && (save_msp > msp))
				UNW_MV_STENT_TO(save_msp, save_mv_chain);
			t_retry(cdb_status);
			skip_INVOKE_RESTART = FALSE;
		} else
		{	/* else: t_retry has already been done so no need to do that again but need to still invoke tp_restart
			 * to complete pending "tprestart_state" related work.
			 */
			assert(ERR_TPRETRY == gtm_trig_status);
			TRIGGER_BASE_FRAME_UNWIND_IF_NOMANSLAND;
			/* See comment above about POP_MVALS_FROM_M_STACK_IF_NEEDED */
			if ((NULL != save_msp) && (save_msp > msp))
				UNW_MV_STENT_TO(save_msp, save_mv_chain);
			rc = tp_restart(1, !TP_RESTART_HANDLES_ERRORS);
			assert(0 == rc && TPRESTART_STATE_NORMAL == tprestart_state);
		}
		assert(0 < t_tries);
		if (lcl_implicit_tstart)
		{
			SET_WANT_ROOT_SEARCH(cdb_status, want_root_search);
			assert(!skip_INVOKE_RESTART); /* if set to TRUE above, should have been reset by t_retry */
		}
		/* At this point, we can be in TP only if we implicitly did a tstart in op_ztrigger trying to drive a trigger.
		 * Assert that. So reinvoke the T_BEGIN call only in case of TP. For non-TP, update_trans is unaffected by
		 * t_retry.
		 */
		assert(!dollar_tlevel || lcl_implicit_tstart);
		if (dollar_tlevel)
		{	/* gvcst_kill has similar code and should be maintained in parallel */
			tp_set_sgm();	/* set sgm_info_ptr & first_sgm_info for TP start */
			T_BEGIN_SETORKILL_NONTP_OR_TP(ERR_GVZTRIGFAIL);
		}
		/* In case this is MM and t_retry() remapped an extended database, reset csd */
		assert(is_mm || (csd == cs_data));
		csd = cs_data;
	}
}
#endif /* #ifdef GTM_TRIGGER */
