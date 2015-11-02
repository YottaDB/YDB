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
#include "cache.h"
#include "hashtab_objcode.h"
#include "compile_pattern.h"
#include "op.h"

GBLREF short int 		source_column;

void	op_indpat(mval *v, mval *dst)
{
	int		rval;
	icode_str	indir_src;
	mstr		*obj, object;
	oprtype		x, getdst;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(v);
	indir_src.str = v->str;
	indir_src.code = indir_pattern;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		obj = &object;
		comp_init(&v->str, &getdst);
		source_column = 1;	/* to coordinate with scanner redirection*/
		rval = compile_pattern(&x, (TK_ATSIGN == TREF(window_token)));
		if (EXPR_FAIL == comp_fini(rval, obj, OC_IRETMVAL, &x, &getdst, v->str.len))
			return;
		indir_src.str.addr = v->str.addr;
		cache_put(&indir_src, obj);
		/* Fall into code activation below */
	}
	TREF(ind_result) = dst;					/* Where to store return value */
	comp_indr(obj);
	return;
}
