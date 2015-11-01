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

#include <limits.h>

#include "compiler.h"
#include "opcode.h"
#include "cache.h"
#include "op.h"

LITREF int (*indir_fcn[])();
GBLREF mval **ind_result_sp, **ind_result_top;

#define INDIR(a, b, c) c
static readonly opctype indir_opcode[] = {
#include "indir.h"
};
#undef INDIR

void op_indfun(mval *v, mint code, mval *dst)
{
	bool		rval;
	mstr		*obj, object;
	oprtype		x;
	unsigned char	argcode;

	error_def(ERR_INDMAXNEST);

	argcode = (unsigned char)code;
	assert(UCHAR_MAX >= code); /* if not, the assignment to argcode is lossy */
	assert(indir_opcode[argcode]);
	MV_FORCE_STR(v);
	if (!(obj = cache_get(argcode, &v->str)))
	{
		comp_init(&v->str);
		rval = (*indir_fcn[argcode])(&x, indir_opcode[argcode]);
		if (!comp_fini(rval, &object, OC_IRETMVAL, &x, v->str.len))
			return;
		cache_put(argcode, &v->str, &object);
		*ind_result_sp++ = dst;
		if (ind_result_sp >= ind_result_top)
			rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
		comp_indr(&object);
		return;
	}
	*ind_result_sp++ = dst;
	if (ind_result_sp >= ind_result_top)
		rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
	comp_indr(obj);
	return;
}
