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
#include "op.h"
#include "valid_mname.h"

error_def(ERR_VAREXPECTED);

void	op_indlvarg(mval *v, mval *dst)
{
	icode_str	indir_src;
	int		rval;
	mstr		*obj, object;
	oprtype		x, getdst;
	triple		*ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(v);
	if (v->str.len < 1)
		rts_error(VARLSTCNT(1) ERR_VAREXPECTED);
	if (valid_mname(&v->str))
	{
		*dst = *v;
		dst->mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
		return;
	}
	if (*v->str.addr != '@')
		rts_error(VARLSTCNT(1) ERR_VAREXPECTED);
	indir_src.str = v->str;
	indir_src.code = indir_lvarg;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		obj = &object;
		obj->addr = v->str.addr;
		obj->len  = v->str.len;
		comp_init(obj, &getdst);
		if (EXPR_FAIL != (rval = indirection(&x)))	/* NOTE assignment */
		{
			ref = newtriple(OC_INDLVARG);
			ref->operand[0] = x;
			x = put_tref(ref);
		}
		if (EXPR_FAIL == comp_fini(rval, obj, OC_IRETMVAL, &x, &getdst, obj->len))
			return;
		indir_src.str.addr = v->str.addr;
		cache_put(&indir_src, obj);
		/* Fall into code activation below */
	}
	TREF(ind_result) = dst;					/* Where to store return value */
	comp_indr(obj);
	return;
}
