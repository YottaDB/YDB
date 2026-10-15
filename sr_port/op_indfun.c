/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "opcode.h"
#include "cache.h"
#include "gtm_limits.h"
#include "hashtab_objcode.h"
#include "op.h"
#include "lv_val.h"
#include "indir_enum.h"
#include "indr_fast_lvn.h"

UNIX_ONLY(GBLREF) VMS_ONLY(LITREF) int (*indir_fcn[])();

#define INDIR(a, b, c) c
static readonly opctype indir_opcode[] = {
#include "indir.h"
};
#undef INDIR

void op_indfun(mval *v, mint argcode, mval *dst)
{
	icode_str	indir_src;
	int		rval;
	mstr		*obj, object;
	oprtype		x, getdst;
	indr_fast_lvn	lvn;
	lv_val		*node;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((SIZEOF(indir_opcode)/SIZEOF(indir_opcode[0])) > argcode);
	assert(indir_opcode[argcode]);
	MV_FORCE_STR(v);
	indir_src.str = v->str;
	indir_src.code = argcode;
	if (((indir_fndata == argcode) || (indir_fnzdata == argcode) || (indir_fnorder1 == argcode))
			&& indr_fast_lvn_parse(v, &lvn))
	{	/* Fast indirection path, which avoids compiling the string for performance: it is a simple local variable
		 * reference, so do what the compiled code would do.
		 */
		if (indir_fnorder1 != argcode)
		{
			node = indr_fast_lvn_node(&lvn, lvn.nsubs);
			if (indir_fndata == argcode)
				op_fndata(node, dst);
			else
				op_fnzdata(node, dst);
			indr_fast_lvn_done(&lvn);
			indr_fast_lvn_release(v);
			return;
		}
		if (0 < lvn.nsubs)
		{
			op_fnorder(indr_fast_lvn_node(&lvn, lvn.nsubs - 1), lvn.subs[lvn.nsubs - 1], dst);
			indr_fast_lvn_done(&lvn);
			indr_fast_lvn_release(v);
			return;
		}
		indr_fast_lvn_done(&lvn);	/* $ORDER of an unsubscripted name is an error the compiler reports */
	}
	if (NULL == (obj = cache_get(&indir_src)))
	{
		obj = &object;
		comp_init(&v->str, &getdst);
		rval = (*indir_fcn[argcode])(&x, indir_opcode[argcode]);
		if (EXPR_FAIL == comp_fini(rval, obj, OC_IRETMVAL, &x, &getdst, v->str.len))
			return;
		indir_src.str.addr = v->str.addr;
		cache_put(&indir_src, obj);
		/* Fall into code activation below */
	}
	TREF(ind_result) = dst;			/* Where to store return value */
	comp_indr(obj);
	return;
}
