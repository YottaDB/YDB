/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdskill.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
#include "tp_timeout.h"
#include "lv_val.h"		/* needed for tp_unwind.h */
#include "tp_unwind.h"
#include "op.h"
#include "jobinterrupt_process.h"
#include "gvcst_protos.h"
#include "repl_msg.h"			/* for gtmsource.h */
#include "gtmsource.h"			/* for jnlpool_addrs_ptr_t */
#include "have_crit.h"
#include "deferred_events_queue.h"
#include "deferred_events.h"
#include "error_trap.h"
#include "ztimeout_routines.h"
#include "db_snapshot.h"
#include "gvt_inline.h"

GBLREF	uint4			dollar_tlevel;
GBLREF	uint4			dollar_trestart;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	gd_addr			*gd_header;
GBLREF	tp_region		*tp_reg_list;	/* Chained list of regions used in this transaction not cleared on tp_restart */
GBLREF	gd_region		*gv_cur_region;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	void			(*tp_timeout_clear_ptr)(boolean_t toss_queued);
GBLREF	int			process_exiting;
#ifdef GTM_TRIGGER
GBLREF	int4			gtm_trigger_depth;
GBLREF	int4			tstart_trigger_depth;
#endif
#ifdef DEBUG
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
GBLREF	unsigned char		*tpstackbase, *tpstacktop;
#endif
GBLREF	boolean_t		implicit_trollback;
GBLREF	boolean_t		in_timed_tn;
GBLREF	tp_frame		*tp_pointer;
GBLREF	int4			tstart_gtmci_nested_level;

error_def(ERR_TLVLZERO);
error_def(ERR_TROLLBK2DEEP);
error_def(ERR_INVROLLBKLVL);

#define	RESTORE_GV_CUR_REGION						\
{									\
	gv_cur_region = save_cur_region;				\
	TP_CHANGE_REG(gv_cur_region);					\
	jnlpool = save_jnlpool;						\
}

