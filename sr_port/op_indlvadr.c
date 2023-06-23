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
#include "gtm_string.h"
#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "indir_enum.h"
#include "cache.h"
#include "hashtab_mname.h"
#include "hashtab_objcode.h"
#include "op.h"
#include "valid_mname.h"
#include "stack_frame.h"
#include "stringpool.h"

GBLREF	boolean_t		mstr_native_align;
GBLREF	hash_table_objcode	cache_table;
GBLREF	stack_frame		*frame_pointer;

error_def(ERR_VAREXPECTED);

/* note: not currently used anywhere */
void	op_indlvadr(mval *target)
{
	char		*ptr;
	icode_str	indir_src;
	int		rval;
	mname_entry	*targ_key;
	mstr		*obj, object;
	mval		*saved_indx;
	oprtype		v;
	triple		*s;
	uint4		align_padlen, len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_lvadr;
	saved_indx = NULL;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		obj = &object;
		comp_init(&target->str, NULL);
		switch (TREF(window_token))
		{
		case TK_IDENT:
			rval = lvn(&v, OC_PUTINDX, NULL);
			break;
		case TK_ATSIGN:
			if (EXPR_FAIL != (rval = indirection(&v)))	/* NOTE assignment */
			{
				s = newtriple(OC_INDLVADR);
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
		cache_put(&indir_src, obj);
	}
	comp_indr(obj);
	return;
}
