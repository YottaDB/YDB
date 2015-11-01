/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "cmd_qlf.h"
#include "advancewindow.h"
#include "gtm_caseconv.h"

static readonly mident zero_ident;

GBLREF char window_token;
GBLREF mident window_ident;
GBLREF bool run_time;
GBLREF char routine_name[];
GBLREF command_qualifier cmd_qlf;

int f_text( oprtype *a, opctype op )
{
	triple *r, *label;
	error_def(ERR_TEXTARG);
	error_def(ERR_RTNNAME);

	r = maketriple(op);
	switch (window_token)
	{
	case TK_CIRCUMFLEX:
	case TK_PLUS:
		r->operand[0] = put_str(&zero_ident.c[0],sizeof(mident));
		break;
	case TK_INTLIT:
		int_label();
		/* caution: fall through */
	case TK_IDENT:
		if (!(cmd_qlf.qlf & CQ_LOWER_LABELS))
			lower_to_upper((uchar_ptr_t)&window_ident.c[0],(uchar_ptr_t)&window_ident.c[0],sizeof(mident));

		r->operand[0] = put_str(&window_ident.c[0],sizeof(mident));
		advancewindow();
		break;
	case TK_ATSIGN:
		if (!indirection(&(r->operand[0])))
			return FALSE;
		r->opcode = OC_INDTEXT;
		break;
	default:
		stx_error(ERR_TEXTARG);
		return FALSE;
	}
	label = newtriple(OC_PARAMETER);
	r->operand[1] = put_tref(label);
	if (window_token != TK_PLUS)
	{
		if (window_token != TK_RPAREN && window_token != TK_CIRCUMFLEX)
		{
			stx_error(ERR_TEXTARG);
			return FALSE;
		}
		label->operand[0] = put_ilit(0);
	}
	else
	{
		advancewindow();
		if (!intexpr(&(label->operand[0])))
			return FALSE;
	}
	if (window_token != TK_CIRCUMFLEX)
	{	if (!run_time)
			label->operand[1] = put_str(routine_name, mid_len ((mident *)routine_name));
		else
			label->operand[1] = put_tref(newtriple(OC_CURRTN));
	}
	else
	{	advancewindow();
		switch(window_token)
		{
		case TK_IDENT:
			label->operand[1] = put_str(&window_ident.c[0], mid_len (&window_ident));
			advancewindow();
			break;
		case TK_ATSIGN:
			if (!indirection(&label->operand[1]))
				return FALSE;
			break;
		default:
			stx_error(ERR_RTNNAME);
			return FALSE;
		}
	}
	ins_triple(r);
	*a = put_tref(r);
	return TRUE;
}
