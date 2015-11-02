/****************************************************************
 *								*
 *	Copyright 2004, 2011 Fidelity Information Services, Inc	*
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

GBLREF	char			window_token;
GBLREF	mval			**ind_source_sp, **ind_source_top;
GBLREF	mval			**ind_result_sp, **ind_result_top;

LITREF	mval			literal_null;

void	op_indincr(mval *dst, mval *increment, mval *target)
{
	bool		rval;
	mstr		object, *obj;
	oprtype		v;
	triple		*s, *src, *oldchain, tmpchain, *triptr;
	icode_str	indir_src;
	error_def(ERR_INDMAXNEST);
	error_def(ERR_VAREXPECTED);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_increment;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		comp_init(&target->str);
		src = newtriple(OC_IGETSRC);
		switch (window_token)
		{
			case TK_IDENT:
				if (rval = lvn(&v, OC_PUTINDX, 0))
				{
					s = newtriple(OC_FNINCR);
					s->operand[0] = v;
					s->operand[1] = put_tref(src);
				}
				break;
			case TK_CIRCUMFLEX:
				if (rval = gvn())
				{
					s = newtriple(OC_GVINCR);
					/* dummy fill below since emit_code does not like empty operand[0] */
					s->operand[0] = put_ilit(0);
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
					if (rval = indirection(&v))
					{
						s = newtriple(OC_INDINCR);
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
			*ind_source_sp++ = increment;
			comp_indr(&object);
		}
	} else
	{
		if (ind_source_sp + 1 >= ind_source_top || ind_result_sp + 1 >= ind_result_top)
			rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
		*ind_result_sp++ = dst;
		*ind_source_sp++ = increment;
		comp_indr(obj);
	}
}
