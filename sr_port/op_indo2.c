/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "toktyp.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "mdq.h"
#include "advancewindow.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include "op.h"
#include "underr.h"

GBLREF char 			window_token, director_token;
GBLREF mident 			window_ident;
GBLREF mval 			**ind_source_sp, **ind_source_top;
GBLREF mval 			**ind_result_sp, **ind_result_top;
GBLREF bool 			shift_gvrefs;
GBLREF triple 			*expr_start;

void	op_indo2(mval *dst, mval *target, mval *value)
{
	error_def(ERR_INDMAXNEST);
	error_def(ERR_VAREXPECTED);
	bool		rval;
	mstr		object, *obj;
	oprtype		v, sav_opr;
	triple		*s, *src, *oldchain, tmpchain, *r;
	icode_str	indir_src;

	MV_FORCE_DEFINED(value);
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_fnorder2;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		comp_init(&target->str);
		src = newtriple(OC_IGETSRC);
		s = maketriple(OC_NOOP);	/* we'll fill it in as we go along */
		switch (window_token)
		{
		case TK_IDENT:
			if (director_token != TK_LPAREN)
			{	s->opcode = OC_FNLVNAMEO2;
				s->operand[0] = put_str(window_ident.addr,window_ident.len);
				s->operand[1] = put_tref(src);
				ins_triple(s);
				advancewindow();
				rval = TRUE;
				break;
			}
			if (rval = lvn(&s->operand[0], OC_SRCHINDX, s))
			{
				s->opcode = OC_FNO2;
				sav_opr = s->operand[1];
				r = newtriple(OC_PARAMETER);
				r->operand[0] = sav_opr;
				r->operand[1] = put_tref(src);
				s->operand[1] = put_tref(r);
				ins_triple(s);
			}
			break;
		case TK_CIRCUMFLEX:
			if (rval = gvn())
			{
				s->opcode = OC_GVO2;
				s->operand[0] = put_tref(src);
				ins_triple(s);
			}
			break;
		case TK_ATSIGN:
			if (shift_gvrefs)
			{
				dqinit(&tmpchain, exorder);
				oldchain = setcurtchain(&tmpchain);
				if (rval = indirection(&s->operand[0]))
				{
					s->opcode = OC_INDO2;
					s->operand[1] = put_tref(src);
					ins_triple(s);
					newtriple(OC_GVSAVTARG);
					setcurtchain(oldchain);
					dqadd(expr_start, &tmpchain, exorder);
					expr_start = tmpchain.exorder.bl;
					r = newtriple(OC_GVRECTARG);
					r->operand[0] = put_tref(expr_start);
				}
				else
				{	setcurtchain(oldchain);
				}
			}
			else
			{
				if (rval = indirection(&s->operand[0]))
				{
					s->opcode = OC_INDO2;
					s->operand[1] = put_tref(src);
					ins_triple(s);
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
	}
	else
	{
		if (ind_source_sp + 1 >= ind_source_top || ind_result_sp + 1 >= ind_result_top)
			rts_error(VARLSTCNT(1) ERR_INDMAXNEST);

		*ind_result_sp++ = dst;
		*ind_source_sp++ = value;
		comp_indr(obj);
	}
}
