/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved. *
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "compiler.h"
#include "mmemory.h"
#include "opcode.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "op.h"
#include "gtm_utf8.h"
#include "xfer_enum.h"
#include "stringpool.h"

#if defined(__ia64)
	#define OP_FNEXTRACT op_fnextract2
#else
	#define OP_FNEXTRACT op_fnextract
#endif

GBLREF boolean_t        gtm_utf8_mode;

/* $EXTRACT, $ZEXTRACT, and $ZSUBSTR use this compiler routine as all have similar function and identical invocation signatures */
STATICDEF int f_extract(oprtype *a, opctype op)
{
	triple	*first, *last, *r;
	mval	tmp_mval;
	oprtype	*newop;

	static mstr scratch_space = {0, 0, 0};

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
		return FALSE;
	first = newtriple(OC_PARAMETER);
	last = newtriple(OC_PARAMETER);
	r->operand[1] = put_tref(first);
	first->operand[1] = put_tref(last);
	if (TK_COMMA != TREF(window_token))
	{
		first->operand[0] = put_ilit(1);
		last->operand[0] = put_ilit((OC_FNZSUBSTR == op) ? MAXPOSINT4 : 1);
	} else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(first->operand[0]), MUMPS_INT))
			return FALSE;
		if (TK_COMMA != TREF(window_token))
			last->operand[0] = (OC_FNZSUBSTR == op) ? put_ilit(MAXPOSINT4) : first->operand[0];
		else
		{
			advancewindow();
			if (EXPR_FAIL == expr(&(last->operand[0]), MUMPS_INT))
				return FALSE;
		}
	}
	/* This code tries to execute $EXTRACT at compile time if all parameters are literals */
	if ((OC_LIT == r->operand[0].oprval.tref->opcode)
		&& (OC_ILIT == first->operand[0].oprval.tref->opcode)
		&& (OC_ILIT == last->operand[0].oprval.tref->opcode)
		&& (!gtm_utf8_mode || valid_utf_string(&r->operand[0].oprval.tref->operand[0].oprval.mlit->v.str)))
	{	/* We don't know how much space we will use; but we know it will be <= the size of the current string */
		if (scratch_space.len < r->operand[0].oprval.tref->operand[0].oprval.mlit->v.str.len)
		{
			if (scratch_space.addr != 0)
				free(scratch_space.addr);
			scratch_space.addr = malloc(r->operand[0].oprval.tref->operand[0].oprval.mlit->v.str.len);
			scratch_space.len = r->operand[0].oprval.tref->operand[0].oprval.mlit->v.str.len;
		}
		tmp_mval.str.addr = scratch_space.addr;
		if ((OC_FNEXTRACT == op) || (OC_FNZEXTRACT == op))
		{
			OP_FNEXTRACT(last->operand[0].oprval.tref->operand[0].oprval.ilit,
				first->operand[0].oprval.tref->operand[0].oprval.ilit,
				&r->operand[0].oprval.tref->operand[0].oprval.mlit->v, &tmp_mval);
		} else
		{
			assert(OC_FNZSUBSTR == op);
			op_fnzsubstr(&r->operand[0].oprval.tref->operand[0].oprval.mlit->v,
				first->operand[0].oprval.tref->operand[0].oprval.ilit,
				last->operand[0].oprval.tref->operand[0].oprval.ilit, &tmp_mval);
		}
		s2pool(&tmp_mval.str);
		newop = (oprtype *)mcalloc(SIZEOF(oprtype));
		*newop = put_lit(&tmp_mval);				/* Copies mval so stack var tmp_mval not an issue */
		assert(TRIP_REF == newop->oprclass);
		newop->oprval.tref->src = r->src;
		*a = put_tref(newop->oprval.tref);
		return TRUE;
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
