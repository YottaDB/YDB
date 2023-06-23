/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

GBLREF command_qualifier cmd_qlf;
LITREF mident 	zero_ident;

error_def(ERR_LABELEXPECTED);

int lref(oprtype *label, oprtype *offset, boolean_t no_lab_ok, mint commarg_code, boolean_t commarg_ok, boolean_t *got_some)
{
	char c;
	triple *ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch (TREF(window_token))
	{
	case TK_INTLIT:
		int_label();
		/* caution: fall through */
	case TK_IDENT:
		*got_some = TRUE;
		if (!(cmd_qlf.qlf & CQ_LOWER_LABELS))
			lower_to_upper((uchar_ptr_t)(TREF(window_ident)).addr, (uchar_ptr_t)(TREF(window_ident)).addr,
				(TREF(window_ident)).len);
		*label = put_str((TREF(window_ident)).addr, (TREF(window_ident)).len);
		advancewindow();
		break;
	case TK_ATSIGN:
		*got_some = TRUE;
		if (!indirection(label))
			return FALSE;
		if (commarg_ok
			&& (TK_COLON != (c = TREF(window_token))) && (TK_CIRCUMFLEX != c) && (TK_PLUS != c)) /* NOTE assignment */
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
	if (TK_PLUS != TREF(window_token))
		return TRUE;
	*got_some = TRUE;
	advancewindow();
	return expr(offset, MUMPS_INT);
}
