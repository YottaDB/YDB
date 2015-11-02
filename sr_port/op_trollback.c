/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
#include "tp_timeout.h"
#include "lv_val.h"		/* needed for tp_unwind.h */
#include "tp_unwind.h"
#include "op.h"
#include "jobinterrupt_process.h"

GBLREF	uint4			dollar_tlevel;
GBLREF	uint4			dollar_trestart;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	tp_region		*tp_reg_list;	/* Chained list of regions used in this transaction not cleared on tp_restart */
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	void			(*tp_timeout_clear_ptr)(void);
GBLREF	int			process_exiting;
#ifdef GTM_TRIGGER
GBLREF	int4			gtm_trigger_depth;
GBLREF	int4			tstart_trigger_depth;
#endif
#ifdef DEBUG
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
#endif
GBLREF	boolean_t		implicit_trollback;
GBLREF	tp_frame		*tp_pointer;

#define	RESTORE_GV_CUR_REGION						\
{									\
	gv_cur_region = save_cur_region;				\
	TP_CHANGE_REG(gv_cur_region);					\
}

void	op_trollback(int rb_levels)		/* rb_levels -> # of transaction levels by which we need to rollback : BYPASSOK */
{
	uint4		newlevel;
	tp_region	*tr;
	gd_region	*save_cur_region;	/* saved copy of gv_cur_region before tp_clean_up/tp_incr_clean_up modifies it */
	gd_region	*curreg;
	sgmnt_addrs	*csa;
	boolean_t	lcl_implicit_trollback = FALSE;
	gv_key		*gv_orig_key_ptr;

	error_def(ERR_TLVLZERO);
	error_def(ERR_TROLLBK2DEEP);
	error_def(ERR_INVROLLBKLVL);

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
		rts_error(VARLSTCNT(1) ERR_TLVLZERO);
	if (0 > rb_levels)
	{
		if (dollar_tlevel < -rb_levels)
			rts_error(VARLSTCNT(4) ERR_TROLLBK2DEEP, 2, -rb_levels, dollar_tlevel);
	} else if (dollar_tlevel <= rb_levels)
		rts_error(VARLSTCNT(4) ERR_INVROLLBKLVL, 2, rb_levels, dollar_tlevel);
	newlevel = (0 > rb_levels) ? dollar_tlevel + rb_levels : rb_levels;
	/* The DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC macro is used at various points in the database code to check that
	 * gv_target and cs_addrs are in sync. This is because op_gvname relies on this in order to avoid a gv_bind_name
	 * function call (if incoming key matches gv_currkey from previous call, it uses gv_target and cs_addrs right
	 * away instead of recomputing them). We want to check that here as well. The only exception is if we were
	 * interrupted in the middle of TP transaction by an external signal which resulted in us terminating right away.
	 * In this case, we are guaranteed not to make a call to op_gvname again (because we are exiting) so it is ok
	 * not to do this check.
	 */
	DEBUG_ONLY(
		if (!process_exiting)
		{
			DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC;
		}
	)
	save_cur_region = gv_cur_region;
	GTMTRIG_ONLY(assert(tstart_trigger_depth <= gtm_trigger_depth);) /* see similar assert in op_tcommit.c for why */
	if (!newlevel)
	{
		(*tp_timeout_clear_ptr)();	/* Cancel or clear any pending TP timeout */
		/* Do a rollback type cleanup (invalidate gv_target clues of read as well as
		 * updated blocks). This is typically needed for a restart.
		 */
		tp_clean_up(TRUE);
		for (tr = tp_reg_list;  NULL != tr;  tr = tr->fPtr)
		{
			curreg = tr->reg;
			if (!curreg->open)
				continue;
			csa = &FILE_INFO(curreg)->s_addrs;
			INCR_GVSTATS_COUNTER(csa, csa->nl, n_tp_rolledback, 1);
			if (csa->now_crit && !csa->hold_onto_crit)
				rel_crit(curreg);			/* release any crit regions */
		}
		if (lcl_implicit_trollback && tp_pointer->implicit_tstart)
		{	/* This is an implicit TROLLBACK of an implicit TSTART started for a non-tp explicit update.
			 * gv_currkey needs to be restored to the value it was at the beginning of the implicit TSTART.
			 * This is necessary so as to maintain $reference accurately in case of an error during the
			 * ^#t processing initiated by an explicit non-tp update.
			 */
			assert(NULL != gv_currkey);
			assert(tp_pointer && tp_pointer->orig_key);
			gv_orig_key_ptr = tp_pointer->orig_key;
			/* At this point we expect tp_pointer->orig_key and gv_currkey to be in sync. However there are two
			 * exceptions to this.
			 * (a) If MUPIP TRIGGER detects that all of the triggers are erroneous and attempts an "op_trollback",
			 *     gv_currkey would be pointing to ^#t. However, tp_pointer->orig_key would remain at "" (initialized
			 *     during op_tstart).
			 * (b) If an M program defines $etrap to do a halt and an explicit Non-TP update causes a trigger to be
			 *     executed that further causes a runtime error which will now invoke the $etrap code. Since the
			 *     $etrap code does a halt, gtm_exit_handler will invoke "op_trollback" (since dollar_tlevel is > 0).
			 *     At this point, tp_pointer->orig_key and gv_currkey need not be in sync.
			 * To maintain $reference accurately, we need to restore gv_currkey from tp_pointer->orig_key.
			 */
			memcpy(gv_currkey->base, gv_orig_key_ptr->base, gv_orig_key_ptr->end);
		} else if (gv_currkey != NULL)
		{
			gv_currkey->base[0] = '\0';
			gv_currkey->end = gv_currkey->prev = 0;
		}
		if (NULL != gv_target)
			gv_target->clue.end = 0;
		tp_unwind(newlevel, ROLLBACK_INVOCATION, NULL);
		/* Now that we are out of TP, reset the debug-only global variable that is relevant only if we are in TP */
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = FALSE;)
		dollar_trestart = 0;
		RESTORE_GV_CUR_REGION;
		JOBINTR_TP_RETHROW; /* rethrow job interrupt($ZINT) if $ZTEXIT, when coerced to boolean, is true */
	} else
	{
		tp_incr_clean_up(newlevel);
		RESTORE_GV_CUR_REGION;
		tp_unwind(newlevel, ROLLBACK_INVOCATION, NULL);
	}
	DEBUG_ONLY(
		if (!process_exiting)
		{
			DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC;
		}
	)
}
