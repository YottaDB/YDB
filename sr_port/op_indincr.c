/****************************************************************
 *								*
 *	Copyright 2004, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "toktyp.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "mdq.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include "op.h"
#include "mvalconv.h"	/* for i2mval prototype for the MV_FORCE_MVAL macro */
#include "fullbool.h"

error_def(ERR_VAREXPECTED);

void	op_indincr(mval *dst, mval *increment, mval *target)
{
	icode_str	indir_src;
	int		rval;
	mstr		*obj, object;
	oprtype		v, getdst;
	triple		*s, *src, *oldchain, tmpchain, *triptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_increment;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		obj = &object;
		comp_init(&target->str, &getdst);
		src = newtriple(OC_IGETSRC);
		switch (TREF(window_token))
		{
		case TK_IDENT:
			if (EXPR_FAIL != (rval = lvn(&v, OC_PUTINDX, 0)))	/* NOTE assignment */
			{
				s = newtriple(OC_FNINCR);
				s->operand[0] = v;
				s->operand[1] = put_tref(src);
			}
			break;
		case TK_CIRCUMFLEX:
			if (EXPR_FAIL != (rval = gvn()))			/* NOTE assignment */
			{
				s = newtriple(OC_GVINCR);
				/* dummy fill below since emit_code does not like empty operand[0] */
				s->operand[0] = put_ilit(0);
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
					s = newtriple(OC_INDINCR);
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
					s = newtriple(OC_INDINCR);
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
		if (EXPR_FAIL == comp_fini(rval, obj, OC_IRETMVAL, &v, &getdst, target->str.len))
			return;
		indir_src.str.addr = target->str.addr;
		cache_put(&indir_src, obj);
		/* Fall into code activation below */
	}
	TREF(ind_result) = dst;
	TREF(ind_source) = increment;
	comp_indr(obj);
	return;
}
