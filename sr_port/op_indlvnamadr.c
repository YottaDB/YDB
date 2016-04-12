/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "advancewindow.h"

error_def(ERR_VAREXPECTED);

void	op_indlvnamadr(mval *target)
{
	icode_str	indir_src;
	int		rval;
	mstr		*obj, object;
	oprtype		v;
	triple		*s;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_lvnamadr;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		obj = &object;
		comp_init(&target->str, NULL);
		switch (TREF(window_token))
		{
		case TK_IDENT:
			rval = EXPR_GOOD;
			v = put_mvar(&(TREF(window_ident)));
			advancewindow();
			break;
		case TK_ATSIGN:
			if (EXPR_FAIL != (rval = indirection(&v)))	/* NOTE assignment */
			{
				s = newtriple(OC_INDLVNAMADR);
				s->operand[0] = v;
				v = put_tref(s);
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			rval = EXPR_FAIL;
			break;
		}
		if (EXPR_FAIL == comp_fini(rval, obj, OC_IRETMVAD, &v, NULL, target->str.len))
			return;
		indir_src.str.addr = target->str.addr;
		cache_put(&indir_src, obj);
	}
	comp_indr(obj);
	return;
}
