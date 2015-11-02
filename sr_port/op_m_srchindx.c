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

/*
 * ---------------------------------------------------------------------------
 * op_m_srchindx.c
 * ==============
 * Description:
 *  	Search of local variable key. If it does not exist, it issues an error.
 *	This is different than op_getindx and op_srchindx.
 *	Say, we have a(1,2,3)="three" and a(1) right hand side of merge command.
 *	op_getindx or, op_srchindx do not give me the lv_val * of a(1).
 *
 * Arguments:
 *	varargs : count, *lv_val, array of *mval
 *
 * Return:
 *	lv_val * of the node found. Otherwise issue error.
 *
 * ---------------------------------------------------------------------------
 */
#include "mdef.h"

#include <stdarg.h>

#include "lv_val.h"
#include "callg.h"

lv_val* op_m_srchindx(UNIX_ONLY_COMMA(int4 count) lv_val *lvarg, ...)
{

	va_list			var;
	int			cur_subscr;
	VMS_ONLY(int		count;)
	static lvname_info_ptr	lvn_info = NULL;

	if (!lvn_info)
		lvn_info = (lvname_info_ptr) malloc(SIZEOF(struct lvname_info_struct));
	VAR_START(var, lvarg);
	VMS_ONLY(va_count(count);)
	lvn_info->start_lvp = lvarg;  	/* process arg[1] */
	lvn_info->total_lv_subs = count;
	for (cur_subscr = 0;  cur_subscr < lvn_info->total_lv_subs - 1;  cur_subscr++)
		lvn_info->lv_subs[cur_subscr] = va_arg(var, mval *); /* subcsripts are args[2:argcnt] */
	va_end(var);
	lvn_info->end_lvp = (lv_val *)callg((INTPTR_T (*)(intszofptr_t argcnt_arg, ...))op_srchindx, (gparam_list *)lvn_info);
	return lvn_info->end_lvp;
}
