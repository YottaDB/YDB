/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "tp_frame.h"
#include "tp_timeout.h"
#include "tp_unwind.h"
#include "op.h"

GBLREF	short		dollar_tlevel, dollar_trestart;
GBLREF	gv_key		*gv_currkey;
GBLREF	gv_namehead	*gv_target;
GBLREF tp_region	*tp_reg_list;		/* Chained list of regions used in this transaction not cleared on tp_restart */
GBLREF void		(*tp_timeout_clear_ptr)(void);

void	op_trollback(short rb_levels)		/* rb_levels -> # of transaction levels by which we need to rollback */
{
	short		newlevel;
	tp_region	*tr;

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
	if (!newlevel)
	{
		(*tp_timeout_clear_ptr)();				/* Cancel or clear any pending TP timeout */
		/* Do a rollback type cleanup (invalidate gv_target clues of read as well as
         	 * updated blocks). This is typically needed for a restart.
	 	 */
		tp_clean_up(TRUE);
		for (tr = tp_reg_list;  NULL != tr;  tr = tr->fPtr)
			if (TRUE == FILE_INFO(tr->reg)->s_addrs.now_crit)
				rel_crit(tr->reg);			/* release any crit regions */
		tp_unwind(newlevel, ROLLBACK_INVOCATION);
		dollar_trestart = 0;
		if (gv_currkey != NULL)
		{
			gv_currkey->base[0] = '\0';
			gv_currkey->end = gv_currkey->prev = 0;
		}
		if (NULL != gv_target)
			gv_target->clue.end = 0;
	} else
	{
		tp_incr_clean_up(newlevel);
		tp_unwind(newlevel, ROLLBACK_INVOCATION);
	}
}
