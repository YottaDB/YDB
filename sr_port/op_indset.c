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

#include "lv_val.h"
#include "toktyp.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "cache.h"
#include "op.h"
#include "rtnhdr.h"
#include "valid_mname.h"

GBLREF	symval			*curr_symval;
GBLREF	char			window_token;
GBLREF	mval			**ind_source_sp, **ind_source_top;

void	op_indset(mval *target, mval *value)
{
	error_def(ERR_INDMAXNEST);
	error_def(ERR_VAREXPECTED);
	bool		rval;
	mstr		object, *obj;
	oprtype		v;
	triple		*s, *src;
	char 		new;
	icode_str	indir_src;
	var_tabent	targ_key;
	ht_ent_mname 	*tabent;

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
			targ_key.marked = FALSE;
			if (add_hashtab_mname_symval(&curr_symval->h_symtab, &targ_key, NULL, &tabent))
				lv_newname(tabent, curr_symval);
			((lv_val *)tabent->value)->v = *value;
			return;
		}
		comp_init(&target->str);
		src = maketriple(OC_IGETSRC);
		ins_triple(src);
		switch (window_token)
		{
		case TK_IDENT:
			if (rval = lvn(&v, OC_PUTINDX, 0))
			{
				s = maketriple(OC_STO);
				s->operand[0] = v;
				s->operand[1] = put_tref(src);
				ins_triple(s);
			}
			break;
		case TK_CIRCUMFLEX:
			if (rval = gvn())
			{
				s = maketriple(OC_GVPUT);
				s->operand[0] = put_tref(src);
				ins_triple(s);
			}
			break;
		case TK_ATSIGN:
			if (rval = indirection(&v))
			{
				s = maketriple(OC_INDSET);
				s->operand[0] = v;
				s->operand[1] = put_tref(src);
				ins_triple(s);
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			break;
		}
		if (comp_fini(rval, &object, OC_RET, 0, target->str.len))
		{
			indir_src.str.addr = target->str.addr;
			cache_put(&indir_src, &object);
			*ind_source_sp++ = value;
			if (ind_source_sp >= ind_source_top)
				rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
			comp_indr(&object);
		}
	}
	else
	{
		*ind_source_sp++ = value;
		if (ind_source_sp >= ind_source_top)
			rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
		comp_indr(obj);
	}
}
