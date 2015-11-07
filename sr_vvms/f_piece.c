/****************************************************************
 *								*
 *	Copyright 2006, 2011 Fidelity Information Services, Inc	*
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
#include "advancewindow.h"
#include "fnpc.h"

error_def(ERR_COMMA);

int f_piece(oprtype *a, opctype op)
{
	delimfmt	unichar;
	mval		*delim_mval;
	oprtype		x;
	triple		*delimiter, *first, *last, *r, *srcislit;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))
		return FALSE;
	if (TK_COMMA != TREF(window_token))
	{
		stx_error(ERR_COMMA);
		return FALSE;
	}
	advancewindow();
	delimiter = newtriple(OC_PARAMETER);
	r->operand[1] = put_tref(delimiter);
	first = newtriple(OC_PARAMETER);
	delimiter->operand[1] = put_tref(first);
	if (EXPR_FAIL == expr(&x, MUMPS_STR))
		return FALSE;
	if (TK_COMMA != TREF(window_token))
		first->operand[0] = put_ilit(1);
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(first->operand[0]), MUMPS_INT))
			return FALSE;
	}
	assert(TRIP_REF == x.oprclass);
	if ((TK_COMMA != TREF(window_token)) && (OC_LIT == x.oprval.tref->opcode)
	     && (1 == x.oprval.tref->operand[0].oprval.mlit->v.str.len))
	{	/* Single char delimiter */
		delim_mval = &x.oprval.tref->operand[0].oprval.mlit->v;
		unichar.unichar_val = 0;
		r->opcode = OC_FNP1;
		unichar.unibytes_val[0] = *delim_mval->str.addr;
		delimiter->operand[0] = put_ilit(unichar.unichar_val);
		srcislit = newtriple(OC_PARAMETER);
		first->operand[1] = put_tref(srcislit);
	} else
	{
		delimiter->operand[0] = x;
		last = newtriple(OC_PARAMETER);
		first->operand[1] = put_tref(last);
		if (TK_COMMA != TREF(window_token))
			last->operand[0] = first->operand[0];
		else
		{
			advancewindow();
			if (EXPR_FAIL == expr(&(last->operand[0]), MUMPS_INT))
				return FALSE;
		}
		srcislit = newtriple(OC_PARAMETER);
		last->operand[1] = put_tref(srcislit);
	}
	/* Pass value 1 (TRUE) if src string is a literal, else 0 (FALSE) */
	srcislit->operand[0] = put_ilit((OC_LIT == r->operand[0].oprval.tref->opcode) ? TRUE : FALSE);
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
