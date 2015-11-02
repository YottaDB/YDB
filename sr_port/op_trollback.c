/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
GBLREF	gd_addr			*gd_header;
GBLREF	tp_region		*tp_reg_list;	/* Chained list of regions used in this transaction not cleared on tp_restart */
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	void			(*tp_timeout_clear_ptr)(void);
GBLREF	int			process_exiting;
GBLREF	gd_binding		*gd_map;
#ifdef GTM_TRIGGER
GBLREF	int4			gtm_trigger_depth;
GBLREF	int4			tstart_trigger_depth;
#endif
#ifdef DEBUG
GBLREF	boolean_t		donot_INVOKE_MUMTSTART;
GBLREF	unsigned char		*tpstackbase, *tpstacktop;
#endif
GBLREF	boolean_t		implicit_trollback;
GBLREF	tp_frame		*tp_pointer;

error_def(ERR_TLVLZERO);
error_def(ERR_TROLLBK2DEEP);
error_def(ERR_INVROLLBKLVL);

#define	RESTORE_GV_CUR_REGION						\
{									\
	gv_cur_region = save_cur_region;				\
	TP_CHANGE_REG(gv_cur_region);					\
}

void	op_trollback(int rb_levels)		/* rb_levels -> # of transaction levels by which we need to rollback : BYPASSOK */
{
	boolean_t	lcl_implicit_trollback = FALSE, reg_reset;
	uint4		newlevel;
	gd_region	*save_cur_region;	/* saved copy of gv_cur_region before tp_clean_up/tp_incr_clean_up modifies it */
	gd_region	*curreg;
	gv_key		*gv_orig_key_ptr;
	sgmnt_addrs	*csa;
	tp_region	*tr;
	int		tl;

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
	DBG_CHECK_GVTARGET_GVCURRKEY_IN_SYNC(CHECK_CSA_TRUE);
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
		reg_reset = FALSE;
		if (!process_exiting && lcl_implicit_trollback && tp_pointer->implicit_tstart)
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
			COPY_KEY(gv_currkey, gv_orig_key_ptr);
			gv_target = tp_pointer->orig_gv_target;
			gd_header = tp_pointer->gd_header;
			gd_map = gd_header->maps;
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
		tp_unwind(newlevel, ROLLBACK_INVOCATION, NULL);
		/* Now that we are out of TP, reset the debug-only global variable that is relevant only if we are in TP */
		DEBUG_ONLY(donot_INVOKE_MUMTSTART = FALSE;)
		dollar_trestart = 0;
		if (!reg_reset)
			RESTORE_GV_CUR_REGION;
		JOBINTR_TP_RETHROW; /* rethrow job interrupt($ZINT) if $ZTEXIT, when coerced to boolean, is true */
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
