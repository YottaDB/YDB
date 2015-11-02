/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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
#include "gtm_limits.h"
#include "hashtab_objcode.h"
#include "op.h"

UNIX_ONLY(GBLREF) VMS_ONLY(LITREF) int (*indir_fcn[])();

#define INDIR(a, b, c) c
static readonly opctype indir_opcode[] = {
#include "indir.h"
};
#undef INDIR

error_def(ERR_INDMAXNEST);

void op_indfun(mval *v, mint argcode, mval *dst)
{
	icode_str	indir_src;
	int		rval;
	mstr		*obj, object;
	oprtype		x;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (TREF(ind_result_sp) >= TREF(ind_result_top))
		rts_error(VARLSTCNT(1) ERR_INDMAXNEST); /* mdbcondition_handler resets ind_result_sp */
	assert((SIZEOF(indir_opcode)/SIZEOF(indir_opcode[0])) > argcode);
	assert(indir_opcode[argcode]);
	MV_FORCE_STR(v);
	indir_src.str = v->str;
	indir_src.code = argcode;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		obj = &object;
		comp_init(&v->str);
		rval = (*indir_fcn[argcode])(&x, indir_opcode[argcode]);
		if (EXPR_FAIL == comp_fini(rval, obj, OC_IRETMVAL, &x, v->str.len))
			return;
		indir_src.str.addr = v->str.addr;
		cache_put(&indir_src, obj);
		/* Fall into code activation below */
	}
	*(TREF(ind_result_sp))++ = dst;				/* Where to store return value */
	comp_indr(obj);
	return;
}
