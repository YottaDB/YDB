/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "callg.h"
#include "lv_val.h"
#include "fnname.h"
#include "indr_fast_lvn.h"

error_def(ERR_VAREXPECTED);

void	op_indfnname(mval *dst, mval *target, mval *depth)
{
	boolean_t	gbl;
	icode_str	indir_src;
	int		rval;
	mstr		*obj, object;
	oprtype		v, getdst;
	triple		*s, *src;
	indr_fast_lvn	lvn;
	gparam_list	plist;
	int		i;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(target);
	indir_src.str = target->str;
	indir_src.code = indir_fnname;
	if (indr_fast_lvn_parse(target, &lvn))
	{	/* Fast indirection path, which avoids compiling the string for performance: it is a simple local variable
		 * reference, so drive op_fnname as the compiled code would, unless that needs more arguments than callg
		 * passes to it.
		 */
		if (MAX_ACTUALS >= (4 + lvn.nsubs))
		{
			plist.n = 4 + lvn.nsubs;
			plist.arg[0] = dst;
			plist.arg[1] = (void *)(INTPTR_T)FNLCL;
			plist.arg[2] = depth;
			plist.arg[3] = lvn.name;
			for (i = 0; i < lvn.nsubs; i++)
				plist.arg[4 + i] = lvn.subs[i];
			callg((callgfnptr)op_fnname, &plist);
			indr_fast_lvn_done(&lvn);
			return;
		}
		indr_fast_lvn_done(&lvn);
	}
	if (NULL == (obj = cache_get(&indir_src)))	/* NOTE assignment */
	{
		obj = &object;
		gbl = FALSE;
		comp_init(&target->str, &getdst);
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
				s->operand[0] = put_tref(src);
			break;
		case TK_ATSIGN:
			s->opcode = OC_INDFNNAME;
			if (EXPR_FAIL != (rval = indirection(&(s->operand[0]))))	/* NOTE assignment */
				s->operand[1] = put_tref(src);
			break;
		default:
			stx_error(ERR_VAREXPECTED);
			rval = EXPR_FAIL;
			break;
		}
		ins_triple(s);
		v = put_tref(s);
		if (EXPR_FAIL == comp_fini(rval, obj, OC_IRETMVAL, &v, &getdst, target->str.len))
			return;
		indir_src.str.addr = target->str.addr;
		cache_put(&indir_src, obj);
		/* Fall into code activation below */
	}
	TREF(ind_result) = dst;
	TREF(ind_source) = depth;
	comp_indr(obj);
	return;
}
