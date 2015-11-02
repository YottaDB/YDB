/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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

GBLREF char		window_token, director_token;
GBLREF mident		window_ident;
GBLREF boolean_t	run_time;
GBLREF mident		routine_name;
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
			implicit_offset = 1;
			/* CAUTION - fall-through */
		case TK_PLUS:
			r->operand[0] = put_str(zero_mstr.addr, 0);	/* Null label - top of routine */
			break;
		case TK_INTLIT:
			int_label();
			/* CAUTION - fall through */
		case TK_IDENT:
			if (!(cmd_qlf.qlf & CQ_LOWER_LABELS))
				lower_to_upper((uchar_ptr_t)window_ident.addr, (uchar_ptr_t)window_ident.addr, window_ident.len);
			r->operand[0] = put_str(window_ident.addr, window_ident.len);
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
	{	/* Need another parm chained in to deal with offset and routine name except for the case where an
		 * indirect specifies the entire argument.
		 */
		label = newtriple(OC_PARAMETER);
		r->operand[1] = put_tref(label);
	}
	if (TK_PLUS != window_token)
	{
		if (OC_INDTEXT != r->opcode || TK_CIRCUMFLEX == window_token)
			/* Set default offset (0 or 1 as computed above) when offset not specified */
			label->operand[0] = put_ilit(implicit_offset);
		else
		{	/* Fill in indirect text for case where indirect specifies entire operand */
			r->opcode = OC_INDFUN;
			r->operand[1] = put_ilit((mint)indir_fntext);
		}
	} else
	{	/* Process offset */
		advancewindow();
		if (!intexpr(&(label->operand[0])))
			return FALSE;
	}
	if (TK_CIRCUMFLEX != window_token)
	{	/* No routine specified - default to current routine */
		if (OC_INDFUN != r->opcode)
		{
			if (!run_time)
				label->operand[1] = put_str(routine_name.addr, routine_name.len);
			else
				label->operand[1] = put_tref(newtriple(OC_CURRTN));
		}
	} else
	{	/* Routine has been specified - pull it */
		advancewindow();
		switch(window_token)
		{
			case TK_IDENT:
#				ifdef GTM_TRIGGER
				if (TK_HASH == director_token)
					/* Coagulate tokens as necessary (and available) to allow '#' in the routine name */
					advwindw_hash_in_mname_allowed();
#				endif
				label->operand[1] = put_str(window_ident.addr, window_ident.len);
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
