/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include "cache.h"
#include "op.h"

GBLREF	mval	**ind_result_sp, **ind_result_top;
LITREF	char	ctypetab[NUM_ASCII_CHARS];

void	op_indlvarg(mval *v, mval *dst)
{
	bool		rval;
	mstr		*obj, object;
	oprtype		x;
	triple		*ref;
	char		*c, *c_top;
	int4		y;

	error_def(ERR_INDMAXNEST);
	error_def(ERR_VAREXPECTED);

	MV_FORCE_STR(v);
	if (v->str.len < 1)
		rts_error(VARLSTCNT(1) ERR_VAREXPECTED);
	c = v->str.addr;
	c_top = c + v->str.len;
	if ((y = ctypetab[*c++]) == TK_UPPER || y == TK_LOWER || y == TK_PERCENT)
	{
		for ( ; c < c_top; c++)
		{	y = ctypetab[*c];
			if (y != TK_UPPER && y != TK_DIGIT && y != TK_LOWER)
			{	break;
			}
		}
		if (c == c_top)
		{	*dst = *v;
			return;
		}
	}
	if (*v->str.addr == '@')
	{
		if (!(obj = cache_get(indir_lvarg, &v->str)))
		{
			object.addr = v->str.addr;
			object.len  = v->str.len;
			comp_init(&object);
			if (rval = indirection(&x))
			{	ref = newtriple(OC_INDLVARG);
				ref->operand[0] = x;
				x = put_tref(ref);
			}
			if (comp_fini(rval, &object, OC_IRETMVAL, &x, object.len))
			{
				cache_put(indir_lvarg, &v->str, &object);
				*ind_result_sp++ = dst;
				if (ind_result_sp >= ind_result_top)
					rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
				comp_indr(&object);
				return;
			}
		}
		else
		{
			*ind_result_sp++ = dst;
			if (ind_result_sp >= ind_result_top)
				rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
			comp_indr(obj);
			return;
		}
	}
	rts_error(VARLSTCNT(1) ERR_VAREXPECTED);
}
