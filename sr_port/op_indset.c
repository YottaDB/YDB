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

#include "lv_val.h"
#include "toktyp.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "cache.h"
#include "op.h"
#include <rtnhdr.h>
#include "valid_mname.h"
#include "stack_frame.h"
#include "is_canonic_name.h"

GBLREF	symval		*curr_symval;
GBLREF	stack_frame	*frame_pointer;

error_def(ERR_VAREXPECTED);

void	op_indset(mval *target, mval *value)
{
	char 			new;
	ht_ent_mname 		*tabent;
	icode_str		indir_src;
	int			rval;
	mstr			*obj, object;
	oprtype			v;
	triple			*s, *src;
	var_tabent		targ_key;
	lv_gv_name		glvname;
	int			subs, *start, *stop;
	gv_name_and_subscripts	start_buff, stop_buff;
	lv_val			*ret_lv, tmp_lv;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_DEFINED(value);
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_set;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		if (valid_mname(&target->str))
		{
			targ_key.var_name = target->str;
			COMPUTE_HASH_MNAME(&targ_key);
			targ_key.marked = NOT_MARKED;
			if (add_hashtab_mname_symval(&curr_symval->h_symtab, &targ_key, NULL, &tabent, FALSE))
				lv_newname(tabent, curr_symval);
			((lv_val *)tabent->value)->v = *value;
			return;
		} else
		{
			DO_OP_GVNAME_IF_NEEDED(target, subs, start_buff, stop_buff, start, stop, glvname);
			if (GV_NAME == glvname)
			{
				op_gvput(value);
				return;
			} else if ((LV_NAME == glvname)
					&& (NULL != (ret_lv = op_putindx_runtime(target, subs, start, stop, &tmp_lv))))
			{
				ret_lv->v = *value;
				return;
			}
		}
		obj = &object;
		comp_init(&target->str, NULL);
		src = maketriple(OC_IGETSRC);
		ins_triple(src);
		switch (TREF(window_token))
		{
		case TK_IDENT:
			if (EXPR_FAIL != (rval = lvn(&v, OC_PUTINDX, 0)))	/* NOTE assignment */
			{
				s = maketriple(OC_STO);
				s->operand[0] = v;
				s->operand[1] = put_tref(src);
				ins_triple(s);
			}
			break;
		case TK_CIRCUMFLEX:
			if (EXPR_FAIL != (rval = gvn()))			/* NOTE assignment */
			{
				s = maketriple(OC_GVPUT);
				s->operand[0] = put_tref(src);
				ins_triple(s);
			}
			break;
		case TK_ATSIGN:
			if (EXPR_FAIL != (rval = indirection(&v)))		/* NOTE assignment */
			{
				s = maketriple(OC_INDSET);
				s->operand[0] = v;
				s->operand[1] = put_tref(src);
				ins_triple(s);
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			rval = EXPR_FAIL;
			break;
		}
		if (EXPR_FAIL == comp_fini(rval, obj, OC_RET, NULL, NULL, target->str.len))
			return;
		indir_src.str.addr = target->str.addr;
		cache_put(&indir_src, obj);
		/* Fall into code activation below */
	}
	TREF(ind_source) = value;
	comp_indr(obj);
}
