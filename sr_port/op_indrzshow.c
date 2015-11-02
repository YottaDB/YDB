/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mlkdef.h"
#include "zshow.h"
#include "cache.h"
#include "op.h"

error_def(ERR_VAREXPECTED);

void op_indrzshow(mval *s1, mval *s2)
{
	icode_str	indir_src;
	int		rval;
	mstr		*obj, object;
	oprtype		v;
	triple		*lvar, *outtype, *r, *src;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(s2);
	indir_src.str = s2->str;
	indir_src.code = indir_zshow;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		obj = &object;
		comp_init(&s2->str, NULL);
		src = maketriple(OC_IGETSRC);
		ins_triple(src);
		switch(TREF(window_token))
		{
		case TK_CIRCUMFLEX:
			if (EXPR_FAIL != (rval = gvn()))			/* NOTE assignment */
			{
				r = maketriple(OC_ZSHOW);
				outtype = newtriple(OC_PARAMETER);
				r->operand[1] = put_tref(outtype);
				r->operand[0] = put_tref(src);
				outtype->operand[0] = put_ilit(ZSHOW_GLOBAL);
				ins_triple(r);
			}
			break;
		case TK_IDENT:
			if (EXPR_FAIL != (rval = lvn(&v, OC_PUTINDX, 0)))	/* NOTE assignment */
			{
				r = maketriple(OC_ZSHOWLOC);
				outtype = newtriple(OC_PARAMETER);
				r->operand[1] = put_tref(outtype);
				r->operand[0] = put_tref(src);
				lvar = newtriple(OC_PARAMETER);
				outtype->operand[1] = put_tref(lvar);
				lvar->operand[0] = v;
				outtype->operand[0] = put_ilit(ZSHOW_LOCAL);
				ins_triple(r);
			}
			break;
		case TK_ATSIGN:
			if (EXPR_FAIL != (rval = indirection(&v)))		/* NOTE assignment */
			{
				r = newtriple(OC_INDRZSHOW);
				r->operand[0] = put_tref(src);
				r->operand[1] = v;
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			rval = EXPR_FAIL;
			break;
		}
		if (EXPR_FAIL == comp_fini(rval, obj, OC_RET, NULL, NULL, s2->str.len))
			return;
		indir_src.str = s2->str;
		indir_src.code = indir_zshow;
		cache_put(&indir_src, obj);
		/* Fall into code activation below */
	}
	TREF(ind_source) = s1;
	comp_indr(obj);
	return;
}
