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

#include "gtm_string.h"		/* needed by INCREMENT_EXPR_DEPTH */
#include "lv_val.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "cache.h"
#include "op.h"
#include <rtnhdr.h>
#include "valid_mname.h"
#include "is_canonic_name.h"

GBLREF	bool			undef_inhibit;
GBLREF	symval			*curr_symval;
LITREF	mval			literal_null;

error_def(ERR_UNDEF);

void	op_indglvn(mval *v, mval *dst)
{
	ht_ent_mname		*tabent;
	icode_str		indir_src;
	int			rval;
	mstr			*obj, object;
	oprtype			x, getdst;
	var_tabent		targ_key;
	lv_gv_name		glvname;
	int			subs, *start, *stop;
	gv_name_and_subscripts	start_buff, stop_buff;
	lv_val			*ret_lv, tmp_lv;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(v);
	indir_src.str = v->str;
	indir_src.code = indir_glvn;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		if (valid_mname(&v->str))
		{
			targ_key.var_name = v->str;
			COMPUTE_HASH_MNAME(&targ_key);
			tabent = lookup_hashtab_mname(&curr_symval->h_symtab, &targ_key);
			assert(NULL == tabent ||  NULL != tabent->value);
			if (!tabent || !LV_IS_VAL_DEFINED(tabent->value))
			{
				if (undef_inhibit)
				{
					*dst = literal_null;
					return;
				} else
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_UNDEF, 2, v->str.len, v->str.addr);
			}
			*dst = ((lv_val *)tabent->value)->v;
			dst->mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
			return;
		} else
		{
			DO_OP_GVNAME_IF_NEEDED(v, subs, start_buff, stop_buff, start, stop, glvname);
			if (GV_NAME == glvname)
			{
				op_gvget(dst);
				return;
			} else if ((LV_NAME == glvname) && (NULL != (ret_lv = op_getindx_runtime(v, subs, start, stop, &tmp_lv))))
			{
				*dst = ret_lv->v;
				dst->mvtype &= ~MV_ALIASCONT;
				return;
			}
		}
		obj = &object;
		comp_init(&v->str, &getdst);
		INCREMENT_EXPR_DEPTH;
		rval = glvn(&x);
		DECREMENT_EXPR_DEPTH;
		if (EXPR_FAIL == comp_fini(rval, obj, OC_IRETMVAL, &x, &getdst, v->str.len))
			return;
		indir_src.str.addr = v->str.addr;
		cache_put(&indir_src, obj);
		/* Fall into code activation below */
	}
	TREF(ind_result) = dst;
	comp_indr(obj);
	return;
}
