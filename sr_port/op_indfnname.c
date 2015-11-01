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

#include "toktyp.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "advancewindow.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include "op.h"

GBLREF	char			window_token;
GBLREF	mval			**ind_source_sp, **ind_source_top;
GBLREF	mval			**ind_result_sp, **ind_result_top;

void	op_indfnname(mval *dst, mval *target, mval *depth)
{
	bool		rval;
	mstr		object, *obj;
	oprtype		v;
	bool		gbl;
	triple		*s, *src;
	icode_str	indir_src;

	error_def(ERR_INDMAXNEST);
	error_def(ERR_VAREXPECTED);

	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_fnname;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		gbl = FALSE;
		comp_init(&target->str);
		src = newtriple(OC_IGETSRC);
		s = maketriple(OC_FNNAME);
		switch (window_token)
		{
		case TK_CIRCUMFLEX:
			gbl = TRUE;
			advancewindow();
			/* caution fall through */
		case TK_IDENT:
			if (rval = name_glvn(gbl, &s->operand[1]))
			{
				ins_triple(s);
				s->operand[0] = put_tref(src);
			}
			break;
		case TK_ATSIGN:
			s->opcode = OC_INDFNNAME;
			if (rval = indirection(&(s->operand[0])))
			{
				s->operand[1] = put_tref(src);
				ins_triple(s);
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
			*ind_source_sp++ = depth;
			comp_indr(&object);
		}
	} else
	{
		if (ind_source_sp + 1 >= ind_source_top || ind_result_sp + 1 >= ind_result_top)
			rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
		*ind_result_sp++ = dst;
		*ind_source_sp++ = depth;
		comp_indr(obj);
	}
}
