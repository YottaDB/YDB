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

/* [Used by SET] Saves an indirect variable in the glvn pool and returns its index. Maintain in parallel with op_indsavlvn. */
void op_indsavglvn(mval *target, uint4 slot, uint4 do_ref)
{
	icode_str       indir_src;
	int             rval;
	mstr            *obj, object;
	oprtype         v, getdst;
	opctype		put_oc;
	triple          *s, *s1, *sub, *share;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = do_ref ? indir_savglvn1 : indir_savglvn0;		/* must differenitate the 2 code variants */
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
		case TK_CIRCUMFLEX:
			s1 = (TREF(curtchain))->exorder.bl;
			if (EXPR_FAIL != (rval = gvn()))			/* NOTE assignment */
			{
				for (sub = (TREF(curtchain))->exorder.bl; sub != s1; sub = sub->exorder.bl)
				{
					put_oc = sub->opcode;
					if ((OC_GVNAME == put_oc) || (OC_GVNAKED == put_oc) || (OC_GVEXTNAM == put_oc))
						break;
				}
				assert((OC_GVNAME == put_oc) || (OC_GVNAKED == put_oc) || (OC_GVEXTNAM == put_oc));
				if (!do_ref)
					sub->opcode = OC_SAVGVN;	/* steal gv bind action to suppress global reference */
				else
				{					/* or replicate it to cause bind to update $R before save */
					s = maketriple(OC_SAVGVN);
					s->operand[0] = sub->operand[0];
					s->operand[1] = sub->operand[1];
					dqins(sub, exorder, s);
				}
				share->operand[1] = put_ilit(put_oc);
				dqins(sub->exorder.bl, exorder, share);
			}
			break;
		case TK_ATSIGN:
			if (EXPR_FAIL != (rval = indirection(&v)))		/* NOTE assignment */
			{
				s = newtriple(OC_INDSAVGLVN);
				s->operand[0] = v;
				s1 = newtriple(OC_PARAMETER);
				s->operand[1] = put_tref(s1);
				s1->operand[0] = getdst;
				s1->operand[1] = put_ilit(do_ref);	/* pass along flag to control global reference */
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
