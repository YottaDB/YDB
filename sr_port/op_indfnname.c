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
#include "hashdef.h"
#include "toktyp.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "advancewindow.h"
#include "cache.h"
#include "op.h"

GBLREF char window_token;
GBLREF mval **ind_result_sp, **ind_result_top;

void	op_indfnname(mval *dst, mval *target, int value)
{
	error_def(ERR_INDMAXNEST);
	error_def(ERR_VAREXPECTED);
	bool		rval;
	mstr		object, *obj;
	oprtype		v;
	bool		gbl;
	triple		*s;

	MV_FORCE_STR(target);
	if (!(obj = cache_get(indir_fnname, &target->str)))
	{
		gbl = FALSE;
		comp_init(&target->str);
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
				s->operand[0] = put_ilit(value);
			}
			break;
		case TK_ATSIGN:
			s->opcode = OC_INDFNNAME;
			if (rval = indirection(&(s->operand[0])))
			{
				s->operand[1] = put_ilit(value);
				ins_triple(s);
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			break;
		}
		v = put_tref(s);
		if (comp_fini(rval, &object, OC_IRETMVAL, &v, target->str.len))
		{	cache_put(indir_get, &target->str, &object);
			if (ind_result_sp + 1 >= ind_result_top)
				rts_error(VARLSTCNT(1) ERR_INDMAXNEST);

			*ind_result_sp++ = dst;
			comp_indr(&object);
		}
	}
	else
	{
		if (ind_result_sp + 1 >= ind_result_top)
			rts_error(VARLSTCNT(1) ERR_INDMAXNEST);

		*ind_result_sp++ = dst;
		comp_indr(obj);
	}
}
