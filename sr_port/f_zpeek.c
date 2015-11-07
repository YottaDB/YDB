/****************************************************************
 *								*
 *	Copyright 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "compiler.h"
#include "opcode.h"
#include "toktyp.h"
#include "advancewindow.h"

error_def(ERR_COMMA);

#ifdef UNIX
/* Compile 4 parameter $ZPEEK(baseadr,offset,length<,format>) function where:
 *
 * structid - A string containing a set of mnemonics that identify the structure to fetch from (see op_fnzpeek.c)
 * offset   - Offset into the block (error if negative).
 * length   - Length to return (error if negative or > MAX_STRLEN).
 * format   - Option parm contains single char formatting code (see op_fnzpeek.c)
 */
int f_zpeek(oprtype *a, opctype op)
{
	oprtype		x;
	triple		*offset, *length, *format, *r;
	mval		mv;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r = maketriple(op);
	if (EXPR_FAIL == expr(&(r->operand[0]), MUMPS_STR))	/* Structure identifier string */
		return FALSE;
	if (TK_COMMA != TREF(window_token))
	{
		stx_error(ERR_COMMA);
		return FALSE;
	}
	advancewindow();
	offset = newtriple(OC_PARAMETER);
	r->operand[1] = put_tref(offset);
	if (EXPR_FAIL == expr(&(offset->operand[0]), MUMPS_INT))
		return FALSE;
	if (TK_COMMA != TREF(window_token))
	{
		stx_error(ERR_COMMA);
		return FALSE;
	}
	advancewindow();
	length = newtriple(OC_PARAMETER);
	offset->operand[1] = put_tref(length);
	if (EXPR_FAIL == expr(&(length->operand[0]), MUMPS_INT))
		return FALSE;
	format = newtriple(OC_PARAMETER);
	length->operand[1] = put_tref(format);
	if (TK_COMMA != TREF(window_token))
		format->operand[0] = put_str("C", 1);		/* Default format if none specified */
	else
	{
		advancewindow();
		if (EXPR_FAIL == expr(&(format->operand[0]), MUMPS_STR))
			return FALSE;
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
#else	/* VMS - function not supported here */
int f_zpeek(oprtype *a, opctype op)
{
	GTMASSERT;
}
#endif
