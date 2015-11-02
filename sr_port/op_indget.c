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
#include "compiler.h"
#include "toktyp.h"
#include "opcode.h"
#include "indir_enum.h"
#include "mdq.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include "op.h"
#include "rtnhdr.h"
#include "valid_mname.h"

GBLREF	symval			*curr_symval;
GBLREF	char			window_token;
GBLREF	mval			**ind_source_sp, **ind_source_top;
GBLREF	mval			**ind_result_sp, **ind_result_top;

error_def(ERR_INDMAXNEST);
error_def(ERR_VAREXPECTED);

void	op_indget(mval *dst, mval *target, mval *value)
{
	bool		rval;
	mstr		object, *obj;
	oprtype		v;
	triple		*s, *src, *oldchain, tmpchain, *r, *triptr;
	icode_str	indir_src;
	var_tabent	targ_key;
	ht_ent_mname	*tabent;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_DEFINED(value);
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_get;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		if (valid_mname(&target->str))
		{
			targ_key.var_name = target->str;
			COMPUTE_HASH_MNAME(&targ_key);
			tabent = lookup_hashtab_mname(&curr_symval->h_symtab, &targ_key);
			if (!tabent || !LV_IS_VAL_DEFINED(tabent->value))
				*dst = *value;
			else
				*dst = ((lv_val *)tabent->value)->v;
			dst->mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
			return;
		}
		comp_init(&target->str);
		src = newtriple(OC_IGETSRC);
		switch (window_token)
		{
			case TK_IDENT:
				if (rval = lvn(&v, OC_SRCHINDX, 0))
				{
					s = newtriple(OC_FNGET2);
					s->operand[0] = v;
					s->operand[1] = put_tref(src);
				}
				break;
			case TK_CIRCUMFLEX:
				if (rval = gvn())
				{
					r = newtriple(OC_FNGVGET1);
					s = newtriple(OC_FNGVGET2);
					s->operand[0] = put_tref(r);
					s->operand[1] = put_tref(src);
				}
				break;
			case TK_ATSIGN:
				if (TREF(shift_side_effects))
				{
					dqinit(&tmpchain, exorder);
					oldchain = setcurtchain(&tmpchain);
					if (rval = indirection(&v))
					{
						s = newtriple(OC_INDGET);
						s->operand[0] = v;
						s->operand[1] = put_tref(src);
						newtriple(OC_GVSAVTARG);
						setcurtchain(oldchain);
						dqadd(TREF(expr_start), &tmpchain, exorder);
						TREF(expr_start) = tmpchain.exorder.bl;
						triptr = newtriple(OC_GVRECTARG);
						triptr->operand[0] = put_tref(TREF(expr_start));
					} else
						setcurtchain(oldchain);
				} else
				{
					if (rval = indirection(&v))
					{
						s = newtriple(OC_INDGET);
						s->operand[0] = v;
						s->operand[1] = put_tref(src);
					}
				}
				break;
			default:
				stx_error(ERR_VAREXPECTED);
				break;
		}
		v = put_tref(s);
		if (comp_fini(rval, &object, OC_IRETMVAL, &v, target->str.len))
		{
			indir_src.str.addr = target->str.addr;
			cache_put(&indir_src, &object);
			if (ind_source_sp + 1 >= ind_source_top || ind_result_sp + 1 >= ind_result_top)
				rts_error(VARLSTCNT(1) ERR_INDMAXNEST);

			*ind_result_sp++ = dst;
			*ind_source_sp++ = value;
			comp_indr(&object);
		}
	} else
	{
		if (ind_source_sp + 1 >= ind_source_top || ind_result_sp + 1 >= ind_result_top)
			rts_error(VARLSTCNT(1) ERR_INDMAXNEST);

		*ind_result_sp++ = dst;
		*ind_source_sp++ = value;
		comp_indr(obj);
	}
}
