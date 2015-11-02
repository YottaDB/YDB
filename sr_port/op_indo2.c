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

#include "toktyp.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "mdq.h"
#include "advancewindow.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include "op.h"
#include "fullbool.h"

error_def(ERR_INDMAXNEST);
error_def(ERR_VAREXPECTED);

void	op_indo2(mval *dst, mval *target, mval *value)
{
	icode_str	indir_src;
	int		rval;
	mstr		*obj, object;
	oprtype		v, sav_opr;
	triple		*s, *src, *oldchain, tmpchain, *r;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TREF(ind_source_sp) >= TREF(ind_source_top)) || (TREF(ind_result_sp) >= TREF(ind_result_top)))
		rts_error(VARLSTCNT(1) ERR_INDMAXNEST); /* mdbcondition_handler resets ind_result_sp & ind_source_sp */
	MV_FORCE_DEFINED(value);
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_fnorder2;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		obj = &object;
		comp_init(&target->str);
		src = newtriple(OC_IGETSRC);
		s = maketriple(OC_NOOP);	/* we'll fill it in as we go along */
		switch (TREF(window_token))
		{
		case TK_IDENT:
			if (TREF(director_token) != TK_LPAREN)
			{
				s->opcode = OC_FNLVNAMEO2;
				s->operand[0] = put_str((TREF(window_ident)).addr,(TREF(window_ident)).len);
				s->operand[1] = put_tref(src);
				ins_triple(s);
				advancewindow();
				rval = EXPR_GOOD;
				break;
			}
			if (EXPR_FAIL != (rval = lvn(&s->operand[0], OC_SRCHINDX, s)))	/* NOTE assignment */
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
			if (EXPR_FAIL != (rval = gvn()))				/* NOTE assignment */
			{
				s->opcode = OC_GVO2;
				s->operand[0] = put_tref(src);
				ins_triple(s);
			}
			break;
		case TK_ATSIGN:
			TREF(saw_side_effect) = TREF(shift_side_effects);
			if (TREF(shift_side_effects) && (GTM_BOOL == TREF(gtm_fullbool)))
			{
				dqinit(&tmpchain, exorder);
				oldchain = setcurtchain(&tmpchain);
				if (EXPR_FAIL != (rval = indirection(&s->operand[0])))	/* NOTE assignment */
				{
					s->opcode = OC_INDO2;
					s->operand[1] = put_tref(src);
					ins_triple(s);
					newtriple(OC_GVSAVTARG);
					setcurtchain(oldchain);
					dqadd(TREF(expr_start), &tmpchain, exorder);
					TREF(expr_start) = tmpchain.exorder.bl;
					r = newtriple(OC_GVRECTARG);
					r->operand[0] = put_tref(TREF(expr_start));
				} else
					setcurtchain(oldchain);
			} else
			{
				if (EXPR_FAIL != (rval = indirection(&s->operand[0])))	/* NOTE assignment */
				{
					s->opcode = OC_INDO2;
					s->operand[1] = put_tref(src);
					ins_triple(s);
				}
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			rval = FALSE;
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
