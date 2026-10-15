/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2026 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
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
#include "indr_fast_lvn.h"
#include "valid_mname.h"
#include <rtnhdr.h>	/* see HDR_FILE_INCLUDE_SYNTAX comment in mdef.h for why <> syntax is needed */

GBLREF	bool			undef_inhibit;
GBLREF	symval			*curr_symval;
LITREF	mval			literal_null;

void	op_indglvn(mval *v,mval *dst)
{
	ht_ent_mname	*tabent;
	icode_str	indir_src;
	int		rval;
	mstr		*obj, object;
	oprtype		x, getdst;
	var_tabent	targ_key;
	indr_fast_lvn	lvn;
	lv_val		*node;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(v);
	indir_src.str = v->str;
	indir_src.code = indir_glvn;
	if (indr_fast_lvn_parse(v, &lvn))
	{	/* Fast indirection path, which avoids compiling the string for performance: it is a simple local variable
		 * reference, so fetch the value as the compiled code would. If the node has no value, leave the UNDEF
		 * error (or the NOUNDEF null string) to the compiled code.
		 */
		node = indr_fast_lvn_node(&lvn, lvn.nsubs);
		if ((NULL != node) && LV_IS_VAL_DEFINED(node))
		{
			*dst = node->v;
			/* The mval of an alias container node (SET *x(1)=y) has MV_ALIASCONT set and holds a pointer to
			 * the local variable it refers to. Its value for M code is the null string. Clear the flag so that
			 * the result is a plain value, as op_fnget and op_fnget1 do for the compiled code. op_sto clears it
			 * too, so a SET of the result is safe either way, but no other consumer should mistake the result
			 * for a container.
			 */
			dst->mvtype &= ~MV_ALIASCONT;
			indr_fast_lvn_done(&lvn);
			indr_fast_lvn_release(v);
			return;
		}
		indr_fast_lvn_done(&lvn);
	}
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
					RTS_ERROR_ABT(VARLSTCNT(4) ERR_LVUNDEF, 2, v->str.len, v->str.addr);
			}
			*dst = ((lv_val *)tabent->value)->v;
			dst->mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
			return;
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