void	op_trollback(int rb_levels)		/* rb_levels -> # of transaction levels by which we need to rollback : BYPASSOK */
{
	boolean_t	lcl_implicit_trollback = FALSE, reg_reset;
	uint4		newlevel;
	gd_region	*save_cur_region;	/* saved copy of gv_cur_region before tp_clean_up/tp_incr_clean_up modifies it */
	jnlpool_addrs_ptr_t	save_jnlpool;
	gd_region	*curreg;
	gv_key		*gv_orig_key_ptr;
	sgmnt_addrs	*csa;
	tp_region	*tr;
	int		tl;
	boolean_t	skipped_CALLINTROLLBACK_error;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (implicit_trollback)
	{
		/* Unlike the call to "op_trollback" from generated code, this invocation of "op_trollback" is from C runtime code.
		 * Set the global variable to FALSE right away to avoid incorrect values from persisting in case of errors
		 * down below. Before the reset of the global variable, copy it into a local variable.
		 */
		lcl_implicit_trollback = implicit_trollback;
		implicit_trollback = FALSE;
	}
	if (!dollar_tlevel)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_TLVLZERO);
	if (0 > rb_levels)
	{
		if (dollar_tlevel < -rb_levels)
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_TROLLBK2DEEP, 2, -rb_levels, dollar_tlevel);
	} else if (dollar_tlevel <= rb_levels)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_INVROLLBKLVL, 2, rb_levels, dollar_tlevel);
	newlevel = (0 > rb_levels) ? dollar_tlevel + rb_levels : rb_levels;
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
	save_cur_region = gv_cur_region;
	save_jnlpool = jnlpool;
	GTMTRIG_ONLY(assert(tstart_trigger_depth <= gtm_trigger_depth);) /* see similar assert in op_tcommit.c for why */
	assert(tstart_gtmci_nested_level <= TREF(gtmci_nested_level));
	if (!newlevel)
	{
		skipped_CALLINTROLLBACK_error = FALSE;
		if (tstart_gtmci_nested_level != TREF(gtmci_nested_level))
		{	/* We are inside a call-in but the outermost TP was started before the call-in.
			 * So to unwind the TP, we need to unwind the C-stack/M-stacks which is not easy
			 * at this point. So issue an error. The only exception is if we are in the process
			 * of exiting. In that case, we can skip the M stack unwind part ("tp_unwind" etc.)
			 * but otherwise clean up gv_target clues etc. ("tp_cleanup"). This way we can let
			 * the exit handler do an "op_trollback" (and terminate the process fine) without
			 * encountering further errors.
			 */
			if (!process_exiting)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CALLINTROLLBACK, 2,
						TREF(gtmci_nested_level), tstart_gtmci_nested_level);
			else
			{
				skipped_CALLINTROLLBACK_error = TRUE;
				/* Note: In this situation where the M-stack is not easily unwound (because of an active call-in),
				 * we need to skip some parts of the following code as it is not safe to do. Those will be
				 * prefixed with an "if (!skipped_CALLINTROLLBACK_error)" check.
				 */
			}
		}
		if (in_timed_tn)
			(*tp_timeout_clear_ptr)(TRUE);	/* Cancel or clear any pending TP timeout */
		/* Do a rollback type cleanup (invalidate gv_target clues of read as well as
		 * updated blocks). This is typically needed for a restart.
		 */
		tp_clean_up(TP_ROLLBACK);
		for (tr = tp_reg_list;  NULL != tr;  tr = tr->fPtr)
		{
			curreg = tr->reg;
			if (!curreg->open)
				continue;
			csa = &FILE_INFO(curreg)->s_addrs;
			if (SNAPSHOTS_IN_PROG(csa))
				SS_RELEASE_IF_NEEDED(csa, (node_local_ptr_t)csa->nl);
			INCR_GVSTATS_COUNTER(csa, csa->nl, n_tp_rolledback, 1);
			if (csa->now_crit && !csa->hold_onto_crit)
				rel_crit(curreg);			/* release any crit regions */
		}
		reg_reset = FALSE;
		if (!skipped_CALLINTROLLBACK_error)
			CALL_ZTIMEOUT_IF_DEFERRED;
		if (!process_exiting && lcl_implicit_trollback && tp_pointer->implicit_tstart && !tp_pointer->ydb_tp_s_tstart)
		{	/* This is an implicit TROLLBACK of an implicit TSTART started for a non-tp explicit update.
			 * gv_currkey needs to be restored to the value it was at the beginning of the implicit TSTART.
			 * This is necessary so as to maintain $reference accurately (to user-visible global name) in case
			 * of an error during the ^#t processing initiated by an explicit non-tp update.
			 * Note that in case the process is already exiting, it is not necessary to do this maintenance.
			 * And since it is safer to minimize processing during exiting, we skip this step.
			 */
			/* Determine tp_pointer corresponding to outermost TSTART first */
			for (tl = dollar_tlevel - 1;  tl > newlevel;  --tl)
				tp_pointer = tp_pointer->old_tp_frame;
			assert(NULL == tp_pointer->old_tp_frame);	/* this is indeed the outermost TSTART */
			assert(tp_pointer->implicit_tstart);	/* assert implicit_tstart is inherited across nested TSTARTs */
			assert(tp_pointer && tp_pointer->orig_key);
			assert(tp_pointer >= (tp_frame *)tpstacktop);
			assert(tp_pointer <= (tp_frame *)tpstackbase);
			assert(NULL != gv_currkey);
			gv_orig_key_ptr = tp_pointer->orig_key;
			assert(NULL != gv_orig_key_ptr);
			if (NULL != gv_currkey)
				COPY_KEY(gv_currkey, gv_orig_key_ptr);
			gv_target = tp_pointer->orig_gv_target;
			gd_header = tp_pointer->gd_header;
			gv_cur_region = tp_pointer->gd_reg;
			TP_CHANGE_REG(gv_cur_region);
			reg_reset = TRUE;	/* so we dont restore gv_cur_region again */
		} else if (NULL != gv_currkey)
		{
			gv_currkey->base[0] = '\0';
			gv_currkey->end = gv_currkey->prev = 0;
		}
		if (NULL != gv_target)
			gv_target->clue.end = 0;
		if (!skipped_CALLINTROLLBACK_error)
			tp_unwind(newlevel, ROLLBACK_INVOCATION, NULL);
		else
			dollar_tlevel = 0;	/* Even though we skipped "tp_unwind", set state of global variable to reflect
						 * that the TP is fully unwound as later exit handling code relies on this
						 * (e.g. assert in "secshr_db_clnup" after an OP_TROLLBACK invocation).
						 */
		/* Now that we are out of TP, reset the debug-only global variable that is relevant only if we are in TP */
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = FALSE;)
		dollar_trestart = 0;
		if (!reg_reset)
			RESTORE_GV_CUR_REGION;
		if (!skipped_CALLINTROLLBACK_error)
		{	/* Transaction is complete as the outer transaction has been rolled back. Check now to see if any statsDB
			 * region initializations were deferred and drive them now if they were.
			 */
			if (NULL != TREF(statsDB_init_defer_anchor))
				gvcst_deferred_init_statsDB();
			JOBINTR_TP_RETHROW; /* rethrow job interrupt($ZINT) if $ZTEXIT, when coerced to boolean, is true */
		}
	} else
	{
		tp_incr_clean_up(newlevel);
		if (gv_currkey != NULL)
		{
			gv_currkey->base[0] = '\0';
			gv_currkey->end = gv_currkey->prev = 0;
		}
		RESTORE_GV_CUR_REGION;
		tp_unwind(newlevel, ROLLBACK_INVOCATION, NULL);
	}
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
}
