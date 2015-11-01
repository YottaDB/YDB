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
#include "indir_enum.h"
#include "toktyp.h"
#include "cmd_qlf.h"
#include "advancewindow.h"
#include "gtm_caseconv.h"

static readonly mstr zero_mstr;

GBLREF char window_token;
GBLREF mident window_ident;
GBLREF bool run_time;
GBLREF char routine_name[];
GBLREF command_qualifier cmd_qlf;

int f_text(oprtype *a, opctype op)
{
	int	implicit_offset = 0;
	triple	*r, *label;
	error_def(ERR_TEXTARG);
	error_def(ERR_RTNNAME);

	r = maketriple(op);
	switch (window_token)
	{
	case TK_CIRCUMFLEX:
		implicit_offset = 1;	/* CAUTION - fall-through */
	case TK_PLUS:
		r->operand[0] = put_str(zero_mstr.addr, 0);
		break;
	case TK_INTLIT:
		int_label();
		/* caution: fall through */
	case TK_IDENT:
		if (!(cmd_qlf.qlf & CQ_LOWER_LABELS))
			lower_to_upper((uchar_ptr_t)&window_ident.c[0], (uchar_ptr_t)window_ident.c, sizeof(mident));
		r->operand[0] = put_str(&window_ident.c[0], mid_len(&window_ident));
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
	assert(TK_PLUS == window_token || TK_CIRCUMFLEX == window_token || TK_RPAREN == window_token || TK_EOL == window_token);
	if (OC_INDTEXT != r->opcode || TK_PLUS == window_token || TK_CIRCUMFLEX == window_token)
	{
		label = newtriple(OC_PARAMETER);
		r->operand[1] = put_tref(label);
	}
	if (TK_PLUS != window_token)
	{
		if (OC_INDTEXT != r->opcode || TK_CIRCUMFLEX == window_token)
			label->operand[0] = put_ilit(implicit_offset);
		else
		{
			r->opcode = OC_INDFUN;
			r->operand[1] = put_ilit((mint)indir_fntext);
		}
	} else
	{
		advancewindow();
		if (!intexpr(&(label->operand[0])))
			return FALSE;
	}
	if (TK_CIRCUMFLEX != window_token)
	{
		if (OC_INDFUN != r->opcode)
		{
			if (!run_time)
				label->operand[1] = put_str(routine_name, mid_len((mident *)routine_name));
			else
				label->operand[1] = put_tref(newtriple(OC_CURRTN));
		}
	} else
	{
		advancewindow();
		switch(window_token)
		{
		case TK_IDENT:
			label->operand[1] = put_str(&window_ident.c[0], mid_len(&window_ident));
			advancewindow();
			break;
		case TK_ATSIGN:
			if (!indirection(&label->operand[1]))
				return FALSE;
			r->opcode = OC_INDTEXT;
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
