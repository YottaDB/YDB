/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "op.h"

GBLREF char	window_token;

void	op_indlvadr(mval *target)
{
	error_def(ERR_VAREXPECTED);
	bool		rval;
	mstr		object, *obj;
	oprtype		v;
	triple		*s;

	MV_FORCE_STR(target);

	if (!(obj = cache_get(indir_lvadr, &target->str)))
	{
		comp_init(&target->str);
		switch (window_token)
		{
		case TK_IDENT:
			rval = lvn(&v, OC_PUTINDX, 0);
			if (comp_fini(rval, &object, OC_IRETMVAD, &v, target->str.len))
			{	cache_put(indir_lvadr, &target->str, &object);
				comp_indr(&object);
			}
			break;
		case TK_ATSIGN:
			if (rval = indirection(&v))
			{
				s = newtriple(OC_INDLVADR);
				s->operand[0] = v;
				v = put_tref(s);
				if (comp_fini(rval, &object, OC_IRETMVAD, &v, target->str.len))
				{	cache_put(indir_lvadr, &target->str, &object);
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
	{
		comp_indr(obj);
	}
}
