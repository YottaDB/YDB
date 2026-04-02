/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "indir_enum.h"
#include "lv_val.h"
#include "is_canonic_name.h"

GBLREF int (*indir_fcn[])();
LITREF mval     literal_zero;

#define INDIR(a, b, c) c
static readonly opctype indir_opcode[] = {
#include "indir.h"
};
#undef INDIR

void op_indfun(mval *v, mint argcode, mval *dst)
{
	icode_str		indir_src;
	int			rval;
	mstr			*obj, object;
	oprtype			x, getdst;
	lv_gv_name		glvname;
	int			subs, *start, *stop;
	gv_name_and_subscripts	start_buff, stop_buff;
	lv_val			*ret_lv, tmp_lv;
	mval			key;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert((SIZEOF(indir_opcode)/SIZEOF(indir_opcode[0])) > argcode);
	assert(indir_opcode[argcode]);
	MV_FORCE_STR(v);
	indir_src.str = v->str;
	indir_src.code = argcode;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		switch(argcode)
		{
			case indir_fndata:
			case indir_fnzdata:
				DO_OP_GVNAME_IF_NEEDED(v, subs, start_buff, stop_buff, start, stop, glvname);
				if (GV_NAME == glvname)
				{
					op_gvdata(dst);
					return;
				} else if ((LV_NAME == glvname))
				{
					if (NULL == (ret_lv = op_srchindx_runtime(v, subs, start, stop, &tmp_lv)))
						*dst = literal_zero;	/* $[Z]DATA returns zero when lvn was not found */
					else if (indir_fndata == argcode)
						op_fndata(ret_lv, dst);
					else
						op_fnzdata(ret_lv, dst);
					return;
				}
				break;
			case indir_fnorder1:
			case indir_fnzprevious:
				DO_OP_GVNAME_IF_NEEDED(v, subs, start_buff, stop_buff, start, stop, glvname);
				if (GV_NAME == glvname)
				{
					if (indir_fnorder1 == argcode)
						op_gvorder(dst);
					else
						op_zprevious(dst);
					return;
				} else if (LV_NAME == glvname)
				{
					assert (0 <= subs);
					if (0 == subs)
					{
						if (indir_fnorder1 == argcode)
							op_fnlvname(v, FALSE, dst);
						else
							op_fnlvprvname(v, dst);
						return;
					} else if (NULL != (ret_lv = op_srchindx_runtime(v, (subs - 1), start, stop, &tmp_lv)))
					{
						op_fnqsubscript_fast(v, subs, &key, subs, start[subs], stop[subs]);
						if (indir_fnorder1 == argcode)
							op_fnorder(ret_lv, &key, dst);
						else
							op_fnzprevious(ret_lv, &key, dst);
						return;
					}
				}
				break;
			case indir_fnquery:
				DO_OP_GVNAME_IF_NEEDED(v, subs, start_buff, stop_buff, start, stop, glvname);
				if (GV_NAME == glvname)
				{
					op_gvquery(dst);
					return;
				} else if ((LV_NAME == glvname) && (op_fnquery_runtime(v, subs, start, stop, dst)))
					return;
				break;
			default:		/* No-op: Use the compiler behavior */
				break;
		}
		obj = &object;
		comp_init(&v->str, &getdst);
		ASSERT_INDIR_FUNCTION_OPR(argcode);
		rval = (*(indir_fptr_opr_t)(indir_fcn[argcode]))(&x, indir_opcode[argcode]);
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
