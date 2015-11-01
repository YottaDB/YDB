/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

#include <varargs.h>

#include "hashdef.h"
#include "lv_val.h"
#include "sbs_blk.h"
#include "subscript.h"
#include "lvname_info.h"
#include "callg.h"

lv_val* op_m_srchindx(va_alist)
va_dcl
{

	va_list			var;
	int			cur_subscr;
	static lvname_info_ptr	lvn_info = NULL;
	unsigned char           buff[MAX_STRLEN], *endbuff;

	if (!lvn_info)
		lvn_info = (lvname_info_ptr) malloc(sizeof(struct lvname_info_struct));
	VAR_START(var);
	lvn_info->total_lv_subs = va_arg(var, int4);
	lvn_info->start_lvp = va_arg(var, lv_val *);  	/* process arg[1] */
	for (cur_subscr = 0;  cur_subscr < lvn_info->total_lv_subs - 1;  cur_subscr++)
		lvn_info->lv_subs[cur_subscr] = va_arg(var, mval *); /* subcsripts are args[2:argcnt] */
	lvn_info->end_lvp = (lv_val *)callg((int(*)())op_srchindx, lvn_info);
	return lvn_info->end_lvp;
}
