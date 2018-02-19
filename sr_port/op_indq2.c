/****************************************************************
 *								*
 * Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Note: Code in this module is based on op_indo2.c which has a FIS copyright hence
 * the copyright is copied over here even though this module was not created by FIS.
 */

#include "mdef.h"

#include "compiler.h"
#include "opcode.h"
#include "op.h"
#include "lv_val.h"
#include "mvalconv.h"
#include "glvn_pool.h"
#include "callg.h"

error_def(ERR_QUERY2);
error_def(ERR_VAREXPECTED);

GBLREF	symval			*curr_symval;

LITREF	mval	literal_one;
LITREF	mval	literal_minusone;

void	op_indq2(mval *dst, uint4 indx, mval *direct)
{
	int		i;
	glvn_pool_entry	*slot;
	int4		dummy_intval;
	intszofptr_t	n;
	lv_val		*lv;
	mval		*key;
	opctype		oc;
	gparam_list	paramlist;
	ht_ent_mname	*tabent;
	mname_entry	targ_key;
	lvname_info	*lvn_info;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_NUM(direct);
	if (!MV_IS_TRUEINT(direct, &dummy_intval)
			|| ((literal_one.m[1] != direct->m[1]) && (literal_minusone.m[1] != direct->m[1])))
		rts_error(VARLSTCNT(1) ERR_QUERY2);
	slot = &((TREF(glvn_pool_ptr))->slot[indx]);
	oc = slot->sav_opcode;
	if (OC_SAVLVN == oc)
	{	/* lvn */
		lvn_info = (lvname_info *)&slot->glvn_info;
		n = lvn_info->total_lv_subs + 2;
		assert(3 <= n);
		/* Prepare paramlist to pass to "op_fnquery" or "op_fnreversequery" */
		paramlist.n = n;
		paramlist.arg[0] = dst;
		paramlist.arg[1] = slot->lvname;
		targ_key.var_name = slot->lvname->str;
		COMPUTE_HASH_MNAME(&targ_key);
		targ_key.marked = FALSE;
		if (add_hashtab_mname_symval(&curr_symval->h_symtab, &targ_key, NULL, &tabent))
			lv_newname(tabent, curr_symval);
		paramlist.arg[2] = (lv_val *)tabent->value;
		for (i = 3; i < n; i++)
			paramlist.arg[i] = lvn_info->lv_subs[i - 3];
		if (literal_one.m[1] == direct->m[1])
			callg((callgfnptr)op_fnquery, &paramlist);
		else
			callg((callgfnptr)op_fnreversequery, &paramlist);
	} else if (OC_NOOP != oc)					/* if indirect error blew set up, skip this */
	{	/* gvn */
		op_rfrshgvn(indx, oc);
		/* like op_gvno2 */
		if (literal_one.m[1] == direct->m[1])
			op_gvquery(dst);
		else
			op_gvreversequery(dst);
	}
	return;
}
