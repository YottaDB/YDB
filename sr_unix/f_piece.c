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
#include "gtm_utf8.h"

GBLREF boolean_t	gtm_utf8_mode;
GBLREF boolean_t	badchar_inhibit;

error_def(ERR_COMMA);

int f_piece(oprtype *a, opctype op)
{
	delimfmt	unichar;
	mval		*delim_mval;
	oprtype		x;
	triple		*delimiter, *first, *last, *r;
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
		 && (1 == ((gtm_utf8_mode && (OC_FNZPIECE != op))  ?   MV_FORCE_LEN(&x.oprval.tref->operand[0].oprval.mlit->v)
								   : x.oprval.tref->operand[0].oprval.mlit->v.str.len)))
	{	/* Potential shortcut to op_fnzp1 or op_fnp1. Make some further checks */
		delim_mval = &x.oprval.tref->operand[0].oprval.mlit->v;
		/* Both valid chars of char_len 1 and invalid chars of byte length 1 get the fast path */
		unichar.unichar_val = 0;
		if (!gtm_utf8_mode || OC_FNZPIECE == op)
		{       /* Single byte delimiter */
			r->opcode = OC_FNZP1;
			unichar.unibytes_val[0] = *delim_mval->str.addr;
		} else
		{       /* Potentially multiple bytes in one int */
			r->opcode = OC_FNP1;
			assert(SIZEOF(int) >= delim_mval->str.len);
			memcpy(unichar.unibytes_val, delim_mval->str.addr, delim_mval->str.len);
		}
		delimiter->operand[0] = put_ilit(unichar.unichar_val);
		ins_triple(r);
		*a = put_tref(r);
		return TRUE;
	}
	/* Fall into here if (1) have multi-char delimiter or (2) an invalid utf8 sequence of bytelen > 1
	 * This generates the longer form call to op_fnpiece/op_fnzpiece.
	 */
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
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
