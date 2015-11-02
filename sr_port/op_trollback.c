/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "lv_val.h"
#include "tp_unwind.h"
#include "op.h"
#include "jobinterrupt_process.h"

GBLREF	short			dollar_tlevel, dollar_trestart;
GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF	tp_region		*tp_reg_list;	/* Chained list of regions used in this transaction not cleared on tp_restart */
GBLREF  gd_region		*gv_cur_region;
GBLREF  sgmnt_data_ptr_t	cs_data;
GBLREF  sgmnt_addrs		*cs_addrs;
GBLREF	void			(*tp_timeout_clear_ptr)(void);
GBLREF	int			process_exiting;

#define	RESTORE_GV_CUR_REGION						\
{									\
	gv_cur_region = save_cur_region;				\
	TP_CHANGE_REG(gv_cur_region);					\
}

void	op_trollback(int rb_levels)		/* rb_levels -> # of transaction levels by which we need to rollback */
{
	short		newlevel;
	tp_region	*tr;
	gd_region	*save_cur_region;	/* saved copy of gv_cur_region before tp_clean_up/tp_incr_clean_up modifies it */
	gd_region	*curreg;
	sgmnt_addrs	*csa;

	error_def(ERR_TLVLZERO);
	error_def(ERR_TROLLBK2DEEP);
	error_def(ERR_INVROLLBKLVL);

	if (0 == dollar_tlevel)
		rts_error(VARLSTCNT(1) ERR_TLVLZERO);
	if (0 > rb_levels && dollar_tlevel < -rb_levels)
		rts_error(VARLSTCNT(4) ERR_TROLLBK2DEEP, 2, -rb_levels, dollar_tlevel);
	else if (dollar_tlevel <= rb_levels)
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
	if (!newlevel)
	{
		(*tp_timeout_clear_ptr)();				/* Cancel or clear any pending TP timeout */
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
			if (csa->now_crit)
				rel_crit(curreg);			/* release any crit regions */
		}
		tp_unwind(newlevel, ROLLBACK_INVOCATION);
		dollar_trestart = 0;
		if (gv_currkey != NULL)
		{
			gv_currkey->base[0] = '\0';
			gv_currkey->end = gv_currkey->prev = 0;
		}
		if (NULL != gv_target)
			gv_target->clue.end = 0;
		RESTORE_GV_CUR_REGION;
		JOBINTR_TP_RETHROW; /* rethrow job interrupt($ZINT) if $ZTEXIT, when coerced to boolean, is true */
	} else
	{
		tp_incr_clean_up(newlevel);
		RESTORE_GV_CUR_REGION;
		tp_unwind(newlevel, ROLLBACK_INVOCATION);
	}
	DEBUG_ONLY(
		if (!process_exiting)
		{
			DBG_CHECK_GVTARGET_CSADDRS_IN_SYNC;
		}
	)
}
