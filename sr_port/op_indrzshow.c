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
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mlkdef.h"
#include "zshow.h"
#include "cache.h"
#include "op.h"

GBLREF mval **ind_source_sp, **ind_source_top;
GBLREF char window_token;

void op_indrzshow(mval *s1, mval *s2)
{
	icode_str	indir_src;
	mstr		*obj, object;
	bool		rval;
	oprtype		v;
	triple		*src, *r, *outtype, *lvar;
	error_def(ERR_VAREXPECTED);
	error_def(ERR_INDMAXNEST);

	MV_FORCE_STR(s2);
	indir_src.str = s2->str;
	indir_src.code = indir_zshow;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		comp_init(&s2->str);
		src = maketriple(OC_IGETSRC);
		ins_triple(src);
		switch(window_token)
		{
		case TK_CIRCUMFLEX:
			if (rval = gvn())
			{	r = maketriple(OC_ZSHOW);
				outtype = newtriple(OC_PARAMETER);
				r->operand[1] = put_tref(outtype);
				r->operand[0] = put_tref(src);
				outtype->operand[0] = put_ilit(ZSHOW_GLOBAL);
				ins_triple(r);
			}
			break;
		case TK_IDENT:
			if (rval = lvn(&v, OC_PUTINDX, 0))
			{	r = maketriple(OC_ZSHOWLOC);
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
			if (rval = indirection(&v))
			{	r = newtriple(OC_INDRZSHOW);
				r->operand[0] = put_tref(src);
				r->operand[1] = v;
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			break;
		}
		if (comp_fini(rval, &object, OC_RET, 0, s2->str.len))
		{
			indir_src.str = s2->str;
			indir_src.code = indir_zshow;
			cache_put(&indir_src, &object);
			obj = &object;
		} else
			return;
	}
	*ind_source_sp++ = s1;
	if (ind_source_sp >= ind_source_top)
		rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
	comp_indr(obj);
	return;
}
