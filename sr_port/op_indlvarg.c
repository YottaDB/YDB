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
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include "op.h"
#include "valid_mname.h"

GBLREF	mval			**ind_result_sp, **ind_result_top;

void	op_indlvarg(mval *v, mval *dst)
{
	bool		rval;
	mstr		*obj, object;
	oprtype		x;
	triple		*ref;
	icode_str	indir_src;

	error_def(ERR_INDMAXNEST);
	error_def(ERR_VAREXPECTED);

	MV_FORCE_STR(v);
	if (v->str.len < 1)
		rts_error(VARLSTCNT(1) ERR_VAREXPECTED);
	if (valid_mname(&v->str))
	{
		*dst = *v;
		dst->mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
		return;
	}
	if (*v->str.addr == '@')
	{
		indir_src.str = v->str;
		indir_src.code = indir_lvarg;
		if (NULL == (obj = cache_get(&indir_src)))
		{
			object.addr = v->str.addr;
			object.len  = v->str.len;
			comp_init(&object);
			if (rval = indirection(&x))
			{
				ref = newtriple(OC_INDLVARG);
				ref->operand[0] = x;
				x = put_tref(ref);
			}
			if (comp_fini(rval, &object, OC_IRETMVAL, &x, object.len))
			{
				indir_src.str.addr = v->str.addr;
				cache_put(&indir_src, &object);
				*ind_result_sp++ = dst;
				if (ind_result_sp >= ind_result_top)
					rts_error(VARLSTCNT(1) ERR_INDMAXNEST);
				comp_indr(&object);
				return;
			}
		} else
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
