/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
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
#include "opcode.h"
#include "indir_enum.h"
#include "advancewindow.h"

error_def(ERR_LKNAMEXPECTED);

int nref(void)
{
	boolean_t	gbl;
	oprtype		tmparg;
	triple		*ref;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	gbl = FALSE;
	switch (TREF(window_token))
	{
	case TK_CIRCUMFLEX:
		gbl = TRUE;
		advancewindow();
		/* caution fall through */
	case TK_LBRACKET:
	case TK_IDENT:
	case TK_VBAR:
		return lkglvn(gbl);
	case TK_ATSIGN:
		if (!indirection(&tmparg))
			return EXPR_FAIL;
		ref = maketriple(OC_COMMARG);
		ref->operand[0] = tmparg;
		ref->operand[1] = put_ilit((mint) indir_nref);
		ins_triple(ref);
		return EXPR_INDR;
	default:
		stx_error(ERR_LKNAMEXPECTED);
		return EXPR_FAIL;
	}
}
