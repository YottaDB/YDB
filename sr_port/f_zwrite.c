/****************************************************************
 *                                                              *
 * Copyright (c) 2012-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "mmemory.h"
#include "opcode.h"
#include "indir_enum.h"
#include "toktyp.h"
#include "mdq.h"
#include "fullbool.h"
#include "gtm_utf8.h"
#include "advancewindow.h"
#include "op.h"
#include "stringpool.h"

/* $ZWRITE(): Single parameter - string expression */
int f_zwrite(oprtype *a, opctype op)
{
	mval		tmp_mval;
	oprtype		*newop;
	triple		*r;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
		return FALSE;
	if (TK_COMMA != TREF(window_token))
		r->operand[1] = put_ilit(0);
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(r->operand[1]), MUMPS_INT))
			return FALSE;
	}
	/* This code tries to execute $ZWRITE at compile time if all parameters are literals */
	if ((OC_LIT == r->operand[0].oprval.tref->opcode) && (ILIT_REF == r->operand[1].oprval.tref->operand->oprclass)
		&& (!gtm_utf8_mode || valid_utf_string(&r->operand[0].oprval.tref->operand[0].oprval.mlit->v.str)))
	{	/* We don't know how much space we will use; but we know it's based on the size of the current string */
		op_fnzwrite(r->operand[1].oprval.tref->operand[0].oprval.ilit,
			&r->operand[0].oprval.tref->operand[0].oprval.mlit->v, &tmp_mval);
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
