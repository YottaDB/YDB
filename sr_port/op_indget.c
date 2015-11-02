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
#include <rtnhdr.h>
#include "valid_mname.h"
#include "fullbool.h"

GBLREF	symval	*curr_symval;

error_def(ERR_INDMAXNEST);
error_def(ERR_VAREXPECTED);

void	op_indget(mval *dst, mval *target, mval *value)
{
	icode_str	indir_src;
	int		rval;
	ht_ent_mname	*tabent;
	mstr		*obj, object;
	oprtype		v;
	triple		*s, *src, *oldchain, tmpchain, *r, *triptr;
	var_tabent	targ_key;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TREF(ind_source_sp) >= TREF(ind_source_top)) || (TREF(ind_result_sp) >= TREF(ind_result_top)))
		rts_error(VARLSTCNT(1) ERR_INDMAXNEST); /* mdbcondition_handler resets ind_result_sp & ind_source_sp */
	MV_FORCE_DEFINED(value);
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_get;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		obj = &object;
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
		switch (TREF(window_token))
		{
		case TK_IDENT:
			if (EXPR_FAIL != (rval = lvn(&v, OC_SRCHINDX, 0)))	/* NOTE assignment */
			{
				s = newtriple(OC_FNGET2);
				s->operand[0] = v;
				s->operand[1] = put_tref(src);
			}
			break;
		case TK_CIRCUMFLEX:
			if (EXPR_FAIL != (rval = gvn()))			/* NOTE assignment */
			{
				r = newtriple(OC_FNGVGET1);
				s = newtriple(OC_FNGVGET2);
				s->operand[0] = put_tref(r);
				s->operand[1] = put_tref(src);
			}
			break;
		case TK_ATSIGN:
			TREF(saw_side_effect) = TREF(shift_side_effects);
			if (TREF(shift_side_effects) && (GTM_BOOL == TREF(gtm_fullbool)))
			{
				dqinit(&tmpchain, exorder);
				oldchain = setcurtchain(&tmpchain);
				if (EXPR_FAIL != (rval = indirection(&v)))	/* NOTE assignment */
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
				if (EXPR_FAIL != (rval = indirection(&v)))	/* NOTE assignment */
				{
					s = newtriple(OC_INDGET);
					s->operand[0] = v;
					s->operand[1] = put_tref(src);
				}
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			rval = EXPR_FAIL;
			break;
		}
		v = put_tref(s);
		if (EXPR_FAIL == comp_fini(rval, obj, OC_IRETMVAL, &v, target->str.len))
			return;
		indir_src.str.addr = target->str.addr;
		cache_put(&indir_src, obj);
		/* Fall into code activation below */
	}
	*(TREF(ind_result_sp))++ = dst;
	*(TREF(ind_source_sp))++ = value;
	comp_indr(obj);
	return;
}
