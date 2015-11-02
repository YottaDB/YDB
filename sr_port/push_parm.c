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

#include <stdarg.h>

#include "gtm_stdio.h"

#include "lv_val.h"
#include "rtnhdr.h"
#include "mv_stent.h"
#include "compiler.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF mv_stent		*mv_chain;
GBLREF unsigned char	*msp, *stackbase, *stackwarn, *stacktop;
GBLREF symval		*curr_symval;

/* Create lv_val on stack to hold copy of parameter. Address of created lv_val will be used by op_bindparm() */
void push_parm(UNIX_ONLY_COMMA(unsigned int totalcnt) int truth_value, ...)
{
	va_list		var;
	mval		*ret_value;
	int		mask;
	VMS_ONLY(unsigned	totalcnt;)
	unsigned	actualcnt;
	parm_blk	*parm;
	mv_stent	*mvp_blk;
	int		i;
	lv_val		*actp;
	mval		*actpmv;

	error_def	(ERR_STACKOFLOW);
	error_def	(ERR_STACKCRIT);

	VAR_START(var, truth_value);
	VMS_ONLY(va_count(totalcnt));
	assert(4 <= totalcnt);
	ret_value = va_arg(var, mval *);
	mask = va_arg(var, int);
	actualcnt = va_arg(var, unsigned int);
	assert(4 + actualcnt == totalcnt);
	assert(MAX_ACTUALS >= actualcnt);
	PUSH_MV_STENT(MVST_PARM);
	parm = (parm_blk *)malloc(SIZEOF(parm_blk) - SIZEOF(lv_val *) + actualcnt * SIZEOF(lv_val *));
	parm->actualcnt = actualcnt;
	parm->mask = mask;
	mvp_blk = mv_chain;
	mvp_blk->mv_st_cont.mvs_parm.save_truth = truth_value;
	mvp_blk->mv_st_cont.mvs_parm.ret_value = (mval *)NULL;
	mvp_blk->mv_st_cont.mvs_parm.mvs_parmlist = parm;
	for (i = 0;  i < actualcnt;  i++)
	{
		actp = va_arg(var, lv_val *);
		if (!(mask & 1 << i))
		{	/* Not a dotted pass-by-reference parm */
			actpmv = &actp->v;
			if ((!MV_DEFINED(actpmv)) && (actpmv->str.addr != (char *)&actp->v))
				actpmv = underr(actpmv);
			PUSH_MV_STENT(MVST_PVAL);
			mv_chain->mv_st_cont.mvs_pval.mvs_val = lv_getslot(curr_symval);
			LVVAL_INIT(mv_chain->mv_st_cont.mvs_pval.mvs_val, curr_symval);
			mv_chain->mv_st_cont.mvs_pval.mvs_val->v = *actpmv;		/* Copy mval input */
			mv_chain->mv_st_cont.mvs_pval.mvs_ptab.save_value = NULL;	/* Filled in by op_bindparm */
			mv_chain->mv_st_cont.mvs_pval.mvs_ptab.hte_addr = NULL;
			DEBUG_ONLY(mv_chain->mv_st_cont.mvs_pval.mvs_ptab.nam_addr = NULL);
			parm->actuallist[i] = (lv_val *)&mv_chain->mv_st_cont.mvs_pval;
		} else
			/* Dotted pass-by-reference parm. No save of previous value, just pass lvval */
			parm->actuallist[i] = actp;
	}
	va_end(var);
	mvp_blk->mv_st_cont.mvs_parm.ret_value = ret_value;
	return;
}
