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
#include "cache.h"
#include "op.h"

LITREF int (*indir_fcn[])();
GBLREF mval **ind_result_sp, **ind_result_top;

void op_indfun(mval *v,unsigned char argcode, mval *dst)
{

	static readonly opctype fun_opcode[] =
	{
		OC_FNDATA, OC_FNNEXT, OC_FNORDER, OC_FNGET, OC_FNZPREVIOUS, OC_FNQUERY, OC_FNZQGBLMOD
	};

	unsigned char funcode;
	mstr *obj, object;
	oprtype	x;
	bool rval;
	error_def(ERR_INDMAXNEST);

	assert(argcode < 4 || argcode == 40 || argcode == 41 || argcode == 51);
	switch(argcode)
	{	case 40:
		case 41:
			funcode = argcode - 36;
			break;
		case 51:
			funcode = 6;
		default:
			funcode = argcode;
			break;
	}
	MV_FORCE_STR(v);
	if (!(obj = cache_get(argcode, &v->str)))
	{
		comp_init(&v->str);
		rval = (*indir_fcn[argcode])(&x, fun_opcode[funcode]);
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
