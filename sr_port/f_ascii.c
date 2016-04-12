/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "compiler.h"
#include "toktyp.h"
#include "advancewindow.h"
#include "op.h"
#include "opcode.h"
#include "gtm_utf8.h"

GBLREF boolean_t        gtm_utf8_mode;

int f_ascii(oprtype *a, opctype op)
{
	triple *r;
	mval tmp_mval;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
		return FALSE;
	if (TK_COMMA != TREF(window_token))
		r->operand[1] = put_ilit(1);
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(r->operand[1]), MUMPS_INT))
			return FALSE;
	}
	if ((OC_LIT == r->operand[0].oprval.tref->opcode)
		&& (OC_ILIT == r->operand[1].oprval.tref->opcode)
		&& (!gtm_utf8_mode || valid_utf_string(&r->operand[0].oprval.tref->operand[0].oprval.mlit->v.str)))
	{
		if (OC_FNASCII == r->opcode)
		{
			op_fnascii(r->operand[1].oprval.tref->operand[0].oprval.ilit,
				&r->operand[0].oprval.tref->operand[0].oprval.mlit->v, &tmp_mval);
		} else
		{
			op_fnzascii(r->operand[1].oprval.tref->operand[0].oprval.ilit,
				&r->operand[0].oprval.tref->operand[0].oprval.mlit->v, &tmp_mval);
		}
		*a = put_lit(&tmp_mval);
		a->oprval.tref->src = r->src;
		return TRUE;
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
