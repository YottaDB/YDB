/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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

GBLREF char 	window_token;
GBLREF mident 	window_ident;
GBLREF command_qualifier cmd_qlf;
LITREF mident 	zero_ident;

int lref(oprtype *label,oprtype *offset,bool no_lab_ok,mint commarg_code,bool commarg_ok,bool *got_some)
{

	triple *ref;
	char c;
	error_def(ERR_LABELEXPECTED);

	switch(window_token)
	{
	case TK_INTLIT:
		int_label();
		/* caution: fall through */
	case TK_IDENT:
		*got_some = TRUE;
		if (!(cmd_qlf.qlf & CQ_LOWER_LABELS))
			lower_to_upper((uchar_ptr_t)window_ident.addr, (uchar_ptr_t)window_ident.addr, window_ident.len);
		*label = put_str(window_ident.addr, window_ident.len);
		advancewindow();
		break;
	case TK_ATSIGN:
		*got_some = TRUE;
		if (!indirection(label))
			return FALSE;
		if (commarg_ok && (c = window_token) != TK_COLON && c != TK_CIRCUMFLEX && c != TK_PLUS)
		{
			ref = newtriple(OC_COMMARG);
			ref->operand[0] = *label;
			ref->operand[1] = put_ilit(commarg_code);
			*label = put_tref(ref);
			return TRUE;
		}
		break;
	case TK_COLON:
	case TK_CIRCUMFLEX:
		return TRUE;
	case TK_PLUS:
		*label = put_str(zero_ident.addr, zero_ident.len);
		if (no_lab_ok)
			break;
		/* caution: fall through */
	default:
		stx_error(ERR_LABELEXPECTED);
		return FALSE;
	}
	if (window_token != TK_PLUS)
		return TRUE;
	*got_some = TRUE;
	advancewindow();
	return intexpr(offset);

}
