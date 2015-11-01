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
#include "indir_enum.h"
#include "toktyp.h"
#include "cache.h"
#include "pattern.h"
#include "op.h"

GBLREF short int source_column;
GBLREF mval **ind_result_sp, **ind_result_top;
GBLREF char window_token;

void	op_indpat(mval *v, mval *dst)
{
	bool		rval;
	mstr		*obj, object;
	oprtype		x;
	error_def(ERR_INDMAXNEST);

	MV_FORCE_STR(v);
	if (!(obj = cache_get(indir_pattern, &v->str)))
	{
		comp_init(&v->str);
		source_column = 1;	/* to coordinate with scanner redirection*/
		rval = pattern(&x,window_token == TK_ATSIGN);
		if (comp_fini(rval, &object, OC_IRETMVAL, &x, v->str.len))
		{
			cache_put(indir_pattern, &v->str, &object);
			*ind_result_sp++ = dst;
			if (ind_result_sp >= ind_result_top)
				rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
			comp_indr(&object);
		}
	}
	else
	{
		*ind_result_sp++ = dst;
		if (ind_result_sp >= ind_result_top)
			rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
		comp_indr(obj);
	}
}
