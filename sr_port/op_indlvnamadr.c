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
#include "toktyp.h"
#include "indir_enum.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include "op.h"

GBLREF char			window_token;
GBLREF mident			window_ident;

void	op_indlvnamadr(mval *target)
{
	error_def(ERR_VAREXPECTED);
	bool		rval;
	mstr		object, *obj;
	oprtype		v;
	triple		*s;
	icode_str	indir_src;

	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_lvnamadr;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		comp_init(&target->str);
		switch (window_token)
		{
		case TK_IDENT:
			rval = EXPR_GOOD;
			v = put_mvar(&window_ident);
			if (comp_fini(rval, &object, OC_IRETMVAD, &v, target->str.len))
			{
				indir_src.str.addr = target->str.addr;
				cache_put(&indir_src, &object);
				comp_indr(&object);
			}
			break;
		case TK_ATSIGN:
			if (rval = indirection(&v))
			{
				s = newtriple(OC_INDLVNAMADR);
				s->operand[0] = v;
				v = put_tref(s);
				if (comp_fini(rval, &object, OC_IRETMVAD, &v, target->str.len))
				{
					indir_src.str.addr = target->str.addr;
					cache_put(&indir_src, &object);
					comp_indr(&object);
				}
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			break;
		}
	}
	else
		comp_indr(obj);
}
