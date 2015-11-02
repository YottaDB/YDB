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

#include "toktyp.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "advancewindow.h"
#include "cache.h"
#include "hashtab_objcode.h"
#include "op.h"

error_def(ERR_INDMAXNEST);
error_def(ERR_VAREXPECTED);

void	op_indfnname(mval *dst, mval *target, mval *depth)
{
	boolean_t	gbl;
	icode_str	indir_src;
	int		rval;
	mstr		*obj, object;
	oprtype		v;
	triple		*s, *src;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if ((TREF(ind_source_sp) >= TREF(ind_source_top)) || (TREF(ind_result_sp) >= TREF(ind_result_top)))
		rts_error(VARLSTCNT(1) ERR_INDMAXNEST); /* mdbcondition_handler resets ind_result_sp & ind_source_sp */
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_fnname;
	if (NULL == (obj = cache_get(&indir_src)))	/* NOTE assignment */
	{
		obj = &object;
		gbl = FALSE;
		comp_init(&target->str);
		src = newtriple(OC_IGETSRC);
		s = maketriple(OC_FNNAME);
		switch (TREF(window_token))
		{
		case TK_CIRCUMFLEX:
			gbl = TRUE;
			advancewindow();
			/* caution fall through */
		case TK_IDENT:
			if (EXPR_FAIL != (rval = name_glvn(gbl, &s->operand[1])))	/* NOTE assignment */
			{
				ins_triple(s);
				s->operand[0] = put_tref(src);
			}
			break;
		case TK_ATSIGN:
			s->opcode = OC_INDFNNAME;
			if (EXPR_FAIL != (rval = indirection(&(s->operand[0]))))	/* NOTE assignment */
			{
				s->operand[1] = put_tref(src);
				ins_triple(s);
			}
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			rval = EXPR_FAIL;
			break;
		}
		v = put_tref(s);
		if (EXPR_FAIL == comp_fini(rval, obj, OC_IRETMVAL, &v, target->str.len))
			return;
		indir_src.str.addr = target->str.addr;
		cache_put(&indir_src, obj);
		/* Fall into code activation below */
	}
	*(TREF(ind_result_sp))++ = dst;
	*(TREF(ind_source_sp))++ = depth;
	comp_indr(obj);
	return;
}
