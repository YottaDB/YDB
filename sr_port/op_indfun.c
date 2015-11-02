/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
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
#include "hashtab_objcode.h"
#include "op.h"

UNIX_ONLY(GBLREF) VMS_ONLY(LITREF) int (*indir_fcn[])();
GBLREF mval 			**ind_result_sp, **ind_result_top;

#define INDIR(a, b, c) c
static readonly opctype indir_opcode[] = {
#include "indir.h"
};
#undef INDIR

void op_indfun(mval *v, mint argcode, mval *dst)
{
	bool		rval;
	mstr		*obj, object;
	oprtype		x;
	icode_str	indir_src;

	error_def(ERR_INDMAXNEST);

	assert((SIZEOF(indir_opcode)/SIZEOF(indir_opcode[0])) > argcode);
	assert(indir_opcode[argcode]);
	MV_FORCE_STR(v);
	indir_src.str = v->str;
	indir_src.code = argcode;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		comp_init(&v->str);
		rval = (*indir_fcn[argcode])(&x, indir_opcode[argcode]);
		if (!comp_fini(rval, &object, OC_IRETMVAL, &x, v->str.len))
			return;
		indir_src.str.addr = v->str.addr;
		cache_put(&indir_src, &object);
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
