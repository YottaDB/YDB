/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "lv_val.h"
#include "toktyp.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "cache.h"
#include "op.h"
#include <rtnhdr.h>
#include "valid_mname.h"
#include "gtm_string.h"
#include "cachectl.h"
#include "gtm_text_alloc.h"
#include "callg.h"
#include "mdq.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "mv_stent.h"
#include "min_max.h"
#include "glvn_pool.h"

error_def(ERR_VAREXPECTED);

/* [Used by FOR] Same as op_indsavglvn, but only allows local variables. Compare with op_indlvadr. */
void op_indsavlvn(mval *target, uint4 slot)
{
	icode_str       indir_src;
	int             rval;
	mstr            *obj, object;
	oprtype         v, getdst;
	triple          *s, *share;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_savlvn;
	if (NULL == (obj = cache_get(&indir_src)))
	{
		obj = &object;
		comp_init(&target->str, &getdst);
		share = maketriple(OC_SHARESLOT);
		share->operand[0] = getdst;
		switch (TREF(window_token))
		{
		case TK_IDENT:
			if (EXPR_FAIL != (rval = lvn(&v, OC_SAVLVN, NULL)))	/* NOTE assignment */
			{
				s = v.oprval.tref;
				if (OC_SAVLVN != s->opcode)
				{	/* No subscripts. Still, let's do savindx */
					s = newtriple(OC_SAVLVN);
					s->operand[0] = put_ilit(1);
					s->operand[1] = v;
				}
				share->operand[1] = put_ilit(OC_SAVLVN);
				dqins(s->exorder.bl, exorder, share);
			}
			break;
		case TK_ATSIGN:
			if (EXPR_FAIL != (rval = indirection(&v)))		/* NOTE assignment */
			{
				s = newtriple(OC_INDSAVLVN);
				s->operand[0] = v;
				s->operand[1] = getdst;
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			rval = EXPR_FAIL;
			break;
		}
		if (EXPR_FAIL == comp_fini(rval, obj, OC_RET, NULL, NULL, target->str.len))
			return;
		indir_src.str.addr = target->str.addr;
		cache_put(&indir_src, obj);
		assert(NULL != cache_get(&indir_src));
	}
	TREF(ind_result) = (mval *)(UINTPTR_T)slot;
	comp_indr(obj);
}
